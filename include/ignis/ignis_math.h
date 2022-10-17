#ifndef IGNIS_MATH_H
#define IGNIS_MATH_H

#include <stdint.h>

// ----------------------------------------
// Structs
// ----------------------------------------

// 2-D vector
typedef struct
{
    float x, y;
} Vector2;

// 3-D vector
typedef struct
{
    float x, y, z;
} Vector3;

// Rectangular bounding box
typedef struct
{
    float x, y, width, height;
} Rect;

// When linking with C++, prevent name mangling of C functions
#ifdef __cplusplus
extern "C"
{
#endif

// ----------------------------------------
// Vector2 operations
// ----------------------------------------

/**
 * @brief Addition between two Vector2's.
 * @param[in] a First vector
 * @param[in] b Second vector
 * @return Sum of the two vectors
 */
Vector2 Vector2Add(Vector2 a, Vector2 b);

/**
 * @brief Subtraction between two Vector2's.
 * @param[in] a First vector
 * @param[in] b Second vector
 * @return Difference of the two vectors
 */
Vector2 Vector2Subtract(Vector2 a, Vector2 b);

/**
 * @brief Scales the specified vector by the specified scale value.
 * @param[in] v Vector
 * @param[in] s Scale value
 * @return Scaled vector
 */
Vector2 Vector2Scale(Vector2 v, float s);

/**
 * @brief Gets the dot product between two Vector2's.
 * @param[in] a First vector
 * @param[in] b Scond vector
 * @return Dot product
 */
float Vector2DotProduct(Vector2 a, Vector2 b);

/**
 * @brief Gets the squared magnitude of the specified vector.
 * @param[in] v Vector
 * @return Vector squared magnitude
 */
float Vector2SquaredMagnitude(Vector2 v);

/**
 * @brief Gets the magnitude of the specified vector.
 * @param[in] v Vector
 * @return Vector magnitude
 */
float Vector2Magnitude(Vector2 v);

/**
 * @brief Normalizes the specified vector.
 * @param[in] v Vector to normalize
 * @return Normalized version of the specified vector
 */
Vector2 Vector2Normalize(Vector2 v);

/**
 * @brief Gets the unit vector representing the direction from the specified source to the specified destination.
 * @param[in] source Source point
 * @param[in] destination Destination point
 * @return Unit vector representing the direction from the source to the destination
 */
Vector2 Vector2Direction(Vector2 source, Vector2 destination);

// ----------------------------------------
// Miscellaneous utility operations
// ----------------------------------------

/**
 * @brief Clamps the specified value between the specified minimum and maximum.
 * @param[in] val Value to clamp
 * @param[in] min Minimum value
 * @param[in] max Maximum value
 * @return Clamped value
 */
uint32_t Clamp(uint32_t val, uint32_t min, uint32_t max);

/**
 * @brief Gets the minimum between the two numbers.
 * @param[in] a First number
 * @param[in] b Second number
 * @return Minimum between the two numbers
 */
uint32_t Min(uint32_t a, uint32_t b);

/**
 * @brief Gets the maximum between the two numbers.
 * @param[in] a First number
 * @param[in] b Second number
 * @return Maximum between the two numbers
 */
uint32_t Max(uint32_t a, uint32_t b);

// extern "C"
#ifdef __cplusplus
}
#endif

#endif // IGNIS_MATH_H
