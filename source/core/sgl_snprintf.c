/* source/core/sgl_snprintf.c
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

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

/* float format support switch, disable it to save code size (set to 1 to enable) */
#ifndef CONFIG_SGL_SNPRINTF_FLOAT
#define CONFIG_SGL_SNPRINTF_FLOAT                                 (0)
#endif

/**
 * @brief append a character to the buffer
 * @param buf buffer
 * @param size buffer size
 * @param pos current position
 * @param c character to append
 */
static inline void append_char(char *buf, size_t size, size_t *pos, char c)
{
    if (*pos + 1 < size) {
        buf[*pos] = c;
    }
    (*pos)++;
}

/**
 * @brief append a string to the buffer
 * @param buf buffer
 * @param size buffer size
 * @param pos current position
 * @param s string to append
 */
static inline void append_str(char *buf, size_t size, size_t *pos, const char* s)
{
    while (*s) append_char(buf, size, pos, *s++);
}

/**
 * @brief pad alignment characters (default: space) to the buffer
 * @param buf buffer
 * @param size buffer size
 * @param pos current position
 * @param pad_len number of padding characters to append
 * @param pad_char padding character (default: ' ')
 */
static inline void pad_align(char *buf, size_t size, size_t *pos, size_t pad_len, char pad_char)
{
    for (size_t i = 0; i < pad_len; i++) {
        append_char(buf, size, pos, pad_char);
    }
}

/**
 * @brief calculate the length of an integer when converted to string
 * @param val integer value
 * @return number of characters of the integer string
 */
static inline size_t int_str_len(int val)
{
    if (val == 0) return 1; // Special case: 0 has length 1

    size_t len = 0;
    bool neg = val < 0;
    
    if (neg) val = -val;

    do {
        len++;
        val /= 10;
    } while (val);

    return neg ? len + 1 : len; // Add 1 for negative sign
}

/**
 * @brief append an integer to the buffer with left/right alignment support
 * @param buf buffer
 * @param size buffer size
 * @param pos current position
 * @param val integer to append
 * @param width alignment width (0 means no alignment)
 * @param left_align whether to use left alignment (true: left, false: right)
 * @param zero_pad if pad zeroes, pad zeroes instead of spaces
 */
static void append_int(char *buf, size_t size, size_t *pos, int val, int width, bool left_align, bool zero_pad)
{
    char tmp[12];
    int i = 0;
    bool neg = (val < 0);
    unsigned int uval = neg ? -(unsigned int)val : (unsigned int)val;

    do {
        tmp[i++] = '0' + (uval % 10);
        uval /= 10;
    } while (uval);

    if (neg) tmp[i++] = '-';
    int pad = (width > i) ? (width - i) : 0;

    if (!left_align && pad > 0) {
        char pc = (zero_pad && !neg) ? '0' : ' ';
        if (zero_pad && neg) {
            append_char(buf, size, pos, '-');
            i--;
        }
        while (pad-- > 0) append_char(buf, size, pos, pc);
    }

    while (i-- > 0) append_char(buf, size, pos, tmp[i]);

    if (left_align && pad > 0) {
        while (pad-- > 0) append_char(buf, size, pos, ' ');
    }
}

/**
 * @brief append a hexadecimal number to the buffer
 * @param buf buffer
 * @param size buffer size
 * @param pos current position
 * @param val hexadecimal number to append
 * @param upper whether to use uppercase letters
 */
static inline void append_hex(char *buf, size_t size, size_t *pos, unsigned int val, bool upper)
{
    char tmp[32];
    int i = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    do {
        tmp[i++] = digits[val & 0xF];
        val >>= 4;
    } while (val);

    while (i--) append_char(buf, size, pos, tmp[i]);
}

/**
 * @brief append a float to the buffer
 * @param buf buffer
 * @param size buffer size
 * @param pos current position
 * @param val float to append
 * @param precision number of decimal places (-1 for default of 6)
 */
#if (CONFIG_SGL_SNPRINTF_FLOAT)
static void append_float(char *buf, size_t size, size_t *pos, double val, int precision)
{
    int int_part = (int)val;
    double frac = val - int_part;
    
    if (val < 0) {
        append_char(buf, size, pos, '-');
        int_part = -int_part;
        frac = -frac;
    }

    append_int(buf, size, pos, int_part, 0, false, false);
    append_char(buf, size, pos, '.');

    int prec = (precision >= 0) ? precision : 6;
    for (int i = 0; i < prec; i++) {
        frac *= 10;
        int d = (int)frac;
        append_char(buf, size, pos, '0' + d);
        frac -= d;
    }
}
#endif // !CONFIG_SGL_SNPRINTF_FLOAT

/**
 * @brief format a string, a simple version of vsnprintf (with width alignment support)
 * @param buf buffer
 * @param size buffer size
 * @param fmt format string
 * @param ap argument list
 * @return number of characters written
 */
int sgl_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    size_t pos = 0;

    while (*fmt) {
        char c = *fmt;

        if (c != '%') {
            append_char(buf, size, &pos, c);
            fmt++;
            continue;
        }

        fmt++;
        bool zero_pad = false;
        bool left_align = false;
        int width = 0;
        int precision = -1;

        if (*fmt == '-') {
            left_align = true;
            fmt++;
        }

        if (*fmt == '0') {
            zero_pad = true;
            fmt++;
        }

        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        if (*fmt == '.') {
            fmt++;
            precision = 0;
            while (*fmt >= '0' && *fmt <= '9') {
                precision = precision * 10 + (*fmt - '0');
                fmt++;
            }
        }

        char spec = *fmt;
        switch (spec) {
        case 's': {
            const char *s = va_arg(ap, const char*);
            append_str(buf, size, &pos, s);
            break;
        }

        case 'd': {
            int d = va_arg(ap, int);
            append_int(buf, size, &pos, d, width, left_align, zero_pad);  // 加了个参数
            break;
        }

        case 'X':
        case 'x': {
            unsigned int x = va_arg(ap, unsigned int);
            append_hex(buf, size, &pos, x, spec == 'X');
            break;
        }

#if (CONFIG_SGL_SNPRINTF_FLOAT)
        case 'f': {
            double f = va_arg(ap, double);
            append_float(buf, size, &pos, f, precision);
            break;
        }
#endif // !CONFIG_SGL_SNPRINTF_FLOAT

        case 'c': {
            char ch = (char)va_arg(ap, int);
            append_char(buf, size, &pos, ch);
            break;
        }

        case '%': {
            append_char(buf, size, &pos, '%');
            break;
        }

        default:
            append_char(buf, size, &pos, '%');
            append_char(buf, size, &pos, spec);
            break;
        }

        fmt++;
    }

    if (size > 0) {
        buf[(pos < size) ? pos : (size - 1)] = '\0';
    }

    return (int)pos;
}

/**
 * @brief format a string, a simple version of snprintf (with width alignment support)
 * @param buf buffer
 * @param size buffer size
 * @param fmt format string
 * @param ... arguments
 * @return number of characters written
 */
int sgl_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = sgl_vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}

/**
 * @brief format a string, a simple version of sprintf (with width alignment support)
 * @param buf buffer
 * @param fmt format string
 * @param ... arguments
 * @return number of characters written
 */
int sgl_sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = sgl_vsnprintf(buf, 65535, fmt, ap);
    va_end(ap);
    return ret;
}
