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
 */
Vector2 GetBezierCurvePoint(Vector2* controlPoints, int numControlPoints, float t);

/**
 * @brief Gets the position of all points along the bezier curve.
 * @param[in] controlPoints Control points
 * @param[in] numControlPoints Number of control points
 * @param[in] numSteps Number of segments in the bezier curve
 * @param[in] outPoints Pointer to a Vector2 array of size (numSteps + 1) that will contain the output points
 */
void GetBezierCurvePoints(Vector2* controlPoints, int numControlPoints, int numSteps, Vector2* outPoints);

// extern "C"
#ifdef __cplusplus
}
#endif

#endif // IGNIS_BEZIER_H
