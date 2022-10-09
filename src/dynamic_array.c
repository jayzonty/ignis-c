#include "dynamic_array.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_INITIAL_CAPACITY 2

// Struct representing the internal data of the dynamic array
typedef struct
{
    size_t dataSize;
    size_t capacity;
    size_t size;
    void *data;
} DynamicArrayData;

/**
 * @brief Initializes the specified dynamic array. If the dynamic array has already been
 * initialized before, this function tries to repurpose the dynamic array to fit the
 * new requirements.
 * @param[in] array Pointer to the dynamic array to initialize
 * @param[in] dataSize Size of the data the specified dynamic array will hold
 * @return DynamicArrayOpResult code representing the result of the operation
 */
DynamicArrayOpResult DynamicArrayInit(DynamicArray *array, size_t dataSize)
{
    if (array == NULL)
    {
        return DYNAMIC_ARRAY_OP_ARG_NULL;
    }

    DynamicArrayData *dynamicArrayData = (DynamicArrayData*)array->data;
    if (dynamicArrayData == NULL)
    {
        dynamicArrayData = malloc(sizeof(DynamicArrayData));
        dynamicArrayData->dataSize = 0;
        dynamicArrayData->capacity = 0;
        dynamicArrayData->size = 0;
        dynamicArrayData->data = NULL;
    }

    size_t capacity = dynamicArrayData->capacity;
    if (capacity == 0)
    {
        capacity = DEFAULT_INITIAL_CAPACITY;
    }

    void *data = dynamicArrayData->data;
    if (data != NULL)
    {
        if (dynamicArrayData->dataSize != dataSize)
        {
            void *newData = realloc(data, dataSize * capacity);
            if (newData == NULL)
            {
                return DYNAMIC_ARRAY_OP_NOT_ENOUGH_MEMORY;
            }

            data = newData;
        }
    }
    else
    {
        data = malloc(dataSize * capacity);
    }

    dynamicArrayData->dataSize = dataSize;
    dynamicArrayData->capacity = capacity;
    dynamicArrayData->size = 0;
    dynamicArrayData->data = data;

    array->data = dynamicArrayData;

    return DYNAMIC_ARRAY_OP_SUCCESS;
}

/**
 * @brief Frees the memory allocated for the specified dynamic array.
 * @param[in] array Pointer to the dynamic array to free
 * @return DynamicArrayOpResult code representing the result of the operation
 */
DynamicArrayOpResult DynamicArrayFree(DynamicArray *array)
{
    if (array == NULL)
    {
        return DYNAMIC_ARRAY_OP_ARG_NULL;
    }
    if (array->data == NULL)
    {
        return DYNAMIC_ARRAY_OP_NOT_INITIALIZED;
    }

    DynamicArrayData *dynamicArrayData = (DynamicArrayData*)array->data;
    free(dynamicArrayData->data);
    dynamicArrayData->dataSize = 0;
    dynamicArrayData->size = 0;
    dynamicArrayData->capacity = 0;
    dynamicArrayData->data = NULL;

    free(array->data);
    array->data = NULL;

    return DYNAMIC_ARRAY_OP_SUCCESS;
}

/**
 * @brief Appends the specified value to the back of the dynamic array.
 * @param[in] array Pointer to the dynamic array to append to
 * @return DynmaicArrayOpResult code representing the result of the operation
 */
DynamicArrayOpResult DynamicArrayAdd(DynamicArray *array, void *val)
{
    if ((array == NULL) || (val == NULL))
    {
        return DYNAMIC_ARRAY_OP_ARG_NULL;
    }
    if (array->data == NULL)
    {
        return DYNAMIC_ARRAY_OP_NOT_INITIALIZED;
    }

    DynamicArrayData *dynamicArrayData = (DynamicArrayData*)array->data;
    if (dynamicArrayData->size == dynamicArrayData->capacity)
    {
        size_t newCapacity = dynamicArrayData->capacity * 2;
        void *newData = realloc(dynamicArrayData->data, newCapacity * dynamicArrayData->dataSize);
        if (newData != NULL)
        {
            dynamicArrayData->capacity = newCapacity;
            dynamicArrayData->data = newData;
        }
        else
        {
            return DYNAMIC_ARRAY_OP_NOT_ENOUGH_MEMORY;
        }
    }

    void *dst = dynamicArrayData->data + dynamicArrayData->size * dynamicArrayData->dataSize;
    memcpy(dst, val, dynamicArrayData->dataSize);
    ++dynamicArrayData->size;

    return DYNAMIC_ARRAY_OP_SUCCESS;
}

/**
 * @brief Resets the size of the specified dynamic array back to 0. Previous elements are retained in memory.
 * @param[in] array Pointer to the dynamic array to clear
 * @return DynamicArrayOpResult code representing the result of the operation
 */
DynamicArrayOpResult DynamicArrayClear(DynamicArray *array)
{
    if (array == NULL)
    {
        return DYNAMIC_ARRAY_OP_ARG_NULL;
    }
    if (array->data == NULL)
    {
        return DYNAMIC_ARRAY_OP_NOT_INITIALIZED;
    }

    DynamicArrayData *dynamicArrayData = (DynamicArrayData*)array->data;
    dynamicArrayData->size = 0;
    return DYNAMIC_ARRAY_OP_SUCCESS;
}

/**
 * @brief Gets the size of the specified dynamic array.
 * @param[in] array Pointer to the dynamic array to get the size of
 * @return Size of the dynamic array
 */
size_t DynamicArrayGetSize(DynamicArray *array)
{
    if ((array == NULL) || (array->data == NULL))
    {
        return 0;
    }

    DynamicArrayData *dynamicArrayData = (DynamicArrayData*)array->data;
    return dynamicArrayData->size;
}

/**
 * @brief Gets the element at the specified index.
 * @param[in] array Pointer to the dynamic array
 * @param[in] index Index of the element to get
 * @return Pointer to the element at the specified index. Returns NULL if the index is not valid.
 */
void* DynamicArrayGetElementAt(DynamicArray *array, size_t index)
{
    if ((array == NULL) || (array->data == NULL))
    {
        return NULL;
    }

    DynamicArrayData *dynamicArrayData = (DynamicArrayData*)array->data;
    if (index < dynamicArrayData->size)
    {
        void *ret = dynamicArrayData->data + index * dynamicArrayData->dataSize;
        return ret;
    }

    return NULL;
}

/**
 * @brief Gets the pointer to the specified dynamic array's underlying data.
 * @param[in] array Pointer to the dynamic array
 * @return Pointer to the dynamic array's underlying data
 */
void* DynamicArrayGetData(DynamicArray *array)
{
    if ((array == NULL) || (array->data == NULL))
    {
        return NULL;
    }

    DynamicArrayData *dynamicArrayData = (DynamicArrayData*)array->data;
    return dynamicArrayData->data;
}

