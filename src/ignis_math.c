#include "ignis/ignis_math.h"

#include <math.h>

/**
 * @brief Addition between two Vector2's.
 * @param[in] a First vector
 * @param[in] b Second vector
 * @return Sum of the two vectors
 */
Vector2 Vector2Add(Vector2 a, Vector2 b)
{
    Vector2 ret = { a.x + b.x, a.y + b.y };
    return ret;
}

/**
 * @brief Subtraction between two Vector2's.
 * @param[in] a First vector
 * @param[in] b Second vector
 * @return Difference of the two vectors
 */
Vector2 Vector2Subtract(Vector2 a, Vector2 b)
{
    Vector2 ret = { a.x - b.x, a.y - b.y };
    return ret;
}

/**
 * @brief Scales the specified vector by the specified scale value.
 * @param[in] v Vector
 * @param[in] s Scale value @return Scaled vector
 */
Vector2 Vector2Scale(Vector2 v, float s)
{
    Vector2 ret = { v.x * s, v.y * s };
    return ret;
}

/**
 * @brief Gets the dot product between two Vector2's.
 * @param[in] a First vector
 * @param[in] b Scond vector
 * @return Dot product
 */
float Vector2DotProduct(Vector2 a, Vector2 b)
{
    return a.x * b.x + a.y * b.y;
}

/**
 * @brief Gets the squared magnitude of the specified vector.
 * @param[in] v Vector
 * @return Vector squared magnitude
 */
float Vector2SquaredMagnitude(Vector2 v)
{
    return Vector2DotProduct(v, v);
}

/**
 * @brief Gets the magnitude of the specified vector.
 * @param[in] v Vector
 * @return Vector magnitude
 */
float Vector2Magnitude(Vector2 v)
{
    float squaredMagnitude = Vector2SquaredMagnitude(v);
    return sqrtf(squaredMagnitude);
}

/**
 * @brief Normalizes the specified vector.
 * @param[in] v Vector to normalize
 * @return Normalized version of the specified vector
 */
Vector2 Vector2Normalize(Vector2 v)
{
    float magnitude = Vector2Magnitude(v);
    if (magnitude > 0.0f)
    {
        return Vector2Scale(v, 1.0f / magnitude);
    }
    return v;
}

/**
 * @brief Gets the squared distance between two points.
 * @param[in] a First point
 * @param[in] b Second point
 * @return Squared distance between points a and b.
 */
float Vector2SquaredDistance(Vector2 a, Vector2 b)
{
    Vector2 D = Vector2Subtract(a, b);
    return Vector2SquaredMagnitude(D);
}

/**
 * @brief Gets the distance between two points.
 * @param[in] a First point
 * @param[in] b Second point
 * @return Distance between points a and b.
 */
float Vector2Distance(Vector2 a, Vector2 b)
{
    Vector2 D = Vector2Subtract(a, b);
    return Vector2Magnitude(D);
}

/**
 * @brief Gets the unit vector representing the direction from the specified source to the specified destination.
 * @param[in] source Source point
 * @param[in] destination Destination point
 * @return Unit vector representing the direction from the source to the destination
 */
Vector2 Vector2Direction(Vector2 source, Vector2 destination)
{
    Vector2 ret = Vector2Subtract(destination, source);
    ret = Vector2Normalize(ret);
    return ret;
}

/**
 * @brief Clamps the specified value between the specified minimum and maximum.
 * @param[in] val Value to clamp
 * @param[in] min Minimum value
 * @param[in] max Maximum value
 * @return Clamped value
 */
uint32_t Clamp(uint32_t val, uint32_t min, uint32_t max)
{
    if (val < min)
    {
        return min;
    }
    if (val > max)
    {
        return max;
    }
    return val;
}

/**
 * @brief Gets the minimum between the two numbers.
 * @param[in] a First number
 * @param[in] b Second number
 * @return Minimum between the two numbers
 */
uint32_t Min(uint32_t a, uint32_t b)
{
    if (a <= b)
    {
        return a;
    }
    return b;
}

/**
 * @brief Gets the maximum between the two numbers.
 * @param[in] a First number
 * @param[in] b Second number
 * @return Maximum between the two numbers
 */
uint32_t Max(uint32_t a, uint32_t b)
{
    if (a >= b)
    {
        return a;
    }
    return b;
}

/**
 * @brief Linearly interpolates by a factor t between two float values.
 * @param[in] a First value
 * @param[in] b Second value
 * @param[in] t Interpolation factor
 * @return Result of linear interpolation
 */
float Lerp_Float(float a, float b, float t)
{
    return a * (1.0f - t) + b * t;
}

