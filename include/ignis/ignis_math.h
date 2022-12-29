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
 * @brief Gets the component-wise product between two Vector2's.
 * @param[in] a First vector
 * @param[in] b Second vector
 * @return Componnet-wise product between the two vectors
 */
Vector2 Vector2CWiseProduct(Vector2 a, Vector2 b);

/**
 * @brief Gets the dot product between two Vector2's.
 * @param[in] a First vector
 * @param[in] b Scond vector
 * @return Dot product
 */
float Vector2DotProduct(Vector2 a, Vector2 b);

/**
 * @brief Gets the projection of A onto B.
 * @param[in] a Vector A
 * @param[in] b Vector B
 * @return Vector representing the projection of A onto B
 */
Vector2 Vector2Project(Vector2 a, Vector2 b);

/**
 * @brief Gets the reflection of an incident vector along a normal vector.
 * @param[in] incident Incident vector
 * @param[in] normal Normal vector
 * @return Vector representing the reflection
 */
Vector2 Vector2Reflect(Vector2 incident, Vector2 normal);

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
 * @brief Gets the squared distance between two points.
 * @param[in] a First point
 * @param[in] b Second point
 * @return Squared distance between points a and b.
 */
float Vector2SquaredDistance(Vector2 a, Vector2 b);

/**
 * @brief Gets the distance between two points.
 * @param[in] a First point
 * @param[in] b Second point
 * @return Distance between points a and b.
 */
float Vector2Distance(Vector2 a, Vector2 b);

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
 * @brief Clamps an ineger value between the specified minimum and maximum.
 * @param[in] val Value to clamp
 * @param[in] min Minimum value
 * @param[in] max Maximum value
 * @return Clamped value
 */
int Clamp_Int(int val, int min, int max);

/**
 * @brief Clamps an unsigned 32-bit value between the specified minimum and maximum.
 * @param[in] val Value to clamp
 * @param[in] min Minimum value
 * @param[in] max Maximum value
 * @return Clamped value
 */
uint32_t Clamp_Uint32(uint32_t val, uint32_t min, uint32_t max);

/**
 * @brief Clamps a float value between the specified minimum and maximum.
 * @param[in] val Value to clamp
 * @param[in] min Minimum value
 * @param[in] max Maximum value
 * @return Clamped value
 */
float Clamp_Float(float val, float min, float max);

/**
 * @brief Clamps a double value between the specified minimum and maximum.
 * @param[in] val Value to clamp
 * @param[in] min Minimum value
 * @param[in] max Maximum value
 * @return Clamped value
 */
double Clamp_Double(double val, double min, double max);

/**
 * @brief Gets the minimum between two integers.
 * @param[in] a First number
 * @param[in] b Second number
 * @return Minimum between the two numbers
 */
int Min_Int(int a, int b);

/**
 * @brief Gets the minimum between two unsigned 32-bit numbers.
 * @param[in] a First number
 * @param[in] b Second number
 * @return Minimum between the two numbers
 */
uint32_t Min_Uint32(uint32_t a, uint32_t b);

/**
 * @brief Gets the minimum between two float numbers.
 * @param[in] a First number
 * @param[in] b Second number
 * @return Minimum between the two numbers
 */
float Min_Float(float a, float b);

/**
 * @brief Gets the minimum between two double numbers.
 * @param[in] a First number
 * @param[in] b Second number
 * @return Minimum between the two numbers
 */
double Min_Double(double a, double b);

/**
 * @brief Gets the maximum between two integers.
 * @param[in] a First number
 * @param[in] b Second number
 * @return Maximum between the two numbers
 */
uint32_t Max_Int(uint32_t a, uint32_t b);

/**
 * @brief Gets the maximum between two unsigned 32-bit numbers.
 * @param[in] a First number
 * @param[in] b Second number
 * @return Maximum between the two numbers
 */
uint32_t Max_Uint32(uint32_t a, uint32_t b);

/**
 * @brief Gets the maximum between two float numbers.
 * @param[in] a First number
 * @param[in] b Second number
 * @return Maximum between the two numbers
 */
float Max_Float(float a, float b);

/**
 * @brief Gets the maximum between two double numbers.
 * @param[in] a First number
 * @param[in] b Second number
 * @return Maximum between the two numbers
 */
double Max_Double(double a, double b);

/**
 * @brief Linearly interpolates by a factor t between two float values.
 * @param[in] a First value
 * @param[in] b Second value
 * @param[in] t Interpolation factor
 * @return Result of linear interpolation
 */
float Lerp_Float(float a, float b, float t);

/**
 * @brief Linearly interpolates by a factor t between two double values.
 * @param[in] a First value
 * @param[in] b Second value
 * @param[in] t Interpolation factor
 * @return Result of linear interpolation
 */
double Lerp_Double(double a, double b, double t);

// extern "C"
#ifdef __cplusplus
}
#endif

#endif // IGNIS_MATH_H
