/* source/include/sgl_math.h
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

#ifndef __SGL_MATH_H__
#define __SGL_MATH_H__

#include <sgl_types.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define sgl_min(a, b)                     ((a) < (b) ? (a) : (b))
#define sgl_min3(a, b, c)                 (sgl_min(sgl_min((a), (b)), (c)))
#define sgl_min4(a, b, c, d)              (sgl_min(sgl_min((a), (b)), sgl_min((c), (d))))

#define sgl_max(a, b)                     ((a) > (b) ? (a) : (b))
#define sgl_max3(a, b, c)                 (sgl_max(sgl_max((a), (b)), (c)))
#define sgl_max4(a, b, c, d)              (sgl_max(sgl_max((a), (b)), sgl_max((c), (d))))

#define sgl_clamp(val, min, max)          (sgl_max(min, (sgl_min((val), (max)))))

#define sgl_abs(x)                        ((x) > 0 ? (x) : (- (x)))

#define sgl_pow2(x)                       ((x) * (x))
#define sgl_pow3(x)                       ((x) * (x) * (x))
#define sgl_pow4(x)                       ((x) * (x) * (x) * (x))

#define sgl_is_pow2(x)                    (((x) & ((x) - 1)) == 0)
#define sgl_is_even(x)                    (((x) & 1) == 0)
#define sgl_is_odd(x)                     ((x) & 1)

#define sgl_swap(a, b)                    do { (*a) ^= (*b); (*b) ^= (*a); (*a) ^= (*b); } while (0)

#define sgl_mid(a, b)                     (((a) + (b)) / 2)

#define sgl_square_sum(x, y)              ((x*x) + (y*y))

#define SGL_FIXED_SHIFT                   (15)
#define SGL_FIXED_ONE                     (1 << SGL_FIXED_SHIFT)
#define SGL_FIXED_MASK                    (SGL_FIXED_ONE - 1)

#define SGL_SIN_SHIFT                     (15)
#define SGL_SIN_FIXED_ONE                 (32768)
#define SGL_COS_FIXED_ONE                 (32768)

#define SGL_PI                            (3.1415926f)
#define SGL_INV_360                       (1.0f / 360.0f)
#define SGL_RAD_TO_DEG                    (57.29577951308232f)
#define SGL_DEG_TO_RAD                    (0.017453292519943295f)

/**
 * @brief Calculate the absolute value of a float number.
 * @param x: float number
 * @return absolute value of the float number
 */
static inline float sgl_fabsf(float x)
{
    union { float f; uint32_t i;} u;
    u.f = x;
    u.i &= 0x7FFFFFFF;
    return u.f;
}

/**
 * @brief Normalize an angle to the range of 0 to 360 degrees.
 * @param angle: Angle in degrees such 0-359
 * @return Angle in degrees such 0-359
 */
static inline int16_t sgl_mod360(int16_t angle)
{
    while (angle >= 360) {
        angle -= 360;
    }
    while (angle < 0) {
        angle += 360;
    }
    return angle;
}

/**
 * @brief Calculate the sine of an angle
 * @param angle: Angle in degrees such 0-359
 * @return sine of the angle from sin0_90_table
 * @note This function has implemented angle normalization to the range of 0 to 360 degrees.
 */
int32_t sgl_sin(int16_t angle);

/**
 * @brief Calculate the cos of an angle
 * @param angle: Angle in degrees such 0-359
 * @return cos of the angle from sin0_90_table
 * @note This function has implemented angle normalization to the range of 0 to 360 degrees.
 */
static inline int32_t sgl_cos(int16_t angle)
{
    return sgl_sin(angle + 90);
}

/**
 * @brief Calculate the sine of an angle
 * @param angle: Angle in degrees such 0-359
 * @return sine of the angle from sin0_90_table
 * @note This function has implemented angle normalization to the range of 0 to 360 degrees.
 */
float sgl_sinf(float angle);

/**
 * @brief Calculate the cosine of an angle
 * @param angle: Angle in degrees such 0-359
 * @return cosine of the angle from sin0_90_table
 * @note This function has implemented angle normalization to the range of 0 to 360 degrees.
 */
static inline float sgl_cosf(float angle)
{
    return sgl_sinf(angle + 90.0f);
}

/**
 * @brief Calculate the sine of an angle (radian version)
 * @param rad: Angle in radians
 * @return sine of the angle
 * @note Internally converts to degrees and delegates to sgl_sinf().
 *       No manual normalization needed — sgl_sinf handles it.
 */
static inline float sgl_sinf_rad(float rad)
{
    return sgl_sinf(rad * SGL_RAD_TO_DEG);
}

/**
 * @brief Calculate the cosine of an angle (radian version)
 * @param rad: Angle in radians
 * @return cosine of the angle
 * @note Uses identity cos(x) = sin(x + π/2), then delegates to sgl_sinf().
 *       The +90° offset is applied AFTER conversion, matching sgl_cosf's logic.
 */
static inline float sgl_cosf_rad(float rad)
{
    return sgl_cosf(rad * SGL_RAD_TO_DEG);
}

/**
 * @brief  Calculate x number square root
 * @param  x: x number
 * @retval x number square root
 */
uint16_t sgl_sqrt(uint32_t x);

/**
 * @brief  Calculate x float number square root
 * @param  x: x number
 * @retval x number square root
 */
float sgl_sqrtf(float x);

/**
 * @brief Calculate error of the square root for a number.
 * @param x The number to calculate the error for.
 * @return The error of the square root. [0 ~ 255]
 */
uint8_t sgl_sqrt_error(uint32_t x);

/**
 * @brief Calculate the angle based on the x and y coordinates. This function is a fast algorithm
 *         implementation, with reference address: www.RomanBlack.com (Fast XY vector to integer degree algorithm)
 *
 * @param x:  The x coordinate on a circle
 * @param y:  The y coordinate on a circle
 * @return angle 
 * @note return angle [-32767 ~ 32767]
 */
int32_t sgl_atan2_raw(int x, int y);

/**
 * @brief Calculate the angle based on the x and y coordinates. This function is a fast algorithm 
 *        implementation, with reference address: www.RomanBlack.com (Fast XY vector to integer degree algorithm)
 * 
 * @param x:  The x coordinate on a circle
 * @param y:  The y coordinate on a circle
 * @return angle
 * @note return angle [0 ~ 359]
*/
uint16_t sgl_atan2(int x, int y);

/**
 * @brief Fast atan2 returning radians in (-π, π], backed by sgl_atan2 LUT.
 * @param x: The x coordinate (standard atan2 parameter order)
 * @param y: The y coordinate (standard atan2 parameter order)
 * @return Angle in radians in the range (-π, π]
 * @note Internally delegates to sgl_atan2(x, y) and converts [0,359] degrees
 *       to signed radians. Resolution is limited to ~1° (~0.01745 rad).
 */
float sgl_atan2f_rad(float x, float y);

/**
 * @brief Calculate the angle based on the x and y coordinates. This function is a fast algorithm 
 *        implementation, with reference address: www.RomanBlack.com (Fast XY vector to integer degree algorithm)
 * 
 * @param x:  The x coordinate on a circle
 * @param y:  The y coordinate on a circle
 * @return angle
 * @note return angle [0 ~ 359]
*/
float sgl_atan2f(float y, float x);

/**
 * @brief Split the length into n parts, with the weight of each part.
 * @param weight: The weight of each part.
 * @param count: The count of parts.
 * @param length: The length to split.
 * @param gap: The gap between each part.
 * @param out: The length of each part.
 * @note The error accumulation method of Bresenham's algorithm
 */
void sgl_split_len(const uint8_t *weight, int count, int16_t length, int16_t gap, int16_t *out);

/**
 * @brief Split the length into n parts, with the weight of each part.
 * @param length: The length to split.
 * @param count: The count of parts.
 * @param gap: The gap between each part.
 * @param out: The length of each part.
 * @note The average method of Bresenham's algorithm
 */
void sgl_split_len_avg(int length, int count, int16_t gap, int16_t *out);

/**
 * @brief Calculate whether the x and y coordinates have a component.
 * @param x:  The x coordinate on a circle
 * @param y:  The y coordinate on a circle
 * @param xv: The x coordinate of the vector
 * @param yv: The y coordinate of the vector
 * @return 1: The x and y coordinates have a component. 0: The x and y coordinates do not have a component.
 */
static inline int sgl_xy_has_component(int x, int y, int xv, int yv)
{
    return (x * xv + y * yv) > 0;
}

/**
 * @brief Generate a random number.
 * @param none
 * @return Random number.
 * @note The random number generator is in the range of [0, 32767].
 */
int16_t sgl_rand(void);

/**
 * @brief Generate a float random number.
 * @param none
 * @return Random number.
 * @note The random number generator is in the range of [0, 1].
 */
static inline float sgl_randf(void)
{
    return (float)sgl_rand() / (float)32767;
}

/**
 * @brief Set the seed of the random number generator.
 * @param seed: The seed of the random number generator.
 * @return none
 */
void sgl_srand(unsigned int seed);

#ifdef __cplusplus
}
#endif

#endif // !__SGL_MATH_H__
