#include "log_capture.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* Ring buffer state. When full, the oldest bytes are overwritten. */
static char s_buffer[LOG_CAPTURE_BUFFER_SIZE];
static size_t s_head = 0; /* write position */
static size_t s_len  = 0; /* number of valid bytes stored */

static SemaphoreHandle_t s_mutex = NULL;
static vprintf_like_t s_orig_vprintf = NULL;

static void ring_write(const char *data, size_t len)
{
    if (s_mutex == NULL || data == NULL || len == 0) {
        return;
    }
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return; /* never block logging for long */
    }
    for (size_t i = 0; i < len; i++) {
        s_buffer[s_head] = data[i];
        s_head = (s_head + 1) % LOG_CAPTURE_BUFFER_SIZE;
        if (s_len < LOG_CAPTURE_BUFFER_SIZE) {
            s_len++;
        }
    }
    xSemaphoreGive(s_mutex);
}

/* vprintf hook: capture the formatted line, then forward to the original
 * handler so UART output is unchanged. */
static int log_tee_vprintf(const char *fmt, va_list args)
{
    char line[256];
    va_list args_copy;
    va_copy(args_copy, args);
    int n = vsnprintf(line, sizeof(line), fmt, args_copy);
    va_end(args_copy);

    if (n > 0) {
        if ((size_t)n >= sizeof(line)) {
            n = sizeof(line) - 1; /* line was truncated */
        }
        ring_write(line, (size_t)n);
    }

    if (s_orig_vprintf) {
        return s_orig_vprintf(fmt, args);
    }
    return vprintf(fmt, args);
}

void log_capture_init(void)
{
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
    }
    /* Returns the previously installed handler, which we keep calling. */
    s_orig_vprintf = esp_log_set_vprintf(log_tee_vprintf);
}

size_t log_capture_read(char *dst, size_t dst_len)
{
    if (dst == NULL || dst_len == 0) {
        return 0;
    }
    size_t to_copy = dst_len - 1;
    if (s_mutex == NULL ||
        xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        dst[0] = '\0';
        return 0;
    }
    if (s_len < to_copy) {
        to_copy = s_len;
    }
    /* Oldest byte is s_len bytes behind the write head. */
    size_t start = (s_head + LOG_CAPTURE_BUFFER_SIZE - s_len) % LOG_CAPTURE_BUFFER_SIZE;
    for (size_t i = 0; i < to_copy; i++) {
        dst[i] = s_buffer[(start + i) % LOG_CAPTURE_BUFFER_SIZE];
    }
    dst[to_copy] = '\0';
    xSemaphoreGive(s_mutex);
    return to_copy;
}

void log_capture_clear(void)
{
    if (s_mutex != NULL && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_head = 0;
        s_len  = 0;
        xSemaphoreGive(s_mutex);
    }
}
