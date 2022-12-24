#include "ignis/ignis_bezier.h"

#include "ignis_bezier_internal.h"

#include <vulkan/vulkan_core.h>

#include <math.h>

// Maximum rows in the pascal triangle
#define MAX_PASCAL_TRIANGLE_SIZE 100

// 2D array containing the coefficients of the pascal triangle
int pascalTriangle[MAX_PASCAL_TRIANGLE_SIZE][MAX_PASCAL_TRIANGLE_SIZE + 1];


// ----------------------------------------
// Public Functions
// ----------------------------------------

Vector2 GetBezierCurvePoint(Vector2* controlPoints, int numControlPoints, float t)
{
    return GetBezierCurvePointTangent(controlPoints, numControlPoints, t, NULL);
}

Vector2 GetBezierCurvePointTangent(Vector2* controlPoints, int numControlPoints, float t, Vector2* outTangent)
{
    int curveOrder = numControlPoints - 1; // A quadratic (order = 2) bezier curve has 3 points, a cubic (order = 3) curve has 4 points, ...

    // For (f + g)^n, the exponent of f goes from n to 0, while the exponent of g goes from 0 to n
    float f = 1 - t;
    float g = t;
    int fExponent = curveOrder;
    int gExponent = 0;

    Vector2 ret = { 0 };
    Vector2 tangent = { 0 };
    for (int i = 0; i < numControlPoints; ++i)
    {
        float coefficient = powf(f, fExponent) * powf(g, gExponent) * pascalTriangle[curveOrder][i];
        ret = Vector2Add(ret, Vector2Scale(controlPoints[i], coefficient));

        if (i < numControlPoints - 1)
        {
            Vector2 edge = Vector2Subtract(controlPoints[i + 1], controlPoints[i]);
            tangent = Vector2Add(tangent, Vector2Scale(edge, coefficient));
        }

        --fExponent;
        ++gExponent;
    }

    if (outTangent != NULL)
    {
        *outTangent = tangent;
    }

    return ret;
}

void GetBezierCurvePoints(Vector2* controlPoints, int numControlPoints, int numSteps, Vector2* outPoints)
{
    GetBezierCurvePointsTangents(controlPoints, numControlPoints, numSteps, outPoints, NULL);
}

void GetBezierCurvePointsTangents(Vector2* controlPoints, int numControlPoints, int numSteps, Vector2* outPoints, Vector2* outTangents)
{
    float stepAmount = 1.0f / numSteps;
    for (int i = 0; i <= numSteps; ++i)
    {
        float t = i * stepAmount;

        Vector2 tangent;
        Vector2 point = GetBezierCurvePointTangent(controlPoints, numControlPoints, t, &tangent);

        if (outPoints != NULL)
        {
            outPoints[i] = point;
        }
        if ((outTangents != NULL) && (i < numSteps))
        {
            outTangents[i] = tangent;
        }
    }
}


// ----------------------------------------
// Internal Functions
// ----------------------------------------

bool Ignis_Bezier_Init()
{
    // --- Generate pascal triangle for bezier curve ---
    pascalTriangle[0][0] = 1;
    pascalTriangle[1][0] = pascalTriangle[1][1] = 1;
    for (int i = 2; i < MAX_PASCAL_TRIANGLE_SIZE; ++i)
    {
        for (int j = 0; j <= i; ++j)
        {
            if ((j == 0) || (j == i))
            {
                pascalTriangle[i][j] = 1;
            }
            else
            {
                pascalTriangle[i][j] = pascalTriangle[i - 1][j - 1] + pascalTriangle[i - 1][j];
            }
        }
    }

    return true;
}

void Ignis_Bezier_Cleanup()
{
}

