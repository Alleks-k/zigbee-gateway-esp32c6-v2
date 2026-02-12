#include "http_static.h"
#include "esp_log.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static const char *TAG = "HTTP_STATIC";
/* Keep a small, bounded RAM footprint while streaming static files. */
#define HTTP_FILE_CHUNK_SIZE 4096

static bool req_hdr_equals(httpd_req_t *req, const char *hdr_name, const char *expected)
{
    size_t len = httpd_req_get_hdr_value_len(req, hdr_name);
    if (len == 0 || len >= 128) {
        return false;
    }

    char value[128];
    if (httpd_req_get_hdr_value_str(req, hdr_name, value, sizeof(value)) != ESP_OK) {
        return false;
    }
    return strcmp(value, expected) == 0;
}

static esp_err_t serve_spiffs_file(httpd_req_t *req, const char *filepath, const char *content_type, const char *cache_control)
{
    struct stat st;
    if (stat(filepath, &st) != 0) {
        ESP_LOGE(TAG, "Failed to stat %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    char etag[48];
    snprintf(etag, sizeof(etag), "W/\"%lx-%lx\"", (unsigned long)st.st_mtime, (unsigned long)st.st_size);

    char last_modified[48] = {0};
    struct tm tm_utc;
    if (gmtime_r(&st.st_mtime, &tm_utc)) {
        strftime(last_modified, sizeof(last_modified), "%a, %d %b %Y %H:%M:%S GMT", &tm_utc);
    }

    bool etag_matches = req_hdr_equals(req, "If-None-Match", etag);
    bool lm_matches = (last_modified[0] != '\0') && req_hdr_equals(req, "If-Modified-Since", last_modified);
    if (etag_matches || lm_matches) {
        httpd_resp_set_status(req, "304 Not Modified");
        if (cache_control) {
            httpd_resp_set_hdr(req, "Cache-Control", cache_control);
        }
        httpd_resp_set_hdr(req, "ETag", etag);
        if (last_modified[0] != '\0') {
            httpd_resp_set_hdr(req, "Last-Modified", last_modified);
        }
        return httpd_resp_send(req, NULL, 0);
    }

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, content_type);
    if (cache_control) {
        httpd_resp_set_hdr(req, "Cache-Control", cache_control);
    }
    httpd_resp_set_hdr(req, "ETag", etag);
    if (last_modified[0] != '\0') {
        httpd_resp_set_hdr(req, "Last-Modified", last_modified);
    }

    char buf[HTTP_FILE_CHUNK_SIZE];
    size_t len = 0;
    while ((len = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, len) != ESP_OK) {
            fclose(f);
            httpd_resp_sendstr_chunk(req, NULL);
            return ESP_FAIL;
        }
    }

    if (ferror(f)) {
        ESP_LOGE(TAG, "Read error while streaming %s", filepath);
        fclose(f);
        httpd_resp_sendstr_chunk(req, NULL);
        return ESP_FAIL;
    }

    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t web_handler(httpd_req_t *req)
{
    return serve_spiffs_file(req, "/www/index.html", "text/html; charset=utf-8", "no-cache");
}

esp_err_t css_handler(httpd_req_t *req)
{
    return serve_spiffs_file(req, "/www/style.css", "text/css", "public, max-age=86400");
}

esp_err_t js_handler(httpd_req_t *req)
{
    return serve_spiffs_file(req, "/www/script.js", "application/javascript", "public, max-age=86400");
}

esp_err_t favicon_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}
