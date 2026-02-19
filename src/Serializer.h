#ifndef SERIALIZER_H
#define SERIALIZER_H

#include <Arduino.h>
#include <cstring>

/**
 * @class Serializer
 * @brief Abstract base for data serialization
 * 
 * Allows different serialization formats while keeping
 * the core library generic.
 */
class Serializer
{
public:
    virtual ~Serializer() = default;

    /**
     * @brief Serialize object to byte array
     * @param data Pointer to object
     * @param size Size of object
     * @param buffer Output buffer
     * @param bufferSize Buffer capacity
     * @return Number of bytes written, or 0 on failure
     */
    virtual size_t serialize(const void* data, size_t size,
                             uint8_t* buffer, size_t bufferSize) = 0;

    /**
     * @brief Deserialize byte array to object
     * @param buffer Input buffer
     * @param bufferSize Buffer size
     * @param data Output object pointer
     * @param size Size of object
     * @return Number of bytes read, or 0 on failure
     */
    virtual size_t deserialize(const uint8_t* buffer, size_t bufferSize,
                               void* data, size_t size) = 0;
};

/**
 * @class BinarySerializer
 * @brief Simple binary serialization (direct memcpy)
 * 
 * Fast and compact. Best for simple POD types.
 */
class BinarySerializer : public Serializer
{
public:
    size_t serialize(const void* data, size_t size,
                     uint8_t* buffer, size_t bufferSize) override
    {
        if (bufferSize < size)
            return 0;

        memcpy(buffer, data, size);
        return size;
    }

    size_t deserialize(const uint8_t* buffer, size_t bufferSize,
                       void* data, size_t size) override
    {
        if (bufferSize < size)
            return 0;

        memcpy(data, buffer, size);
        return size;
    }
};

#endif // SERIALIZER_H
