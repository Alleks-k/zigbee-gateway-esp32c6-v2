#include "http_static.h"
#include "esp_log.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static const char *TAG = "HTTP_STATIC";
/* Keep a small, bounded RAM footprint while streaming static files. */
#define HTTP_FILE_CHUNK_SIZE 4096

static char *alloc_chunk_buffer(size_t *out_size)
{
    static const size_t candidates[] = {HTTP_FILE_CHUNK_SIZE, 2048, 1024, 512};
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        size_t chunk = candidates[i];
        char *buf = (char *)malloc(chunk);
        if (buf) {
            *out_size = chunk;
            return buf;
        }
    }
    *out_size = 0;
    return NULL;
}

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

    size_t chunk_size = 0;
    char *buf = alloc_chunk_buffer(&chunk_size);
    if (!buf) {
        fclose(f);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
        return ESP_ERR_NO_MEM;
    }
    if (chunk_size != HTTP_FILE_CHUNK_SIZE) {
        ESP_LOGW(TAG, "Low memory: fallback chunk size %u bytes for %s", (unsigned)chunk_size, filepath);
    }

    size_t len = 0;
    while ((len = fread(buf, 1, chunk_size, f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, len) != ESP_OK) {
            free(buf);
            fclose(f);
            httpd_resp_sendstr_chunk(req, NULL);
            return ESP_FAIL;
        }
    }

    if (ferror(f)) {
        ESP_LOGE(TAG, "Read error while streaming %s", filepath);
        free(buf);
        fclose(f);
        httpd_resp_sendstr_chunk(req, NULL);
        return ESP_FAIL;
    }

    free(buf);
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
