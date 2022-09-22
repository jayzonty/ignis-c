#ifndef ENGINE_VECTOR_H
#define ENGINE_VECTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Enum of codes representing possible results of an operation
typedef enum
{
    VECTOR_OP_SUCCESS = 0,
    VECTOR_OP_ARG_NULL = 1,
    VECTOR_OP_VECTOR_NOT_INITIALIZED = 2,
    VECTOR_OP_NOT_ENOUGH_MEMORY = 3
} VectorOpResult;

// Vector
typedef struct
{
    void *data;
} Vector;

/**
 * @brief Initializes the specified vector. If the specified vector has already been
 * initialized before, this function tries to repurpose the specified vector to fit the
 * new requirements.
 * @param[in] v Pointer to the vector to initialize
 * @param[in] dataSize Size of the data the specified vector will hold
 * @return VectorOpResult code representing the result of the operation
 */
VectorOpResult VectorInit(Vector *v, size_t dataSize);

/**
 * @brief Frees the specified vector.
 * @param[in] v Pointer to the vector to free
 * @return VectorOpResult code representing the result of the operation
 */
VectorOpResult VectorFree(Vector *v);

/**
 * @brief Appends the specified value to the back of the vector.
 * @param[in] v Pointer to the vector to append to
 * @return VectorOpResult code representing the result of the operation
 */
VectorOpResult VectorPushBack(Vector *v, void *val);

/**
 * @brief Resets the size of the specified vector back to 0. Previous elements are retained in memory.
 * @param[in] v Pointer to the vector to clear
 * @return VectorOpResult code representing the result of the operation
 */
VectorOpResult VectorClear(Vector *v);

/**
 * @brief Gets the size of the specified vector.
 * @param[in] v Pointer to the vector to get the size of
 * @return Size of the vector
 */
size_t VectorGetSize(Vector *v);

/**
 * @brief Gets the element at the specified index.
 * @param[in] v Pointer to the vector
 * @param[in] index Index of the element to get
 * @return Pointer to the element at the specified index. Returns NULL if the index is not valid.
 */
void* VectorGetElementAt(Vector *v, size_t index);

/**
 * @brief Gets the pointer to the specified vector's underlying data.
 * @param[in] v Pointer to the vector
 * @return Pointer to the vector's underlying data
 */
void* VectorGetData(Vector *v);

#endif // ENGINE_VECTOR_H
