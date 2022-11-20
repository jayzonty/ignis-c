#ifndef IGNIS_BEZIER_H
#define IGNIS_BEZIER_H

#include "ignis/ignis_math.h"

// When linking with C++, prevent name mangling of C functions
#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Gets the position of a point along the bezier curve. 
 * @param[in] controlPoints Control points
 * @param[in] numControlPoints Number of control points
 * @param[in] t Percentage along the bezier curve path
 * @return Point along the bezier curve
 */
Vector2 GetBezierCurvePoint(Vector2* controlPoints, int numControlPoints, float t);

/**
 * @brief Gets the point and tangent of a point along the bezier curve. 
 * @param[in] controlPoints Control points
 * @param[in] numControlPoints Number of control points
 * @param[in] t Percentage along the bezier curve path
 * @param[out] outTangent Pointer to the Vector2 where the tangent will be outputted
 * @return Point along the bezier curve
 */
Vector2 GetBezierCurvePointTangent(Vector2* controlPoints, int numControlPoints, float t, Vector2* outTangent);

/**
 * @brief Gets the position of all points along the bezier curve.
 * @param[in] controlPoints Control points
 * @param[in] numControlPoints Number of control points
 * @param[in] numSteps Number of segments in the bezier curve
 * @param[in] outPoints Pointer to a Vector2 array of size (numSteps + 1) that will contain the output points
 */
void GetBezierCurvePoints(Vector2* controlPoints, int numControlPoints, int numSteps, Vector2* outPoints);

/**
 * @brief Gets the position of all points and tangents along the bezier curve.
 * @param[in] controlPoints Control points
 * @param[in] numControlPoints Number of control points
 * @param[in] numSteps Number of segments in the bezier curve
 * @param[in] outPoints Pointer to a Vector2 array of size (numSteps + 1) that will contain the output points
 * @param[in] outTangents Pointer to a Vector2 array of size (numSteps) that will contain the output tangents
 */
void GetBezierCurvePointsTangents(Vector2* controlPoints, int numControlPoints, int numSteps, Vector2* outPoints, Vector2* outTangents);

// extern "C"
#ifdef __cplusplus
}
#endif

#endif // IGNIS_BEZIER_H
