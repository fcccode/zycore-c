/***************************************************************************************************

  Zyan Core Library (Zycore-C)

  Original Author : Florian Bernd

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

***************************************************************************************************/

#include <Zycore/LibC.h>
#include <Zycore/Vector.h>

/* ============================================================================================== */
/* Internal constants                                                                             */
/* ============================================================================================== */

#define ZYAN_VECTOR_MIN_CAPACITY     1
#define ZYAN_VECTOR_GROWTH_FACTOR    2.00f
#define ZYAN_VECTOR_SHRINK_THRESHOLD 0.25f

/* ============================================================================================== */
/* Internal macros                                                                                */
/* ============================================================================================== */

/**
 * @brief   Checks, if the passed vector should grow.
 *
 * @param   size        The desired size of the vector.
 * @param   capacity    The current capacity of the vector.
 *
 * @return  `ZYAN_TRUE`, if the vector should grow or `ZYAN_FALSE`, if not.
 */
#define ZYAN_VECTOR_SHOULD_GROW(size, capacity) \
    ((size) > (capacity))

/**
 * @brief   Checks, if the passed vector should shrink.
 *
 * @param   size        The desired size of the vector.
 * @param   capacity    The current capacity of the vector.
 * @param   threshold   The shrink threshold.
 *
 * @return  `ZYAN_TRUE`, if the vector should shrink or `ZYAN_FALSE`, if not.
 */
#define ZYAN_VECTOR_SHOULD_SHRINK(size, capacity, threshold) \
    ((size) < (capacity) * (threshold))

/**
 * @brief   Returns the offset of the element at the given `index`.
 *
 * @param   vector  A pointer to the `ZydisVector` instance.
 * @param   index   The element index.
 *
 * @return  The offset of the element at the given `index`.
 */
#define ZYAN_VECTOR_OFFSET(vector, index) \
    ((void*)((ZyanU8*)(vector)->data + ((index) * (vector)->element_size)))

/* ============================================================================================== */
/* Internal functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Helper functions                                                                               */
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief   Reallocates the internal buffer of the vector.
 *
 * @param   vector      A pointer to the `ZydisVector` instance.
 * @param   capacity    The new capacity.
 *
 * @return  A zycore status code.
 */
static ZyanStatus ZyanVectorReallocate(ZyanVector* vector, ZyanUSize capacity)
{
    ZYAN_ASSERT(vector);
    ZYAN_ASSERT(vector->capacity >= ZYAN_VECTOR_MIN_CAPACITY);
    ZYAN_ASSERT(vector->element_size);
    ZYAN_ASSERT(vector->data);

    if (!vector->allocator)
    {
        if (vector->capacity < capacity)
        {
            return ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE;
        }
        return ZYAN_STATUS_SUCCESS;
    }

    ZYAN_ASSERT(vector->allocator);
    ZYAN_ASSERT(vector->allocator->reallocate);

    if (capacity < ZYAN_VECTOR_MIN_CAPACITY)
    {
        if (vector->capacity > ZYAN_VECTOR_MIN_CAPACITY)
        {
            capacity = ZYAN_VECTOR_MIN_CAPACITY;
        } else
        {
            return ZYAN_STATUS_SUCCESS;
        }
    }

    vector->capacity = capacity;
    ZYAN_CHECK(vector->allocator->reallocate(vector->allocator, &vector->data,
        vector->element_size, vector->capacity));

    return ZYAN_STATUS_SUCCESS;
}

/**
 * @brief   Shifts all elements starting at the specified `index` by the amount of `count` to the
 *          left.
 *
 * @param   vector  A pointer to the `ZydisVector` instance.
 * @param   index   The start index.
 * @param   count   The amount of shift operations.
 *
 * @return  A zycore status code.
 */
static ZyanStatus ZyanVectorShiftLeft(ZyanVector* vector, ZyanUSize index, ZyanUSize count)
{
    ZYAN_ASSERT(vector);
    ZYAN_ASSERT(vector->element_size);
    ZYAN_ASSERT(vector->data);
    ZYAN_ASSERT(index > 0);
    ZYAN_ASSERT(count > 0);
    ZYAN_ASSERT((ZyanISize)index - (ZyanISize)count >= 0);

    void* const source   = ZYAN_VECTOR_OFFSET(vector, index + count);
    void* const dest     = ZYAN_VECTOR_OFFSET(vector, index);
    const ZyanUSize size = (vector->size - index) * vector->element_size;
    ZYAN_MEMMOVE(dest, source, size);

    return ZYAN_STATUS_SUCCESS;
}

/**
 * @brief   Shifts all elements starting at the specified `index` by the amount of `count` to the
 *          right.
 *
 * @param   vector  A pointer to the `ZydisVector` instance.
 * @param   index   The start index.
 * @param   count   The amount of shift operations.
 *
 * @return  A zycore status code.
 */
static ZyanStatus ZyanVectorShiftRight(ZyanVector* vector, ZyanUSize index, ZyanUSize count)
{
    ZYAN_ASSERT(vector);
    ZYAN_ASSERT(vector->element_size);
    ZYAN_ASSERT(vector->data);
    ZYAN_ASSERT(count > 0);
    ZYAN_ASSERT(vector->size + count <= vector->capacity);

    void* const source   = ZYAN_VECTOR_OFFSET(vector, index);
    void* const dest     = ZYAN_VECTOR_OFFSET(vector, index + count);
    const ZyanUSize size = (vector->size - index) * vector->element_size;
    ZYAN_MEMMOVE(dest, source, size);

    return ZYAN_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Constructor and destructor                                                                     */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyanVectorInit(ZyanVector* vector, ZyanUSize element_size, ZyanUSize capacity)
{
    return ZyanVectorInitEx(vector,  element_size, capacity, ZyanAllocatorDefault(),
        ZYAN_VECTOR_GROWTH_FACTOR, ZYAN_VECTOR_SHRINK_THRESHOLD);
}

ZyanStatus ZyanVectorInitEx(ZyanVector* vector, ZyanUSize element_size, ZyanUSize capacity,
    ZyanAllocator* allocator, float growth_factor, float shrink_threshold)
{
    if (!vector || !element_size || !allocator || (growth_factor < 1.0f) ||
        (shrink_threshold < 0.0f) || (shrink_threshold > 1.0f))
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    ZYAN_ASSERT(allocator->allocate);

    vector->allocator        = allocator;
    vector->growth_factor    = growth_factor;
    vector->shrink_threshold = shrink_threshold;
    vector->size             = 0;
    vector->capacity         = ZYAN_MAX(ZYAN_VECTOR_MIN_CAPACITY, capacity);
    vector->element_size     = element_size;
    vector->data             = ZYAN_NULL;

    return allocator->allocate(vector->allocator, &vector->data, vector->element_size,
        vector->capacity);
}

ZyanStatus ZyanVectorInitBuffer(ZyanVector* vector, ZyanUSize element_size,
    void* buffer, ZyanUSize capacity)
{
    if (!vector || !element_size || !buffer || !capacity)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    vector->allocator        = ZYAN_NULL;
    vector->growth_factor    = 1.0f;
    vector->shrink_threshold = 0.0f;
    vector->size             = 0;
    vector->capacity         = capacity;
    vector->element_size     = element_size;
    vector->data             = buffer;

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyanVectorDestroy(ZyanVector* vector)
{
    if (!vector)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    ZYAN_ASSERT(vector->element_size);
    ZYAN_ASSERT(vector->data);

    if (vector->allocator && vector->capacity)
    {
        ZYAN_ASSERT(vector->allocator->deallocate);
        ZYAN_CHECK(vector->allocator->deallocate(vector->allocator, vector->data,
            vector->element_size, vector->capacity));
    }

    return ZYAN_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */
/* Lookup                                                                                         */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyanVectorGet(const ZyanVector* vector, ZyanUSize index, void** element)
{
    if (!vector || !element)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    if (index >= vector->size)
    {
        return ZYAN_STATUS_OUT_OF_RANGE;
    }

    ZYAN_ASSERT(vector->element_size);
    ZYAN_ASSERT(vector->data);

    *element = ZYAN_VECTOR_OFFSET(vector, index);

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyanVectorGetConst(const ZyanVector* vector, ZyanUSize index, const void** element)
{
    if (!vector || !element)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    if (index >= vector->size)
    {
        return ZYAN_STATUS_OUT_OF_RANGE;
    }

    ZYAN_ASSERT(vector->element_size);
    ZYAN_ASSERT(vector->data);

    *element = (const void*)ZYAN_VECTOR_OFFSET(vector, index);

    return ZYAN_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */
/* Assignment                                                                                     */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyanVectorAssign(ZyanVector* vector, ZyanUSize index, const void* element)
{
    if (!vector || !element)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    if (index >= vector->size)
    {
        return ZYAN_STATUS_OUT_OF_RANGE;
    }

    ZYAN_ASSERT(vector->element_size);
    ZYAN_ASSERT(vector->data);

    void* const offset = ZYAN_VECTOR_OFFSET(vector, index);
    ZYAN_MEMCPY(offset, element, vector->element_size);

    return ZYAN_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */
/* Insertion                                                                                      */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyanVectorPush(ZyanVector* vector, const void* element)
{
    if (!vector || !element)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    ZYAN_ASSERT(vector->element_size);
    ZYAN_ASSERT(vector->data);

    if (ZYAN_VECTOR_SHOULD_GROW(vector->size + 1, vector->capacity))
    {
        ZYAN_CHECK(ZyanVectorReallocate(vector,
            ZYAN_MAX(1, (ZyanUSize)((vector->size + 1) * vector->growth_factor))));
    }

    void* const offset = ZYAN_VECTOR_OFFSET(vector, vector->size);
    ZYAN_MEMCPY(offset, element, vector->element_size);

    ++vector->size;

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyanVectorInsert(ZyanVector* vector, ZyanUSize index, const void* element)
{
    return ZyanVectorInsertElements(vector, index, element, 1);
}

ZyanStatus ZyanVectorInsertElements(ZyanVector* vector, ZyanUSize index, const void* elements,
    ZyanUSize count)
{
    if (!vector || !elements || !count)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    if (index > vector->size)
    {
        return ZYAN_STATUS_OUT_OF_RANGE;
    }

    ZYAN_ASSERT(vector->element_size);
    ZYAN_ASSERT(vector->data);

    if (ZYAN_VECTOR_SHOULD_GROW(vector->size + count, vector->capacity))
    {
        ZYAN_CHECK(ZyanVectorReallocate(vector,
            ZYAN_MAX(1, (ZyanUSize)((vector->size + count) * vector->growth_factor))));
    }

    if (index < vector->size)
    {
        ZYAN_CHECK(ZyanVectorShiftRight(vector, index, count));
    }

    void* const offset = ZYAN_VECTOR_OFFSET(vector, index);
    ZYAN_MEMCPY(offset, elements, count * vector->element_size);
    vector->size += count;

    return ZYAN_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */
/* Deletion                                                                                       */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyanVectorDelete(ZyanVector* vector, ZyanUSize index)
{
    return ZyanVectorDeleteElements(vector, index, 1);
}

ZyanStatus ZyanVectorDeleteElements(ZyanVector* vector, ZyanUSize index, ZyanUSize count)
{
    if (!vector || !count)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    if (index >= vector->size)
    {
        return ZYAN_STATUS_OUT_OF_RANGE;
    }

    if (index < vector->size - 1)
    {
        ZYAN_CHECK(ZyanVectorShiftLeft(vector, index , count));
    }

    vector->size -= count;
    if (ZYAN_VECTOR_SHOULD_SHRINK(vector->size, vector->capacity, vector->shrink_threshold))
    {
        return ZyanVectorReallocate(vector,
            ZYAN_MAX(1, (ZyanUSize)(vector->size * vector->growth_factor)));
    }

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyanVectorPop(ZyanVector* vector)
{
    if (!vector)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    if (vector->size == 0)
    {
        return ZYAN_STATUS_OUT_OF_RANGE;
    }

    --vector->size;
    if (ZYAN_VECTOR_SHOULD_SHRINK(vector->size, vector->capacity, vector->shrink_threshold))
    {
        return ZyanVectorReallocate(vector,
            ZYAN_MAX(1, (ZyanUSize)(vector->size * vector->growth_factor)));
    }

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyanVectorClear(ZyanVector* vector)
{
    return ZyanVectorResize(vector, 0);
}

/* ---------------------------------------------------------------------------------------------- */
/* Memory management                                                                              */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyanVectorResize(ZyanVector* vector, ZyanUSize size)
{
    if (!vector)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    if (ZYAN_VECTOR_SHOULD_GROW(size, vector->capacity) ||
        ZYAN_VECTOR_SHOULD_SHRINK(size, vector->capacity, vector->shrink_threshold))
    {
        ZYAN_CHECK(ZyanVectorReallocate(vector, (ZyanUSize)(size * vector->growth_factor)));
    };

    vector->size = size;

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyanVectorReserve(ZyanVector* vector, ZyanUSize capacity)
{
    if (!vector)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    if (capacity > vector->capacity)
    {
        ZYAN_CHECK(ZyanVectorReallocate(vector, capacity));
    }

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyanVectorShrinkToFit(ZyanVector* vector)
{
    return ZyanVectorReallocate(vector, vector->size);
}

/* ---------------------------------------------------------------------------------------------- */
/* Information                                                                                    */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyanVectorSize(const ZyanVector* vector, ZyanUSize* size)
{
    if (!vector)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    *size = vector->size;

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyanVectorCapacity(const ZyanVector* vector, ZyanUSize* capacity)
{
    if (!vector)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    *capacity = vector->capacity;

    return ZYAN_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
