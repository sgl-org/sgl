/* source/core/sgl_log.c
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL  
 * Document reference link: https://sgl-docs.readthedocs.io
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sgl_log.h>
#include <sgl_core.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#if CONFIG_SGL_LOG_LEVEL > SGL_LOG_LEVEL_NONE

/**
 * @brief sgl log printing function, used to print debugging information. Note that this function 
 *        should only be called in debugging mode, otherwise it may affect system real-time 
 *        performance due to long execution time
 * @param level:  log level, such as, INFO, USER...
 * @param format:  log content
 * @return none
*/
void sgl_log(const char *level, const char * format, ...)
{
    char buffer[120];
    int  tail = 0;
    int  pref_size = strlen(level);

    sgl_snprintf(buffer, sizeof(buffer), "%s", level);

    va_list va;
    va_start(va, format);
    sgl_vsnprintf(buffer + pref_size, sizeof(buffer) - pref_size, format, va);
    va_end(va);

    tail = strlen(buffer);
    if (tail + (int)strlen("\r\n"SGL_LOG_NONE) < (int)sizeof(buffer) - 1) {
        memcpy(&buffer[tail], &"\r\n"SGL_LOG_NONE, strlen("\r\n"SGL_LOG_NONE));
        buffer[tail + strlen("\r\n"SGL_LOG_NONE)] = 0;
    }

    sgl_log_stdout(buffer);
}

/**
 * @brief sgl assert handler, used to handle assertions
 * @param file:  file name
 * @param func:  function name
 * @param line:  line number
 * @return none
*/
void sgl_assert_handler(const char *file, const char *func, int line)
{
    sgl_log(SGL_ASSERT_FLAG, "file: %s, function: %s, line: %d", file, func, line);
    while (1) {

    };
}

#endif // CONFIG_SGL_LOG_LEVEL > SGL_LOG_LEVEL_NONE
