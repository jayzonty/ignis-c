#include "ignis_vector.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_INITIAL_CAPACITY 2

// Struct representing the internal data of the vector
typedef struct
{
    size_t dataSize;
    size_t capacity;
    size_t size;
    void *data;
} VectorData;

/**
 * @brief Initializes the specified vector. If the specified vector has already been
 * initialized before, this function tries to repurpose the specified vector to fit the
 * new requirements.
 * @param[in] v Pointer to the vector to initialize
 * @param[in] dataSize Size of the data the specified vector will hold
 * @return VectorOpResult code representing the result of the operation
 */
VectorOpResult VectorInit(Vector *v, size_t dataSize)
{
    if (v == NULL)
    {
        return VECTOR_OP_ARG_NULL;
    }

    VectorData *vectorData = (VectorData*)v->data;
    if (vectorData == NULL)
    {
        vectorData = malloc(sizeof(VectorData));
        vectorData->dataSize = 0;
        vectorData->capacity = 0;
        vectorData->size = 0;
        vectorData->data = NULL;
    }

    size_t capacity = vectorData->capacity;
    if (capacity == 0)
    {
        capacity = DEFAULT_INITIAL_CAPACITY;
    }

    void *data = vectorData->data;
    if (data != NULL)
    {
        if (vectorData->dataSize != dataSize)
        {
            void *newData = realloc(data, dataSize * capacity);
            if (newData == NULL)
            {
                return VECTOR_OP_NOT_ENOUGH_MEMORY;
            }

            data = newData;
        }
    }
    else
    {
        data = malloc(dataSize * capacity);
    }

    vectorData->dataSize = dataSize;
    vectorData->capacity = capacity;
    vectorData->size = 0;
    vectorData->data = data;

    v->data = vectorData;

    return VECTOR_OP_SUCCESS;
}

/**
 * @brief Frees the specified vector.
 * @param[in] v Pointer to the vector to free
 * @return VectorOpResult code representing the result of the operation
 */
VectorOpResult VectorFree(Vector *v)
{
    if (v == NULL)
    {
        return VECTOR_OP_ARG_NULL;
    }
    if (v->data == NULL)
    {
        return VECTOR_OP_VECTOR_NOT_INITIALIZED;
    }

    VectorData *vectorData = (VectorData*)v->data;
    free(vectorData->data);
    vectorData->dataSize = 0;
    vectorData->size = 0;
    vectorData->capacity = 0;
    vectorData->data = NULL;

    free(v->data);
    v->data = NULL;

    return VECTOR_OP_SUCCESS;
}

/**
 * @brief Appends the specified value to the back of the vector.
 * @param[in] v Pointer to the vector to append to
 * @return VectorOpResult code representing the result of the operation
 */
VectorOpResult VectorPushBack(Vector *v, void *val)
{
    if ((v == NULL) || (val == NULL))
    {
        return VECTOR_OP_ARG_NULL;
    }
    if (v->data == NULL)
    {
        return VECTOR_OP_VECTOR_NOT_INITIALIZED;
    }

    VectorData *vectorData = (VectorData*)v->data;
    if (vectorData->size == vectorData->capacity)
    {
        size_t newCapacity = vectorData->capacity * 2;
        void *newData = realloc(vectorData->data, newCapacity * vectorData->dataSize);
        if (newData != NULL)
        {
            vectorData->capacity = newCapacity;
            vectorData->data = newData;
        }
        else
        {
            return VECTOR_OP_NOT_ENOUGH_MEMORY;
        }
    }

    void *dst = vectorData->data + vectorData->size * vectorData->dataSize;
    memcpy(dst, val, vectorData->dataSize);
    ++vectorData->size;

    return VECTOR_OP_SUCCESS;
}

/**
 * @brief Resets the size of the specified vector back to 0. Previous elements are retained in memory.
 * @param[in] v Pointer to the vector to clear
 * @return VectorOpResult code representing the result of the operation
 */
VectorOpResult VectorClear(Vector *v)
{
    if (v == NULL)
    {
        return VECTOR_OP_ARG_NULL;
    }
    if (v->data == NULL)
    {
        return VECTOR_OP_VECTOR_NOT_INITIALIZED;
    }

    VectorData *vectorData = (VectorData*)v->data;
    vectorData->size = 0;
    return VECTOR_OP_SUCCESS;
}

/**
 * @brief Gets the size of the specified vector.
 * @param[in] v Pointer to the vector to get the size of
 * @return Size of the vector
 */
size_t VectorGetSize(Vector *v)
{
    if ((v == NULL) || (v->data == NULL))
    {
        return 0;
    }

    VectorData *vectorData = (VectorData*)v->data;
    return vectorData->size;
}

/**
 * @brief Gets the element at the specified index.
 * @param[in] v Pointer to the vector
 * @param[in] index Index of the element to get
 * @return Pointer to the element at the specified index. Returns NULL if the index is not valid.
 */
void* VectorGetElementAt(Vector *v, size_t index)
{
    if ((v == NULL) || (v->data == NULL))
    {
        return NULL;
    }

    VectorData *vectorData = (VectorData*)v->data;
    if (index < vectorData->size)
    {
        void *ret = vectorData->data + index * vectorData->dataSize;
        return ret;
    }

    return NULL;
}

/**
 * @brief Gets the pointer to the specified vector's underlying data.
 * @param[in] v Pointer to the vector
 * @return Pointer to the vector's underlying data
 */
void* VectorGetData(Vector *v)
{
    if ((v == NULL) || (v->data == NULL))
    {
        return NULL;
    }

    VectorData *vectorData = (VectorData*)v->data;
    return vectorData->data;
}
