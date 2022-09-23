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
 * @brief Scales the specified vector by the specified scale value.
 * @param[in] v Vector
 * @param[in] s Scale value
 * @return Scaled vector
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
float Vector2Dot(Vector2 a, Vector2 b)
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
    return Vector2Dot(v, v);
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
    float magnitude = Vector2SquaredMagnitude(v);
    if (magnitude > 0.0f)
    {
        return Vector2Scale(v, 1.0f / magnitude);
    }
    return v;
}

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

uint32_t Min(uint32_t a, uint32_t b)
{
    if (a <= b)
    {
        return a;
    }
    return b;
}

uint32_t Max(uint32_t a, uint32_t b)
{
    if (a >= b)
    {
        return a;
    }
    return b;
}
