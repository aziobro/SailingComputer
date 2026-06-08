#pragma once
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TRACK_TAG = "Track";

#define TRACK_LOOP_MAGIC 0xB0A7FACE

// 32-byte binary record stored in the loop file.
struct TrackPoint {
    int32_t  lat_e7;     // latitude  * 1e7
    int32_t  lon_e7;     // longitude * 1e7
    uint32_t unix_ts;    // UTC unix timestamp
    int16_t  hdt_x10;    // true heading * 10
    int16_t  heel_x10;   // roll/heel   * 10
    int16_t  sog_x100;   // SOG (kts)   * 100
    int16_t  cog_x10;    // COG         * 10
    uint8_t  fix;        // GGA fix quality
    uint8_t  pad[3];
};  // 32 bytes

// Fixed-size header at offset 0 of .loop.bin.
struct LoopHeader {
    uint32_t magic;
    uint32_t write_idx;   // next slot to write (0..max_points-1)
    uint32_t count;       // points currently stored (≤ max_points)
    uint32_t max_points;  // capacity (must match current config to be valid)
};  // 16 bytes

class TrackRecorder {
public:
    bool     loopRunning = false;   // user-controlled start/stop
    bool     segActive   = false;   // between Select Start and Select Stop
    uint32_t segStartTs  = 0;
    char     lastFile[80] = {};     // full path of last exported GPX
    bool     fileReady   = false;   // true after successful export

    SemaphoreHandle_t mtx = nullptr;

    // Call once after SD card mounts.  dir must be the tracks directory path,
    // e.g. "/sdcard/tracks".  Creates the directory and opens (or creates) the
    // loop file.  Returns false if SD is unavailable.
    bool begin(const char *dir, uint8_t intervalSec, uint8_t loopHours) {
        intervalSec_ = intervalSec ? intervalSec : 5;
        loopHours_   = loopHours   ? loopHours   : 3;
        maxPts_      = (uint32_t)loopHours_ * 3600u / intervalSec_;
        trackDir_    = dir;
        mtx          = xSemaphoreCreateMutex();

        mkdir(dir, 0755);  // no-op if already exists

        snprintf(loopPath_, sizeof(loopPath_), "%s/.loop.bin", dir);
        bool ok = openOrCreateLoop();
        if (ok) ESP_LOGI(TRACK_TAG, "begin ok  maxPts=%u interval=%us", maxPts_, intervalSec_);
        return ok;
    }

    // Called from the NMEA parse task for every GGA with fix>0.
    // Thread-safe; rate-limited internally.
    void tryWrite(double lat, double lon, float hdt, float heel,
                  float sog, float cog, uint8_t fix, uint32_t ts) {
        if (!loopRunning || fix == 0 || ts == 0) return;
        if (ts == lastWriteTs_ && lastWriteTs_ != 0) return;          // same second
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

    // Extract the segment [t0..t1] from the loop and write a GPX file.
    // Sets fileReady=true and lastFile on success.  Thread-safe.
    bool exportSegment(uint32_t t0, uint32_t t1) {
        if (!mtx) return false;
        xSemaphoreTake(mtx, portMAX_DELAY);

        char t0str[24], t1str[24];
        fmtTs(t0, t0str, sizeof(t0str));
        fmtTs(t1, t1str, sizeof(t1str));
        snprintf(lastFile, sizeof(lastFile), "%s/track_%s_to_%s.gpx",
                 trackDir_, t0str, t1str);
        fileReady = false;

        FILE *lp = fopen(loopPath_, "rb");
        if (!lp) { xSemaphoreGive(mtx); return false; }

        LoopHeader hdr = {};
        fread(&hdr, sizeof(hdr), 1, lp);
        if (hdr.magic != TRACK_LOOP_MAGIC || hdr.max_points == 0) {
            fclose(lp); xSemaphoreGive(mtx); return false;
        }

        FILE *out = fopen(lastFile, "w");
        if (!out) { fclose(lp); xSemaphoreGive(mtx); return false; }

        // GPX 1.1 header with SailingComputer extension namespace
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

        uint32_t stored  = hdr.count < hdr.max_points ? hdr.count : hdr.max_points;
        uint32_t oldest  = (hdr.count < hdr.max_points) ? 0 : (hdr.write_idx % hdr.max_points);
        int      written = 0;

        for (uint32_t i = 0; i < stored; i++) {
            uint32_t idx  = (oldest + i) % hdr.max_points;
            long     off  = (long)(sizeof(LoopHeader) + idx * sizeof(TrackPoint));
            fseek(lp, off, SEEK_SET);
            TrackPoint pt = {};
            fread(&pt, sizeof(pt), 1, lp);
            if (pt.unix_ts < t0 || pt.unix_ts > t1) continue;

            time_t   t   = (time_t)pt.unix_ts;
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
        fclose(lp);

        xSemaphoreGive(mtx);

        if (written > 0) {
            fileReady = true;
            ESP_LOGI(TRACK_TAG, "Exported %d pts → %s", written, lastFile);
            return true;
        }
        remove(lastFile);
        return false;
    }

    // Discard loop and re-create with new parameters.
    void reconfigure(uint8_t intervalSec, uint8_t loopHours) {
        if (mtx) xSemaphoreTake(mtx, portMAX_DELAY);
        if (loopFp_) { fclose(loopFp_); loopFp_ = nullptr; }
        intervalSec_ = intervalSec ? intervalSec : 5;
        loopHours_   = loopHours   ? loopHours   : 3;
        maxPts_      = (uint32_t)loopHours_ * 3600u / intervalSec_;
        writeIdx_    = 0;
        count_       = 0;
        firstTs_     = 0;
        lastTs_      = 0;
        lastWriteTs_ = 0;
        remove(loopPath_);
        openOrCreateLoop();
        if (mtx) xSemaphoreGive(mtx);
    }

    uint32_t loopFirstTs()  const { return firstTs_;  }
    uint32_t loopLastTs()   const { return lastTs_;   }
    uint32_t loopCount()    const { return count_;    }
    uint32_t loopMaxPts()   const { return maxPts_;   }
    uint8_t  intervalSec()  const { return intervalSec_; }
    uint8_t  loopHours()    const { return loopHours_;   }

    // Basename of lastFile for display.
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
    uint8_t     intervalSec_ = 5;
    uint8_t     loopHours_   = 3;
    char        loopPath_[80] = {};
    const char *trackDir_    = "/sdcard/tracks";

    bool openOrCreateLoop() {
        // Try to open and validate an existing loop file.
        loopFp_ = fopen(loopPath_, "r+b");
        if (loopFp_) {
            LoopHeader hdr = {};
            if (fread(&hdr, sizeof(hdr), 1, loopFp_) == 1 &&
                hdr.magic == TRACK_LOOP_MAGIC &&
                hdr.max_points == maxPts_) {
                writeIdx_ = hdr.write_idx;
                count_    = hdr.count < maxPts_ ? hdr.count : maxPts_;
                readBoundaryTs();
                ESP_LOGI(TRACK_TAG, "Loop re-opened: %u pts stored", count_);
                return true;
            }
            fclose(loopFp_);
            loopFp_ = nullptr;
            remove(loopPath_);  // stale / wrong size — start fresh
        }

        loopFp_ = fopen(loopPath_, "w+b");
        if (!loopFp_) {
            ESP_LOGE(TRACK_TAG, "Cannot create %s", loopPath_);
            return false;
        }

        LoopHeader hdr = { TRACK_LOOP_MAGIC, 0, 0, maxPts_ };
        fwrite(&hdr, sizeof(hdr), 1, loopFp_);

        // Pre-allocate data region in 512-byte chunks to avoid thousands of
        // tiny writes — faster on FATFS.
        static const uint8_t zeroBuf[512] = {};
        uint32_t totalBytes = maxPts_ * sizeof(TrackPoint);
        for (uint32_t w = 0; w < totalBytes; w += sizeof(zeroBuf)) {
            size_t chunk = sizeof(zeroBuf);
            if (w + chunk > totalBytes) chunk = totalBytes - w;
            fwrite(zeroBuf, 1, chunk, loopFp_);
        }
        fflush(loopFp_);
        writeIdx_ = 0;
        count_    = 0;
        ESP_LOGI(TRACK_TAG, "Loop created: %u pts max (%u KB)",
                 maxPts_, (uint32_t)(sizeof(LoopHeader) + maxPts_ * sizeof(TrackPoint)) / 1024);
        return true;
    }

    void writePointToLoop(const TrackPoint &pt) {
        if (!loopFp_) return;
        long off = (long)(sizeof(LoopHeader) + writeIdx_ * sizeof(TrackPoint));
        fseek(loopFp_, off, SEEK_SET);
        fwrite(&pt, sizeof(pt), 1, loopFp_);

        writeIdx_ = (writeIdx_ + 1) % maxPts_;
        if (count_ < maxPts_) count_++;
        lastTs_ = pt.unix_ts;
        if (firstTs_ == 0) firstTs_ = pt.unix_ts;

        // Persist header so the file is always self-consistent.
        fseek(loopFp_, 0, SEEK_SET);
        LoopHeader hdr = { TRACK_LOOP_MAGIC, writeIdx_, count_, maxPts_ };
        fwrite(&hdr, sizeof(hdr), 1, loopFp_);
        fflush(loopFp_);

        // Once the buffer wraps, the oldest entry is the slot we just vacated.
        if (count_ == maxPts_) {
            long oldest_off = (long)(sizeof(LoopHeader) + writeIdx_ * sizeof(TrackPoint));
            fseek(loopFp_, oldest_off, SEEK_SET);
            TrackPoint oldest = {};
            fread(&oldest, sizeof(oldest), 1, loopFp_);
            firstTs_ = oldest.unix_ts;
        }
    }

    void readBoundaryTs() {
        if (!loopFp_ || count_ == 0) return;
        uint32_t oldest = (count_ < maxPts_) ? 0 : (writeIdx_ % maxPts_);
        uint32_t newest = (writeIdx_ == 0) ? (maxPts_ - 1) : (writeIdx_ - 1);
        TrackPoint pt = {};
        fseek(loopFp_, (long)(sizeof(LoopHeader) + oldest * sizeof(TrackPoint)), SEEK_SET);
        fread(&pt, sizeof(pt), 1, loopFp_);
        firstTs_ = pt.unix_ts;
        fseek(loopFp_, (long)(sizeof(LoopHeader) + newest * sizeof(TrackPoint)), SEEK_SET);
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
