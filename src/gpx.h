#pragma once
// Self-contained GPX parser — no external XML library required.
// Handles the subset of GPX used by Navionics and similar apps:
//   <wpt lat lon><name/></wpt>
//   <rte><name/><rtept lat lon><name/></rtept></rte>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_log.h"
#include "storage.h"

#define GPX_TAG        "GPX"
#define GPX_MAX_ROUTES 64

struct GpxRoutePoint {
    char   name[32];
    double lat, lon;
};

struct GpxRoute {
    char          name[32];
    GpxRoutePoint points[MAX_COURSE_MARKS];
    int           point_count;
};

// ── Tiny XML helpers ──────────────────────────────────────────────────────────

// Return pointer to first '<' at or after p, or nullptr.
static inline const char* nextTag(const char *p, const char *end) {
    while (p < end && *p != '<') p++;
    return p < end ? p : nullptr;
}

// Read tag name into out[outlen] from position just after '<'.
// Returns pointer past the name (points at first space, '/', or '>').
static const char* readTagName(const char *p, const char *end, char *out, int outlen) {
    int i = 0;
    while (p < end && *p != ' ' && *p != '/' && *p != '>' && i < outlen - 1)
        out[i++] = *p++;
    out[i] = '\0';
    return p;
}

// Find needle within [haystack, end) without requiring null termination.
static const char* findInRange(const char *haystack, const char *end, const char *needle) {
    int nlen = strlen(needle);
    while (haystack + nlen <= end) {
        if (memcmp(haystack, needle, nlen) == 0) return haystack;
        haystack++;
    }
    return nullptr;
}

// Extract a double attribute value (e.g. lat="40.1234") from the attribute string.
static double attrDouble(const char *attrs_start, const char *tag_end, const char *attrName) {
    char pat[24];
    snprintf(pat, sizeof(pat), "%s=\"", attrName);
    const char *found = findInRange(attrs_start, tag_end, pat);
    if (!found) return 0.0;
    return atof(found + strlen(pat));
}

// Extract trimmed text content between current position and the next '<'.
// Writes into out[outlen]. Returns pointer past the closing '>'.
static const char* readTextUntilTag(const char *p, const char *end, char *out, int outlen) {
    // Collect text
    int i = 0;
    while (p < end && *p != '<' && i < outlen - 1)
        out[i++] = *p++;
    out[i] = '\0';
    // Trim leading whitespace
    char *s = out;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    if (s != out) memmove(out, s, strlen(s) + 1);
    // Trim trailing whitespace
    int len = strlen(out);
    while (len > 0 && (out[len-1] == ' ' || out[len-1] == '\t' ||
                       out[len-1] == '\r' || out[len-1] == '\n'))
        out[--len] = '\0';
    return p;
}

// ── GpxImporter ──────────────────────────────────────────────────────────────

class GpxImporter {
public:
    // Parsed marks (unique, extracted from <wpt> and <rtept>)
    Mark     newMarks[MAX_MARKS];
    int      nMarks = 0;

    // Parsed routes
    GpxRoute routes[GPX_MAX_ROUTES];
    int      nRoutes = 0;

    // Feed the complete GPX text. Can be called in multiple chunks;
    // call flush() after the last chunk. Returns false on unrecoverable error.
    bool feed(const char *data, int len, bool /*isFinal*/) {
        // Append to internal buffer
        if (bufUsed + len > (int)sizeof(buf) - 1) {
            ESP_LOGW(GPX_TAG, "GPX input truncated at %d bytes", bufUsed);
            len = (int)sizeof(buf) - 1 - bufUsed;
            if (len <= 0) return true; // just ignore overflow silently
        }
        memcpy(buf + bufUsed, data, len);
        bufUsed += len;
        buf[bufUsed] = '\0';
        return true;
    }

    // Parse the buffered data. Call after all feed() calls.
    bool parse() {
        const char *p   = buf;
        const char *end = buf + bufUsed;

        bool inWpt   = false;
        bool inRte   = false;
        bool inRtept = false;

        double pendLat = 0.0, pendLon = 0.0;
        char   pendName[32] = {};
        GpxRoute curRoute   = {};

        while (p < end) {
            const char *lt = nextTag(p, end);
            if (!lt) break;

            const char *tagStart = lt + 1;
            bool isClose = (*tagStart == '/');
            if (isClose) tagStart++;

            // Self-closing tags (e.g. <rtept ... />) are treated as open
            const char *gt = (const char*)memchr(tagStart, '>', end - tagStart);
            if (!gt) break;

            char tagName[32];
            const char *afterName = readTagName(tagStart, gt, tagName, sizeof(tagName));

            if (!isClose) {
                // ── Opening tags ─────────────────────────────────────────────
                if (strcmp(tagName, "wpt") == 0) {
                    inWpt   = true;
                    pendLat = attrDouble(afterName, gt, "lat");
                    pendLon = attrDouble(afterName, gt, "lon");
                    pendName[0] = '\0';

                } else if (strcmp(tagName, "rte") == 0) {
                    inRte = true;
                    memset(&curRoute, 0, sizeof(curRoute));

                } else if (strcmp(tagName, "rtept") == 0) {
                    inRtept = true;
                    pendLat = attrDouble(afterName, gt, "lat");
                    pendLon = attrDouble(afterName, gt, "lon");
                    pendName[0] = '\0';

                } else if (strcmp(tagName, "name") == 0) {
                    if (inWpt || inRte || inRtept) {
                        // Read text content immediately
                        p = readTextUntilTag(gt + 1, end, pendName, sizeof(pendName));
                        // If inside route (not rtept), this is the route name
                        if (inRte && !inRtept)
                            strlcpy(curRoute.name, pendName, sizeof(curRoute.name));
                        continue; // p already advanced past the text
                    }
                }

            } else {
                // ── Closing tags ─────────────────────────────────────────────
                if (strcmp(tagName, "wpt") == 0 && inWpt) {
                    addUniqueMark(pendName, pendLat, pendLon);
                    inWpt = false;

                } else if (strcmp(tagName, "rtept") == 0 && inRtept) {
                    // Collect as a mark (if not seen before)
                    addUniqueMark(pendName, pendLat, pendLon);
                    // Add to current route
                    if (curRoute.point_count < MAX_COURSE_MARKS && strlen(pendName) > 0) {
                        GpxRoutePoint &pt = curRoute.points[curRoute.point_count++];
                        strlcpy(pt.name, pendName, sizeof(pt.name));
                        pt.lat = pendLat;
                        pt.lon = pendLon;
                    }
                    inRtept = false;

                } else if (strcmp(tagName, "rte") == 0 && inRte) {
                    if (nRoutes < GPX_MAX_ROUTES && curRoute.point_count > 0) {
                        if (strlen(curRoute.name) == 0)
                            snprintf(curRoute.name, sizeof(curRoute.name), "Route %d", nRoutes + 1);
                        routes[nRoutes++] = curRoute;
                    }
                    inRte = false;
                }
            }

            p = gt + 1;
        }

        ESP_LOGI(GPX_TAG, "Parsed %d marks, %d routes", nMarks, nRoutes);
        return true;
    }

    int markCount()  const { return nMarks;  }
    int routeCount() const { return nRoutes; }

    // Build Course objects from parsed routes.
    // resolvedMarks must contain all marks with IDs already assigned.
    int buildCourses(Course *out, int maxOut,
                     const Mark *resolvedMarks, int resolvedCount) {
        int n = 0;
        for (int i = 0; i < nRoutes && n < maxOut; i++) {
            GpxRoute &gr = routes[i];
            Course &c = out[n];
            memset(&c, 0, sizeof(c));
            strlcpy(c.name, gr.name, sizeof(c.name));

            for (int j = 0; j < gr.point_count && c.mark_count < MAX_COURSE_MARKS; j++) {
                const char *mid = resolveMarkId(
                    gr.points[j].name, gr.points[j].lat, gr.points[j].lon,
                    resolvedMarks, resolvedCount);
                if (mid) {
                    strlcpy(c.marks[c.mark_count].mark_id, mid,
                            sizeof(c.marks[c.mark_count].mark_id));
                    c.marks[c.mark_count].port_rounding = true;
                    c.mark_count++;
                }
            }
            if (c.mark_count > 0) n++;
        }
        return n;
    }

private:
    // 64KB buffer — enough for any realistic GPX course file
    char buf[65536] = {};
    int  bufUsed    = 0;

    void addUniqueMark(const char *name, double lat, double lon) {
        if (!name || strlen(name) == 0 || nMarks >= MAX_MARKS) return;
        for (int i = 0; i < nMarks; i++)
            if (strcasecmp(newMarks[i].name, name) == 0) return;
        // Proximity dedup ~5m
        for (int i = 0; i < nMarks; i++) {
            double dlat = newMarks[i].lat - lat;
            double dlon = newMarks[i].lon - lon;
            if (dlat*dlat + dlon*dlon < 0.000045*0.000045) return;
        }
        Mark &m = newMarks[nMarks++];
        memset(&m, 0, sizeof(m));
        strlcpy(m.name, name, sizeof(m.name));
        m.lat = lat;
        m.lon = lon;
    }

    static const char* resolveMarkId(const char *name, double lat, double lon,
                                     const Mark *marks, int count) {
        for (int i = 0; i < count; i++)
            if (strcasecmp(marks[i].name, name) == 0) return marks[i].id;
        // Proximity fallback ~50m
        for (int i = 0; i < count; i++) {
            double dlat = marks[i].lat - lat;
            double dlon = marks[i].lon - lon;
            if (dlat*dlat + dlon*dlon < 0.00045*0.00045) return marks[i].id;
        }
        ESP_LOGW(GPX_TAG, "Cannot resolve '%s' to a mark", name);
        return nullptr;
    }
};
