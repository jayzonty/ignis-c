#include "ignis/ignis_math.h"

#include <math.h>

// ----------------------------------------
// Macros
// ----------------------------------------
#define CLAMP_MACRO(val, min, max) if (val < min) return min; if (val > max) return max; return val;
#define MIN_MACRO(val1, val2) if (val1 <= val2) return val1; return val2;
#define MAX_MACRO(val1, val2) if (val1 >= val2) return val1; return val2;

Vector2 Vector2Add(Vector2 a, Vector2 b)
{
    Vector2 ret = { a.x + b.x, a.y + b.y };
    return ret;
}

Vector2 Vector2Subtract(Vector2 a, Vector2 b)
{
    Vector2 ret = { a.x - b.x, a.y - b.y };
    return ret;
}

Vector2 Vector2Scale(Vector2 v, float s)
{
    Vector2 ret = { v.x * s, v.y * s };
    return ret;
}

float Vector2DotProduct(Vector2 a, Vector2 b)
{
    return a.x * b.x + a.y * b.y;
}

Vector2 Vector2Project(Vector2 a, Vector2 b)
{
    Vector2 ret = { 0 };

    float numerator = Vector2DotProduct(a, b);
    float denominator = Vector2DotProduct(b, b);
    if (denominator > 0.0f)
    {
        ret = Vector2Scale(b, numerator / denominator);
    }

    return ret;
}

Vector2 Vector2Reflect(Vector2 incident, Vector2 normal)
{
    Vector2 proj = Vector2Project(incident, normal);
    proj = Vector2Scale(proj, 2.0f);
    return Vector2Subtract(incident, proj);
}

float Vector2SquaredMagnitude(Vector2 v)
{
    return Vector2DotProduct(v, v);
}

float Vector2Magnitude(Vector2 v)
{
    float squaredMagnitude = Vector2SquaredMagnitude(v);
    return sqrtf(squaredMagnitude);
}

Vector2 Vector2Normalize(Vector2 v)
{
    float magnitude = Vector2Magnitude(v);
    if (magnitude > 0.0f)
    {
        return Vector2Scale(v, 1.0f / magnitude);
    }
    return v;
}

float Vector2SquaredDistance(Vector2 a, Vector2 b)
{
    Vector2 D = Vector2Subtract(a, b);
    return Vector2SquaredMagnitude(D);
}

float Vector2Distance(Vector2 a, Vector2 b)
{
    Vector2 D = Vector2Subtract(a, b);
    return Vector2Magnitude(D);
}

Vector2 Vector2Direction(Vector2 source, Vector2 destination)
{
    Vector2 ret = Vector2Subtract(destination, source);
    ret = Vector2Normalize(ret);
    return ret;
}

int Clamp_Int(int val, int min, int max)
{
    CLAMP_MACRO(val, min, max)
}

uint32_t Clamp_Uint32(uint32_t val, uint32_t min, uint32_t max)
{
    CLAMP_MACRO(val, min, max)
}

float Clamp_Float(float val, float min, float max)
{
    CLAMP_MACRO(val, min, max)
}

double Clamp_Double(double val, double min, double max)
{
    CLAMP_MACRO(val, min, max)
}

int Min_Int(int a, int b)
{
    MIN_MACRO(a, b)
}

uint32_t Min_Uint32(uint32_t a, uint32_t b)
{
    MIN_MACRO(a, b)
}

float Min_Float(float a, float b)
{
    MIN_MACRO(a, b)
}

double Min_Double(double a, double b)
{
    MIN_MACRO(a, b)
}

uint32_t Max_Int(uint32_t a, uint32_t b)
{
    MAX_MACRO(a, b)
}

uint32_t Max_Uint32(uint32_t a, uint32_t b)
{
    MAX_MACRO(a, b)
}

float Max_Float(float a, float b)
{
    MAX_MACRO(a, b)
}

double Max_Double(double a, double b)
{
    MAX_MACRO(a, b)
}

float Lerp_Float(float a, float b, float t)
{
    return a * (1.0f - t) + b * t;
}

double Lerp_Double(double a, double b, double t)
{
    return a * (1.0f - t) + b * t;
}

