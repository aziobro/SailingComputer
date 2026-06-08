#pragma once
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TRACK_TAG = "Track";

#define TRACK_LOOP_MAGIC   0xB0A7FACE
#define TRACK_SYNC_WRITES  60u   // close+reopen loop file every N writes to flush FAT dir entry

// 32-byte binary record stored in the loop file.
struct TrackPoint {
    int32_t  lat_e7;     // latitude  * 1e7
    int32_t  lon_e7;     // longitude * 1e7
    uint32_t unix_ts;    // UTC unix timestamp (0 = empty slot)
    int16_t  hdt_x10;    // true heading * 10
    int16_t  heel_x10;   // roll/heel   * 10
    int16_t  sog_x100;   // SOG (kts)   * 100
    int16_t  cog_x10;    // COG         * 10
    uint8_t  fix;        // GGA fix quality
    uint8_t  pad[3];
};  // 32 bytes — sizeof must stay 32

struct LoopHeader {
    uint32_t magic;
    uint32_t write_idx;   // next slot to write (0..max_points-1)
    uint32_t count;       // points currently stored (≤ max_points)
    uint32_t max_points;  // capacity (must match current config)
};  // 16 bytes

// Design notes
// ============
// exportSegment() reads from the existing open loopFp_ handle rather than
// opening a second handle to the same file.  On FATFS, a second read handle
// opened while a write handle is active sees the directory-entry file_size,
// which may be 0 until the write handle is closed — causing fread to return
// nothing and the export to silently fail.
//
// After the initial pre-allocation and every TRACK_SYNC_WRITES point writes
// the file is closed and reopened.  This forces FATFS to commit the
// directory-entry file_size to disk so the file browser shows the real size.
//
// Power-loss resilience: a corrupt/partial header is recovered by scanning
// data slots for the first empty slot rather than deleting the file.

class TrackRecorder {
public:
    bool     sdAvailable = false;
    bool     loopRunning = false;
    bool     segActive   = false;
    uint32_t segStartTs  = 0;
    char     lastFile[80] = {};
    bool     fileReady   = false;

    SemaphoreHandle_t mtx = nullptr;

    bool begin(const char *dir, uint8_t intervalSec, uint8_t loopHours) {
        intervalSec_ = intervalSec ? intervalSec : 5;
        loopHours_   = loopHours   ? loopHours   : 3;
        maxPts_      = (uint32_t)loopHours_ * 3600u / intervalSec_;
        trackDir_    = dir;
        mtx          = xSemaphoreCreateMutex();
        mkdir(dir, 0755);
        snprintf(loopPath_, sizeof(loopPath_), "%s/.loop.bin", dir);
        sdAvailable = openOrCreateLoop();
        if (sdAvailable)
            ESP_LOGI(TRACK_TAG, "begin ok  maxPts=%u interval=%us", maxPts_, intervalSec_);
        return sdAvailable;
    }

    void tryWrite(double lat, double lon, float hdt, float heel,
                  float sog, float cog, uint8_t fix, uint32_t ts) {
        if (!sdAvailable || !loopRunning || fix == 0 || ts == 0) return;
        if (ts == lastWriteTs_ && lastWriteTs_ != 0) return;
        if (ts - lastWriteTs_ < intervalSec_ && lastWriteTs_ != 0) return;
        lastWriteTs_ = ts;

        TrackPoint pt = {};
        pt.lat_e7   = (int32_t)(lat  * 1e7);
        pt.lon_e7   = (int32_t)(lon  * 1e7);
        pt.unix_ts  = ts;
        pt.hdt_x10  = (int16_t)(hdt  * 10.0f);
        pt.heel_x10 = (int16_t)(heel * 10.0f);
        pt.sog_x100 = (int16_t)(sog  * 100.0f);
        pt.cog_x10  = (int16_t)(cog  * 10.0f);
        pt.fix      = fix;

        if (!mtx || xSemaphoreTake(mtx, pdMS_TO_TICKS(50)) != pdTRUE) return;
        writePointToLoop(pt);
        xSemaphoreGive(mtx);
    }

    bool exportSegment(uint32_t t0, uint32_t t1) {
        if (!mtx) return false;
        xSemaphoreTake(mtx, portMAX_DELAY);

        if (!loopFp_ || count_ == 0) { xSemaphoreGive(mtx); return false; }

        char t0str[24], t1str[24];
        fmtTs(t0, t0str, sizeof(t0str));
        fmtTs(t1, t1str, sizeof(t1str));
        snprintf(lastFile, sizeof(lastFile), "%s/track_%s_to_%s.gpx",
                 trackDir_, t0str, t1str);
        fileReady = false;

        char tmpPath[88];
        snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", lastFile);
        remove(tmpPath);

        // Read the loop header from the already-open write handle.
        // Opening a second read handle via fopen("rb") sees the FAT directory-
        // entry file_size which can be 0 while the write handle is open,
        // causing all reads to return nothing.
        fseek(loopFp_, 0, SEEK_SET);
        LoopHeader hdr = {};
        bool hdrOk = (fread(&hdr, sizeof(hdr), 1, loopFp_) == 1 &&
                      hdr.magic == TRACK_LOOP_MAGIC &&
                      hdr.max_points > 0);
        if (!hdrOk) {
            // Fall back to in-memory state — header may not have been flushed yet
            hdr.magic      = TRACK_LOOP_MAGIC;
            hdr.write_idx  = writeIdx_;
            hdr.count      = count_;
            hdr.max_points = maxPts_;
        }

        FILE *out = fopen(tmpPath, "w");
        if (!out) { xSemaphoreGive(mtx); return false; }

        char trkName[48];
        snprintf(trkName, sizeof(trkName), "%s_to_%s", t0str, t1str);
        fprintf(out,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<gpx version=\"1.1\" creator=\"SailingComputer\"\n"
            "     xmlns=\"http://www.topografix.com/GPX/1/1\"\n"
            "     xmlns:sc=\"http://sailingcomputer.local/gpx/1/0\">\n"
            "  <trk>\n"
            "    <name>%s</name>\n"
            "    <trkseg>\n", trkName);

        uint32_t stored = hdr.count < hdr.max_points ? hdr.count : hdr.max_points;
        uint32_t oldest = (hdr.count < hdr.max_points) ? 0u : (hdr.write_idx % hdr.max_points);
        int      written = 0;

        for (uint32_t i = 0; i < stored; i++) {
            uint32_t idx = (oldest + i) % hdr.max_points;
            long     off = (long)(sizeof(LoopHeader) + idx * (long)sizeof(TrackPoint));
            fseek(loopFp_, off, SEEK_SET);
            TrackPoint pt = {};
            if (fread(&pt, sizeof(pt), 1, loopFp_) != 1) continue;
            if (pt.unix_ts == 0 || pt.unix_ts < t0 || pt.unix_ts > t1) continue;

            time_t t = (time_t)pt.unix_ts;
            struct tm tm_ = {};
            gmtime_r(&t, &tm_);
            char iso[24];
            strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &tm_);

            fprintf(out,
                "      <trkpt lat=\"%.7f\" lon=\"%.7f\">\n"
                "        <time>%s</time>\n"
                "        <extensions>\n"
                "          <sc:hdt>%.1f</sc:hdt>\n"
                "          <sc:heel>%.1f</sc:heel>\n"
                "          <sc:sog>%.2f</sc:sog>\n"
                "          <sc:cog>%.1f</sc:cog>\n"
                "        </extensions>\n"
                "      </trkpt>\n",
                (double)pt.lat_e7 / 1e7,
                (double)pt.lon_e7 / 1e7,
                iso,
                pt.hdt_x10  / 10.0f,
                pt.heel_x10 / 10.0f,
                pt.sog_x100 / 100.0f,
                pt.cog_x10  / 10.0f);
            written++;
        }

        fprintf(out, "    </trkseg>\n  </trk>\n</gpx>\n");
        fclose(out);

        xSemaphoreGive(mtx);

        if (written > 0) {
            remove(lastFile);
            rename(tmpPath, lastFile);
            fileReady = true;
            ESP_LOGI(TRACK_TAG, "Exported %d pts → %s", written, lastFile);
            return true;
        }
        remove(tmpPath);
        return false;
    }

    void reconfigure(uint8_t intervalSec, uint8_t loopHours) {
        if (mtx) xSemaphoreTake(mtx, portMAX_DELAY);
        if (loopFp_) { fclose(loopFp_); loopFp_ = nullptr; }
        intervalSec_ = intervalSec ? intervalSec : 5;
        loopHours_   = loopHours   ? loopHours   : 3;
        maxPts_      = (uint32_t)loopHours_ * 3600u / intervalSec_;
        writeIdx_    = 0;  count_    = 0;
        firstTs_     = 0;  lastTs_   = 0;
        lastWriteTs_ = 0;  syncCtr_  = 0;
        remove(loopPath_);
        sdAvailable = openOrCreateLoop();
        if (mtx) xSemaphoreGive(mtx);
    }

    uint32_t loopFirstTs()  const { return firstTs_;  }
    uint32_t loopLastTs()   const { return lastTs_;   }
    uint32_t loopCount()    const { return count_;    }
    uint32_t loopMaxPts()   const { return maxPts_;   }
    uint8_t  intervalSec()  const { return intervalSec_; }
    uint8_t  loopHours()    const { return loopHours_;   }

    const char *lastFileName() const {
        const char *p = strrchr(lastFile, '/');
        return p ? p + 1 : lastFile;
    }

private:
    FILE       *loopFp_      = nullptr;
    uint32_t    writeIdx_    = 0;
    uint32_t    count_       = 0;
    uint32_t    maxPts_      = 2160;
    uint32_t    firstTs_     = 0;
    uint32_t    lastTs_      = 0;
    uint32_t    lastWriteTs_ = 0;
    uint32_t    syncCtr_     = 0;   // counts writes since last close+reopen
    uint8_t     intervalSec_ = 5;
    uint8_t     loopHours_   = 3;
    char        loopPath_[80] = {};
    const char *trackDir_    = "/sdcard/tracks";

    bool openOrCreateLoop() {
        loopFp_ = fopen(loopPath_, "r+b");
        if (loopFp_) {
            LoopHeader hdr = {};
            bool hdrOk = (fread(&hdr, sizeof(hdr), 1, loopFp_) == 1 &&
                          hdr.magic == TRACK_LOOP_MAGIC &&
                          hdr.max_points == maxPts_);
            if (hdrOk) {
                writeIdx_ = hdr.write_idx;
                count_    = hdr.count < maxPts_ ? hdr.count : maxPts_;
                syncCtr_  = 0;
                readBoundaryTs();
                ESP_LOGI(TRACK_TAG, "Loop re-opened: %u pts stored", count_);
                return true;
            }
            if (recoverLoopState()) {
                ESP_LOGW(TRACK_TAG, "Loop header recovered: count=%u write_idx=%u",
                         count_, writeIdx_);
                return true;
            }
            fclose(loopFp_);
            loopFp_ = nullptr;
            remove(loopPath_);
            ESP_LOGW(TRACK_TAG, "Loop recovery failed — recreating");
        }

        loopFp_ = fopen(loopPath_, "w+b");
        if (!loopFp_) {
            ESP_LOGE(TRACK_TAG, "Cannot create %s", loopPath_);
            return false;
        }

        LoopHeader hdr = { TRACK_LOOP_MAGIC, 0, 0, maxPts_ };
        fwrite(&hdr, sizeof(hdr), 1, loopFp_);

        static const uint8_t zeroBuf[512] = {};
        uint32_t totalBytes = maxPts_ * sizeof(TrackPoint);
        for (uint32_t w = 0; w < totalBytes; w += sizeof(zeroBuf)) {
            size_t chunk = sizeof(zeroBuf);
            if (w + chunk > totalBytes) chunk = totalBytes - w;
            fwrite(zeroBuf, 1, chunk, loopFp_);
        }
        fflush(loopFp_);

        // Close and reopen to force FATFS to commit the directory-entry
        // file_size field.  Without this the file appears as 0 bytes to
        // other file handles and to the file browser.
        fclose(loopFp_);
        loopFp_ = fopen(loopPath_, "r+b");
        if (!loopFp_) {
            ESP_LOGE(TRACK_TAG, "Cannot reopen loop after creation: %s", loopPath_);
            return false;
        }

        writeIdx_ = 0;
        count_    = 0;
        syncCtr_  = 0;
        ESP_LOGI(TRACK_TAG, "Loop created: %u pts max (%u KB)",
                 maxPts_, (uint32_t)(sizeof(LoopHeader) + maxPts_ * sizeof(TrackPoint)) / 1024);
        return true;
    }

    bool recoverLoopState() {
        fseek(loopFp_, 0, SEEK_END);
        long fileSize    = ftell(loopFp_);
        long expectedMin = (long)sizeof(LoopHeader) + (long)maxPts_ * (long)sizeof(TrackPoint);
        if (fileSize < expectedMin) {
            ESP_LOGW(TRACK_TAG, "Loop file too small (%ld < %ld)", fileSize, expectedMin);
            return false;
        }
        ESP_LOGW(TRACK_TAG, "Scanning %u slots to recover loop state…", maxPts_);
        for (uint32_t i = 0; i < maxPts_; i++) {
            fseek(loopFp_, (long)(sizeof(LoopHeader) + i * (long)sizeof(TrackPoint)), SEEK_SET);
            TrackPoint pt = {};
            if (fread(&pt, sizeof(pt), 1, loopFp_) != 1) return false;
            if (pt.unix_ts == 0) {
                writeIdx_ = i;
                count_    = i;
                syncCtr_  = 0;
                rewriteHeader();
                readBoundaryTs();
                return true;
            }
        }
        writeIdx_ = 0;
        count_    = maxPts_;
        syncCtr_  = 0;
        rewriteHeader();
        readBoundaryTs();
        return true;
    }

    void rewriteHeader() {
        fseek(loopFp_, 0, SEEK_SET);
        LoopHeader hdr = { TRACK_LOOP_MAGIC, writeIdx_, count_, maxPts_ };
        fwrite(&hdr, sizeof(hdr), 1, loopFp_);
        fflush(loopFp_);
    }

    // Periodically close and reopen the loop file so FATFS commits the
    // directory-entry file_size visible to external readers (file browser, etc.)
    void syncLoopFile() {
        if (!loopFp_) return;
        fflush(loopFp_);
        fclose(loopFp_);
        loopFp_ = fopen(loopPath_, "r+b");
        if (!loopFp_) {
            ESP_LOGE(TRACK_TAG, "Loop reopen after sync failed");
            sdAvailable = false;
        }
    }

    void writePointToLoop(const TrackPoint &pt) {
        if (!loopFp_) return;
        long off = (long)(sizeof(LoopHeader) + writeIdx_ * (long)sizeof(TrackPoint));
        fseek(loopFp_, off, SEEK_SET);
        if (fwrite(&pt, sizeof(pt), 1, loopFp_) != 1) {
            ESP_LOGE(TRACK_TAG, "Loop write failed at idx=%u", writeIdx_);
            return;
        }

        writeIdx_ = (writeIdx_ + 1) % maxPts_;
        if (count_ < maxPts_) count_++;
        lastTs_ = pt.unix_ts;
        if (firstTs_ == 0) firstTs_ = pt.unix_ts;

        if (count_ == maxPts_) {
            long oldest_off = (long)(sizeof(LoopHeader) + writeIdx_ * (long)sizeof(TrackPoint));
            fseek(loopFp_, oldest_off, SEEK_SET);
            TrackPoint oldest = {};
            if (fread(&oldest, sizeof(oldest), 1, loopFp_) == 1 && oldest.unix_ts)
                firstTs_ = oldest.unix_ts;
        }

        rewriteHeader();

        // Periodically close+reopen to keep the FAT directory entry current.
        if (++syncCtr_ >= TRACK_SYNC_WRITES) {
            syncCtr_ = 0;
            syncLoopFile();
        }
    }

    void readBoundaryTs() {
        if (!loopFp_ || count_ == 0) return;
        uint32_t oldest = (count_ < maxPts_) ? 0u : (writeIdx_ % maxPts_);
        uint32_t newest = (writeIdx_ == 0)   ? (maxPts_ - 1) : (writeIdx_ - 1);
        TrackPoint pt = {};
        fseek(loopFp_, (long)(sizeof(LoopHeader) + oldest * (long)sizeof(TrackPoint)), SEEK_SET);
        fread(&pt, sizeof(pt), 1, loopFp_);
        firstTs_ = pt.unix_ts;
        fseek(loopFp_, (long)(sizeof(LoopHeader) + newest * (long)sizeof(TrackPoint)), SEEK_SET);
        fread(&pt, sizeof(pt), 1, loopFp_);
        lastTs_ = pt.unix_ts;
    }

    void fmtTs(uint32_t ts, char *buf, size_t len) const {
        time_t t = (time_t)ts;
        struct tm tm_ = {};
        gmtime_r(&t, &tm_);
        strftime(buf, len, "%Y%m%d_%H%M%Sz", &tm_);
    }
};
