/* source/include/sgl_log.h
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

#ifndef __SGL_LOG_H__
#define __SGL_LOG_H__

#include <sgl_cfgfix.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SGL_LOG_LEVEL_NONE               0 /**< Do not log anything */
#define SGL_LOG_LEVEL_ERROR              1 /**< Only critical issues */
#define SGL_LOG_LEVEL_WARN               2 /**< Unwanted situations that did not cause a failure */
#define SGL_LOG_LEVEL_INFO               3 /**< Important events */
#define SGL_LOG_LEVEL_TRACE              4 /**< Detailed information */
#define SGL_LOG_LEVEL_USER               5 /**< Custom user logs (enabled at any nonzero level) */
#define _SGL_LOG_LEVEL_NUM               6 /**< Number of log levels */

#if CONFIG_SGL_LOG_LEVEL > SGL_LOG_LEVEL_NONE

/**
 * @brief sgl log printing function, used to print logging information. Note that this function
 *        should only be called when logging is enabled, otherwise it may affect system real-time
 *        performance due to long execution time
 * 
 * @param level:  log level, such as, INFO, USER...
 * @param format:  log content
 * 
 * @return none
*/
void sgl_log(const char *level, const char * format, ...);

/**
 * @brief sgl assert handler, used to handle assertions
 * 
 * @param file:  file name
 * @param func:  function name
 * @param line:  line number
 * 
 * @return none
*/
void sgl_assert_handler(const char *file, const char *func, int line);

#if CONFIG_SGL_LOG_COLOR

#define SGL_LOG_NONE                    "\033[0m"
#define SGL_LOG_RED                     "\033[31m"
#define SGL_LOG_GREEN                   "\033[32m"
#define SGL_LOG_YELLOW                  "\033[33m"
#define SGL_LOG_BLUE                    "\033[34m"
#define SGL_LOG_PURPLE                  "\033[35m"
#define SGL_LOG_CYAN                    "\033[36m"
#define SGL_LOG_WHITE                   "\033[37m"
#define SGL_LOG_LIGHT_BLUE              "\033[1;34m"

#define SGL_LOG_TRACE_FLAG              SGL_LOG_BLUE"[TRACE]"
#define SGL_LOG_INFO_FLAG               SGL_LOG_GREEN"[INFO]"
#define SGL_LOG_WARN_FLAG               SGL_LOG_YELLOW"[WARN]"
#define SGL_LOG_ERROR_FLAG              SGL_LOG_RED"[ERROR]"
#define SGL_LOG_USER_FLAG               SGL_LOG_PURPLE"[USER]"
#define SGL_ASSERT_FLAG                 SGL_LOG_CYAN"[ASSERT]"
#define SGL_DEBUG_TRACE_FLAG            SGL_LOG_LIGHT_BLUE"[DEBUG]"

#else
#define SGL_LOG_TRACE_FLAG              "[TRACE] "
#define SGL_LOG_INFO_FLAG               "[INFO] "
#define SGL_LOG_WARN_FLAG               "[WARN] "
#define SGL_LOG_ERROR_FLAG              "[ERROR] "
#define SGL_LOG_USER_FLAG               "[USER] "
#define SGL_ASSERT_FLAG                 "[ASSERT] "
#define SGL_DEBUG_TRACE_FLAG            "[DEBUG]"
#define SGL_LOG_NONE                    ""

#endif // !CONFIG_SGL_LOG_COLOR

#ifndef SGL_LOG_TRACE
#    if CONFIG_SGL_LOG_LEVEL >= SGL_LOG_LEVEL_TRACE
#        define SGL_LOG_TRACE(...)      sgl_log(SGL_LOG_TRACE_FLAG, __VA_ARGS__)
#    else
#        define SGL_LOG_TRACE(...)      do {} while(0)
#    endif
#endif

#ifndef SGL_LOG_INFO
#    if CONFIG_SGL_LOG_LEVEL >= SGL_LOG_LEVEL_INFO
#        define SGL_LOG_INFO(...)       sgl_log(SGL_LOG_INFO_FLAG,  __VA_ARGS__)
#    else
#        define SGL_LOG_INFO(...)       do {} while(0)
#    endif
#endif

#ifndef SGL_LOG_WARN
#    if CONFIG_SGL_LOG_LEVEL >= SGL_LOG_LEVEL_WARN
#        define SGL_LOG_WARN(...)       sgl_log(SGL_LOG_WARN_FLAG, __VA_ARGS__)
#    else
#        define SGL_LOG_WARN(...)       do {} while(0)
#    endif
#endif

#ifndef SGL_LOG_ERROR
#    if CONFIG_SGL_LOG_LEVEL >= SGL_LOG_LEVEL_ERROR
#        define SGL_LOG_ERROR(...)      sgl_log(SGL_LOG_ERROR_FLAG, __VA_ARGS__)
#    else
#        define SGL_LOG_ERROR(...)      do {} while(0)
#    endif
#endif

#ifndef SGL_LOG_USER
#    define SGL_LOG_USER(...)           sgl_log(SGL_LOG_USER_FLAG,  __VA_ARGS__)
#endif

#if CONFIG_SGL_LOG_LEVEL > SGL_LOG_LEVEL_NONE
#    define SGL_LOG(...)                sgl_log(__VA_ARGS__)
#else
#    define SGL_LOG(...)                do {} while(0)
#endif

#define SGL_ASSERT(cond)                do {                                                          \
                                            if (!(cond)) {                                            \
                                                sgl_assert_handler(__FILE__, __FUNCTION__, __LINE__); \
                                            }                                                         \
                                        } while(0)

#define SGL_ASSERT_INFO(cond, info)     do {                                                          \
                                            if (!(cond)) {                                            \
                                                SGL_LOG_ERROR("ASSERT: %s", info);                    \
                                                sgl_assert_handler(__FILE__, __FUNCTION__, __LINE__); \
                                            }                                                         \
                                        } while(0)

#else // if logging is disabled

#define SGL_LOG_NONE                    0
#define SGL_LOG_TRACE(...)              do {} while(0)
#define SGL_LOG_INFO(...)               do {} while(0)
#define SGL_LOG_WARN(...)               do {} while(0)
#define SGL_LOG_ERROR(...)              do {} while(0)
#define SGL_LOG_USER(...)               do {} while(0)
#define SGL_LOG(...)                    do {} while(0)

#define SGL_ASSERT(cond)                do {} while(0)
#define SGL_ASSERT_INFO(cond, info)     do {} while(0)
#endif

#if CONFIG_SGL_LOG_LEVEL >= SGL_LOG_LEVEL_TRACE
#define SGL_DEBUG_TRACE(...)            sgl_log(SGL_DEBUG_TRACE_FLAG, __VA_ARGS__)
#else
#define SGL_DEBUG_TRACE(...)            do {} while(0)
#endif

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif //!__SGL_LOG_H__
