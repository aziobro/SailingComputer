#pragma once
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "cJSON.h"

#define STORAGE_TAG    "Storage"
#define SD_MOUNT_POINT "/sdcard"
#define SPIFFS_MOUNT_POINT "/spiffs"
#define MAX_MARKS        32
#define MAX_COURSES      64
#define MAX_COURSE_MARKS 12

struct Mark {
    char     id[16];
    char     name[32];
    double   lat;
    double   lon;
    uint32_t created;   // seconds since boot (no RTC — GPS time used when available)
};

struct CourseMarkRef {
    char mark_id[16];
    bool port_rounding; // true = port, false = starboard
};

struct Course {
    char          id[16];
    char          name[32];
    char          start_pin[16];
    char          start_committee[16];
    CourseMarkRef marks[MAX_COURSE_MARKS];
    int           mark_count;
    char          finish_pin[16];
    char          finish_committee[16];
};

class StorageManager {
public:
    bool mounted = false;

    bool begin() {
        if (mountSdCard() || mountSpiffs()) {
            bool ok = ensureFile(marksPath(), "[]");
            ok = ensureFile(coursesPath(), "[]") && ok;
            if (backend == Backend::SdCard) logRootFiles();
            return ok;
        }
        ESP_LOGE(STORAGE_TAG, "No storage backend available");
        return false;
    }

    const char *backendName() const {
        if (backend == Backend::SdCard) return "sdcard";
        if (backend == Backend::Spiffs) return "spiffs";
        return "none";
    }

    // ── Filesystem info ──────────────────────────────────────────────────────
    bool getInfo(uint64_t *total, uint64_t *used) {
        if (!mounted || !total || !used) return false;
        if (backend == Backend::Spiffs) {
            size_t spiffsTotal = 0, spiffsUsed = 0;
            esp_err_t ret = esp_spiffs_info("storage", &spiffsTotal, &spiffsUsed);
            *total = spiffsTotal;
            *used = spiffsUsed;
            return ret == ESP_OK;
        }

        uint64_t totalBytes = 0, freeBytes = 0;
        if (esp_vfs_fat_info(SD_MOUNT_POINT, &totalBytes, &freeBytes) != ESP_OK)
            return false;
        *total = totalBytes;
        *used = totalBytes - freeBytes;
        return true;
    }

    bool isSdCard() const {
        return backend == Backend::SdCard;
    }

    // ── Marks ────────────────────────────────────────────────────────────────
    int loadMarks(Mark *marks, int max_count) {
        char *buf = readFile(marksPath());
        if (!buf) return 0;
        int count = parseMarks(buf, marks, max_count);
        free(buf);
        return count;
    }

    bool saveMark(const Mark &m) {
        Mark marks[MAX_MARKS];
        int count = loadMarks(marks, MAX_MARKS);
        bool found = false;
        for (int i = 0; i < count; i++) {
            if (strcmp(marks[i].id, m.id) == 0) { marks[i] = m; found = true; break; }
        }
        if (!found) {
            if (count >= MAX_MARKS) return false;
            marks[count++] = m;
        }
        return writeMarks(marks, count);
    }

    bool deleteMark(const char *id) {
        Mark marks[MAX_MARKS];
        int count = loadMarks(marks, MAX_MARKS);
        int n = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(marks[i].id, id) != 0) marks[n++] = marks[i];
        }
        if (n == count) return false;
        return writeMarks(marks, n);
    }

    void generateMarkId(char *out, size_t len) {
        // Hex timestamp in ms — unique within a boot session
        snprintf(out, len, "m%06lx", (unsigned long)(esp_timer_get_time() / 1000ULL) & 0xFFFFFF);
    }

    // ── Courses ──────────────────────────────────────────────────────────────
    int loadCourses(Course *courses, int max_count) {
        char *buf = readFile(coursesPath());
        if (!buf) return 0;
        int count = parseCourses(buf, courses, max_count);
        free(buf);
        return count;
    }

    bool saveCourse(const Course &c) {
        Course *courses = (Course *)malloc(MAX_COURSES * sizeof(Course));
        if (!courses) return false;
        int count = loadCourses(courses, MAX_COURSES);
        bool found = false;
        for (int i = 0; i < count; i++) {
            if (strcmp(courses[i].id, c.id) == 0) { courses[i] = c; found = true; break; }
        }
        if (!found) {
            if (count >= MAX_COURSES) { free(courses); return false; }
            courses[count++] = c;
        }
        bool ok = writeCourses(courses, count);
        free(courses);
        return ok;
    }

    bool deleteCourse(const char *id) {
        Course *courses = (Course *)malloc(MAX_COURSES * sizeof(Course));
        if (!courses) return false;
        int count = loadCourses(courses, MAX_COURSES);
        int n = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(courses[i].id, id) != 0) courses[n++] = courses[i];
        }
        bool changed = (n != count);
        bool ok = changed && writeCourses(courses, n);
        free(courses);
        return ok;
    }

    void generateCourseId(char *out, size_t len) {
        snprintf(out, len, "c%06lx", (unsigned long)(esp_timer_get_time() / 1000ULL) & 0xFFFFFF);
    }

    // ── JSON serialization (caller must free() the returned string) ──────────
    char* marksToJson(const Mark *marks, int count) {
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < count; i++) {
            cJSON *o = cJSON_CreateObject();
            cJSON_AddStringToObject(o, "id",      marks[i].id);
            cJSON_AddStringToObject(o, "name",    marks[i].name);
            cJSON_AddNumberToObject(o, "lat",     marks[i].lat);
            cJSON_AddNumberToObject(o, "lon",     marks[i].lon);
            cJSON_AddNumberToObject(o, "created", marks[i].created);
            cJSON_AddItemToArray(arr, o);
        }
        char *json = cJSON_PrintUnformatted(arr);
        cJSON_Delete(arr);
        return json;
    }

    char* coursesToJson(const Course *courses, int count) {
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < count; i++) {
            cJSON *o = cJSON_CreateObject();
            cJSON_AddStringToObject(o, "id",               courses[i].id);
            cJSON_AddStringToObject(o, "name",             courses[i].name);
            cJSON_AddStringToObject(o, "start_pin",        courses[i].start_pin);
            cJSON_AddStringToObject(o, "start_committee",  courses[i].start_committee);
            cJSON *ma = cJSON_CreateArray();
            for (int j = 0; j < courses[i].mark_count; j++) {
                cJSON *m = cJSON_CreateObject();
                cJSON_AddStringToObject(m, "mark_id", courses[i].marks[j].mark_id);
                cJSON_AddBoolToObject  (m, "port",    courses[i].marks[j].port_rounding);
                cJSON_AddItemToArray(ma, m);
            }
            cJSON_AddItemToObject(o, "marks", ma);
            cJSON_AddStringToObject(o, "finish_pin",       courses[i].finish_pin);
            cJSON_AddStringToObject(o, "finish_committee", courses[i].finish_committee);
            cJSON_AddItemToArray(arr, o);
        }
        char *json = cJSON_PrintUnformatted(arr);
        cJSON_Delete(arr);
        return json;
    }

private:
    enum class Backend {
        None,
        SdCard,
        Spiffs
    };

    Backend backend = Backend::None;
    sdmmc_card_t *sdCard = nullptr;
    sd_pwr_ctrl_handle_t sdPower = nullptr;
    char marksFile[32] = {};
    char coursesFile[32] = {};

    bool mountSdCard() {
        // The Waveshare ESP32-P4-WIFI6 connects VDDPST_5 and the microSD
        // supply to ESP_LDO_VO4. Without enabling channel 4 the bus pins toggle,
        // but the card remains unpowered and ACMD41 times out.
        sd_pwr_ctrl_ldo_config_t ldoConfig = {
            .ldo_chan_id = 4
        };
        esp_err_t ret = sd_pwr_ctrl_new_on_chip_ldo(&ldoConfig, &sdPower);
        if (ret != ESP_OK) {
            ESP_LOGW(STORAGE_TAG, "SD LDO setup failed: %s", esp_err_to_name(ret));
            sdPower = nullptr;
            return false;
        }

        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.slot = SDMMC_HOST_SLOT_0;
        host.max_freq_khz = SDMMC_FREQ_DEFAULT;
        host.pwr_ctrl_handle = sdPower;

        sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
        slot.width = 4;
        slot.clk = GPIO_NUM_43;
        slot.cmd = GPIO_NUM_44;
        slot.d0 = GPIO_NUM_39;
        slot.d1 = GPIO_NUM_40;
        slot.d2 = GPIO_NUM_41;
        slot.d3 = GPIO_NUM_42;
        slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

        esp_vfs_fat_sdmmc_mount_config_t mount = {
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 16 * 1024,
            .disk_status_check_enable = true,
            .use_one_fat = false
        };

        ret = esp_vfs_fat_sdmmc_mount(
            SD_MOUNT_POINT, &host, &slot, &mount, &sdCard);
        if (ret != ESP_OK) {
            ESP_LOGW(STORAGE_TAG, "SD card unavailable: %s", esp_err_to_name(ret));
            sdCard = nullptr;
            esp_err_t powerRet = sd_pwr_ctrl_del_on_chip_ldo(sdPower);
            if (powerRet != ESP_OK)
                ESP_LOGW(STORAGE_TAG, "SD LDO cleanup failed: %s",
                         esp_err_to_name(powerRet));
            sdPower = nullptr;
            return false;
        }

        backend = Backend::SdCard;
        mounted = true;
        setPaths(SD_MOUNT_POINT);
        uint64_t total = 0, used = 0;
        getInfo(&total, &used);
        ESP_LOGI(STORAGE_TAG, "SD card mounted: %llu MB total, %llu MB used",
                 (unsigned long long)(total / (1024 * 1024)),
                 (unsigned long long)(used / (1024 * 1024)));
        sdmmc_card_print_info(stdout, sdCard);
        return true;
    }

    bool mountSpiffs() {
        esp_vfs_spiffs_conf_t conf = {
            .base_path              = SPIFFS_MOUNT_POINT,
            .partition_label        = "storage",
            .max_files              = 5,
            .format_if_mount_failed = true
        };
        esp_err_t ret = esp_vfs_spiffs_register(&conf);
        if (ret != ESP_OK) {
            ESP_LOGE(STORAGE_TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
            return false;
        }
        backend = Backend::Spiffs;
        mounted = true;
        setPaths(SPIFFS_MOUNT_POINT);
        size_t total = 0, used = 0;
        esp_spiffs_info("storage", &total, &used);
        ESP_LOGW(STORAGE_TAG, "Using SPIFFS fallback: %u KB total, %u KB used",
                 (unsigned)(total / 1024), (unsigned)(used / 1024));
        return true;
    }

    void setPaths(const char *mountPoint) {
        snprintf(marksFile, sizeof(marksFile), "%s/marks.json", mountPoint);
        snprintf(coursesFile, sizeof(coursesFile), "%s/courses.json", mountPoint);
    }

    const char *marksPath() const {
        return marksFile;
    }

    const char *coursesPath() const {
        return coursesFile;
    }

    void logRootFiles() {
        DIR *dir = opendir(SD_MOUNT_POINT);
        if (!dir) {
            ESP_LOGW(STORAGE_TAG, "Cannot list SD card root");
            return;
        }

        int count = 0;
        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr && count < 32) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            char path[96];
            snprintf(path, sizeof(path), "%s/%s", SD_MOUNT_POINT, entry->d_name);
            struct stat st = {};
            if (stat(path, &st) == 0) {
                ESP_LOGI(STORAGE_TAG, "SD root: %s%s (%u bytes)",
                         entry->d_name, S_ISDIR(st.st_mode) ? "/" : "",
                         (unsigned)st.st_size);
            } else {
                ESP_LOGI(STORAGE_TAG, "SD root: %s", entry->d_name);
            }
            count++;
        }
        closedir(dir);
        if (count == 0) ESP_LOGI(STORAGE_TAG, "SD root is empty");
    }

    bool ensureFile(const char *path, const char *def) {
        struct stat st;
        if (stat(path, &st) == 0) return true;

        FILE *f = fopen(path, "w");
        if (!f) {
            ESP_LOGE(STORAGE_TAG, "Cannot create %s: errno %d", path, errno);
            return false;
        }
        bool ok = fputs(def, f) >= 0 && fflush(f) == 0;
        if (fclose(f) != 0) ok = false;
        if (!ok || stat(path, &st) != 0) {
            ESP_LOGE(STORAGE_TAG, "Failed to verify %s: errno %d", path, errno);
            return false;
        }
        ESP_LOGI(STORAGE_TAG, "Created %s", path);
        return true;
    }

    char* readFile(const char *path) {
        FILE *f = fopen(path, "r");
        if (!f) { ESP_LOGW(STORAGE_TAG, "Cannot open %s", path); return nullptr; }
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (size <= 0 || size > 32768) { fclose(f); return nullptr; }
        char *buf = (char *)malloc(size + 1);
        if (!buf) { fclose(f); return nullptr; }
        size_t r = fread(buf, 1, size, f);
        buf[r] = '\0';
        fclose(f);
        return buf;
    }

    bool writeFile(const char *path, const char *content) {
        char tmpPath[40];
        snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);
        FILE *f = fopen(tmpPath, "w");
        if (!f) { ESP_LOGE(STORAGE_TAG, "Cannot write %s", tmpPath); return false; }
        bool ok = fputs(content, f) >= 0 && fflush(f) == 0 && fsync(fileno(f)) == 0;
        if (fclose(f) != 0) ok = false;
        if (!ok) {
            unlink(tmpPath);
            ESP_LOGE(STORAGE_TAG, "Write failed for %s", path);
            return false;
        }
        unlink(path);
        if (rename(tmpPath, path) != 0) {
            unlink(tmpPath);
            ESP_LOGE(STORAGE_TAG, "Rename failed for %s", path);
            return false;
        }
        return true;
    }

    int parseMarks(const char *json, Mark *marks, int max_count) {
        cJSON *arr = cJSON_Parse(json);
        if (!arr || !cJSON_IsArray(arr)) { cJSON_Delete(arr); return 0; }
        int count = 0;
        cJSON *item;
        cJSON_ArrayForEach(item, arr) {
            if (count >= max_count) break;
            Mark &m = marks[count];
            memset(&m, 0, sizeof(m));
            cJSON *v;
            if ((v = cJSON_GetObjectItem(item, "id"))      && cJSON_IsString(v)) strlcpy(m.id,   v->valuestring, sizeof(m.id));
            if ((v = cJSON_GetObjectItem(item, "name"))    && cJSON_IsString(v)) strlcpy(m.name, v->valuestring, sizeof(m.name));
            if ((v = cJSON_GetObjectItem(item, "lat"))     && cJSON_IsNumber(v)) m.lat     = v->valuedouble;
            if ((v = cJSON_GetObjectItem(item, "lon"))     && cJSON_IsNumber(v)) m.lon     = v->valuedouble;
            if ((v = cJSON_GetObjectItem(item, "created")) && cJSON_IsNumber(v)) m.created = (uint32_t)v->valuedouble;
            count++;
        }
        cJSON_Delete(arr);
        return count;
    }

    int parseCourses(const char *json, Course *courses, int max_count) {
        cJSON *arr = cJSON_Parse(json);
        if (!arr || !cJSON_IsArray(arr)) { cJSON_Delete(arr); return 0; }
        int count = 0;
        cJSON *item;
        cJSON_ArrayForEach(item, arr) {
            if (count >= max_count) break;
            Course &c = courses[count];
            memset(&c, 0, sizeof(c));
            cJSON *v;
            if ((v = cJSON_GetObjectItem(item, "id"))               && cJSON_IsString(v)) strlcpy(c.id,               v->valuestring, sizeof(c.id));
            if ((v = cJSON_GetObjectItem(item, "name"))             && cJSON_IsString(v)) strlcpy(c.name,             v->valuestring, sizeof(c.name));
            if ((v = cJSON_GetObjectItem(item, "start_pin"))        && cJSON_IsString(v)) strlcpy(c.start_pin,        v->valuestring, sizeof(c.start_pin));
            if ((v = cJSON_GetObjectItem(item, "start_committee"))  && cJSON_IsString(v)) strlcpy(c.start_committee,  v->valuestring, sizeof(c.start_committee));
            if ((v = cJSON_GetObjectItem(item, "finish_pin"))       && cJSON_IsString(v)) strlcpy(c.finish_pin,       v->valuestring, sizeof(c.finish_pin));
            if ((v = cJSON_GetObjectItem(item, "finish_committee")) && cJSON_IsString(v)) strlcpy(c.finish_committee, v->valuestring, sizeof(c.finish_committee));
            cJSON *ma = cJSON_GetObjectItem(item, "marks");
            if (ma && cJSON_IsArray(ma)) {
                cJSON *mi;
                cJSON_ArrayForEach(mi, ma) {
                    if (c.mark_count >= MAX_COURSE_MARKS) break;
                    CourseMarkRef &ref = c.marks[c.mark_count];
                    if ((v = cJSON_GetObjectItem(mi, "mark_id")) && cJSON_IsString(v)) strlcpy(ref.mark_id, v->valuestring, sizeof(ref.mark_id));
                    v = cJSON_GetObjectItem(mi, "port");
                    ref.port_rounding = (!v || cJSON_IsTrue(v)); // default port
                    c.mark_count++;
                }
            }
            count++;
        }
        cJSON_Delete(arr);
        return count;
    }

    bool writeMarks(const Mark *marks, int count) {
        char *json = marksToJson(marks, count);
        if (!json) return false;
        bool ok = writeFile(marksPath(), json);
        free(json);
        return ok;
    }

    bool writeCourses(const Course *courses, int count) {
        char *json = coursesToJson(courses, count);
        if (!json) return false;
        bool ok = writeFile(coursesPath(), json);
        free(json);
        return ok;
    }
};
