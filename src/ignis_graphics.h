#ifndef IGNIS_GRAPHICS_H
#define IGNIS_GRAPHICS_H

#include "ignis/ignis.h"
#include "ignis/ignis_math.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdbool.h>
#include <stdint.h>

bool Ignis_Graphics_Init(int windowWidth, int windowHeight, GLFWwindow* windowHandle);
void Ignis_Graphics_WaitDeviceIdle(void);
void Ignis_Graphics_Cleanup(void);

void Ignis_Graphics_SetClearColor(Color color);

void Ignis_Graphics_BeginDrawing(void);
void Ignis_Graphics_EndDrawing(void);
void Ignis_Graphics_Draw(void);

void Ignis_Graphics_DrawCircle(Vector2 position, float radius, Vector2 origin, Color color, float depth);
void Ignis_Graphics_DrawLine(Vector2 start, Vector2 end, float width, Color color, float depth);
void Ignis_Graphics_DrawBezierCurve(Vector2* controlPoints, int numControlPoints, int numSteps, Color color, float depth);
void Ignis_Graphics_DrawTexture(Vector2 position, Vector2 size, Vector2 origin, Texture texture, Rect uvRect, Color tint, float depth);

int Ignis_Graphics_LoadTextureFromData(const uint8_t *pixels, int width, int height, int numChannels);
void Ignis_Graphics_UnloadTexture(int textureId);

#endif // IGNIS_GRAPHICS_H

