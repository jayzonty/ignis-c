#ifndef IGNIS_BEZIER_INTERNAL_H
#define IGNIS_BEZIER_INTERNAL_H

#include <stdbool.h>

/**
 * @brief Initializes the bezier curve module
 * @return True if the initialization was successful. False otherwise
 */
bool Ignis_Bezier_Init();

/**
 * @brief Cleans up resources used by the bezier curve module
 */
void Ignis_Bezier_Cleanup();

#endif // IGNIS_BEZIER_INTERNAL_H
