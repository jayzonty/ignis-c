#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Enum of codes representing possible results of an operation
typedef enum
{
    DYNAMIC_ARRAY_OP_SUCCESS = 0,
    DYNAMIC_ARRAY_OP_ARG_NULL = 1,
    DYNAMIC_ARRAY_OP_NOT_INITIALIZED = 2,
    DYNAMIC_ARRAY_OP_NOT_ENOUGH_MEMORY = 3
} DynamicArrayOpResult;

// Dynamic array
typedef struct
{
    void *data;
} DynamicArray;

/**
 * @brief Initializes the specified dynamic array. If the dynamic array has already been
 * initialized before, this function tries to repurpose the dynamic array to fit the
 * new requirements.
 * @param[in] array Pointer to the dynamic array to initialize
 * @param[in] dataSize Size of the data the specified dynamic array will hold
 * @return DynamicArrayOpResult code representing the result of the operation
 */
DynamicArrayOpResult DynamicArrayInit(DynamicArray *array, size_t dataSize);

/**
 * @brief Frees the memory allocated for the specified dynamic array.
 * @param[in] array Pointer to the dynamic array to free
 * @return DynamicArrayOpResult code representing the result of the operation
 */
DynamicArrayOpResult DynamicArrayFree(DynamicArray *array);

/**
 * @brief Appends the specified value to the back of the dynamic array.
 * @param[in] array Pointer to the dynamic array to append to
 * @return DynmaicArrayOpResult code representing the result of the operation
 */
DynamicArrayOpResult DynamicArrayAdd(DynamicArray *array, void *val);

/**
 * @brief Resets the size of the specified dynamic array back to 0. Previous elements are retained in memory.
 * @param[in] array Pointer to the dynamic array to clear
 * @return DynamicArrayOpResult code representing the result of the operation
 */
DynamicArrayOpResult DynamicArrayClear(DynamicArray *array);

/**
 * @brief Gets the size of the specified dynamic array.
 * @param[in] array Pointer to the dynamic array to get the size of
 * @return Size of the dynamic array
 */
size_t DynamicArrayGetSize(DynamicArray *array);

/**
 * @brief Gets the element at the specified index.
 * @param[in] array Pointer to the dynamic array
 * @param[in] index Index of the element to get
 * @return Pointer to the element at the specified index. Returns NULL if the index is not valid.
 */
void* DynamicArrayGetElementAt(DynamicArray *array, size_t index);

/**
 * @brief Gets the pointer to the specified dynamic array's underlying data.
 * @param[in] array Pointer to the dynamic array
 * @return Pointer to the dynamic array's underlying data
 */
void* DynamicArrayGetData(DynamicArray *array);

#endif // DYNAMIC_ARRAY_H

