#ifndef LOG_CAPTURE_H
#define LOG_CAPTURE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Size of the in-memory ring buffer holding captured serial log output. */
#define LOG_CAPTURE_BUFFER_SIZE 8192

/**
 * @brief Tee all ESP_LOG output into an in-memory ring buffer.
 *
 * Installs a custom vprintf handler via esp_log_set_vprintf(). UART output
 * is preserved (the previous handler is still called). Call once, as early
 * as possible in app_main(), so boot logs are captured too.
 */
void log_capture_init(void);

/**
 * @brief Copy captured log output into dst (NUL-terminated).
 *
 * At most dst_len - 1 bytes are copied. Returns the number of bytes
 * written, excluding the NUL terminator.
 */
size_t log_capture_read(char *dst, size_t dst_len);

/**
 * @brief Discard all captured log output.
 */
void log_capture_clear(void);

#ifdef __cplusplus
}
#endif

#endif // LOG_CAPTURE_H
