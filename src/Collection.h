#ifndef COLLECTION_H
#define COLLECTION_H

#include <Arduino.h>
#include <LittleFS.h>
#include <cstring>
#include "Serializer.h"

// Forward declaration
class Database;

/**
 * @class CollectionBase
 * @brief Base class for type-agnostic operations
 */
class CollectionBase
{
public:
    virtual ~CollectionBase() = default;
    virtual bool initialize() = 0;
    virtual bool flush() = 0;
    virtual uint32_t count() = 0;
    virtual void printStats() = 0;

protected:
    CollectionBase(const char* name, Database* db)
        : _name(name), _db(db), _entryCount(0), _currentOffset(0) {}

    String _name;
    Database* _db;
    uint32_t _entryCount;
    uint32_t _currentOffset;

    // File path management
    String getDataFilePath() const { return String("/db/") + _name + "/data.bin"; }
    String getMetadataFilePath() const { return String("/db/") + _name + "/meta.bin"; }
    String getDir() const { return String("/db/") + _name; }
};

/**
 * @class Collection<T>
 * @brief Generic collection template for storing objects of type T
 * 
 * Features:
 * - Append-only writes for performance
 * - Automatic buffering and batching
 * - File rotation for large datasets
 * - Minimal metadata overhead
 * 
 * @tparam T Data type to store (must be trivially copyable)
 */
template <typename T>
class Collection : public CollectionBase
{
public:
    static const uint16_t DEFAULT_BATCH_SIZE = 32;
    static const uint32_t MAX_FILE_SIZE = 1024 * 1024; // 1MB per file before rotation

    Collection(const char* name, Database* db)
        : CollectionBase(name, db),
          _batchSize(DEFAULT_BATCH_SIZE),
          _pendingCount(0),
          _buffer(nullptr),
          _dataFile(nullptr)
    {
        _buffer = new T[_batchSize];
    }

    ~Collection() override
    {
        if (_dataFile)
            _dataFile.close();
        if (_buffer)
            delete[] _buffer;
    }

    /**
     * @brief Initialize collection and load metadata
     * @return true if successful
     */
    bool initialize() override
    {
        // Create directory if needed
        if (!LittleFS.exists(getDir()))
        {
            if (!LittleFS.mkdir(getDir()))
            {
                Serial.printf("[DB] Failed to create directory for collection: %s\n", _name.c_str());
                return false;
            }
        }

        // Load metadata
        if (!_loadMetadata())
        {
            // First time - initialize empty metadata
            _entryCount = 0;
            _currentOffset = 0;
        }

        // Open data file in append mode
        _dataFile = LittleFS.open(getDataFilePath(), "a");
        if (!_dataFile)
        {
            Serial.printf("[DB] Failed to open data file for collection: %s\n", _name.c_str());
            return false;
        }

        return true;
    }

    /**
     * @brief Add a single entry to the collection
     * @param entry Entry to add
     * @return true if successful (buffered, may not be on flash yet)
     */
    bool add(const T& entry)
    {
        if (_pendingCount >= _batchSize)
        {
            if (!_flushBuffer())
                return false;
        }

        memcpy(&_buffer[_pendingCount], &entry, sizeof(T));
        _pendingCount++;
        return true;
    }

    /**
     * @brief Add multiple entries at once
     * @param entries Array of entries
     * @param count Number of entries
     * @return true if successful
     */
    bool addBatch(const T* entries, uint16_t count)
    {
        for (uint16_t i = 0; i < count; i++)
        {
            if (!add(entries[i]))
                return false;
        }
        return true;
    }

    /**
     * @brief Flush pending entries to storage
     * @return true if successful
     */
    bool flush() override
    {
        return _flushBuffer();
    }

    /**
     * @brief Get total number of entries
     * @return Entry count
     */
    uint32_t count() override
    {
        return _entryCount + _pendingCount;
    }

    /**
     * @brief Get entry at index (requires reading from file)
     * @param index Zero-based index
     * @param entry Output parameter
     * @return true if entry was found and read
     */
    bool get(uint32_t index, T& entry)
    {
        if (index >= count())
            return false;

        // Flush first to ensure data is on flash
        flush();

        File f = LittleFS.open(getDataFilePath(), "r");
        if (!f)
            return false;

        // Seek to entry position
        if (!f.seek(index * sizeof(T)))
        {
            f.close();
            return false;
        }

        size_t read = f.read((uint8_t*)&entry, sizeof(T));
        f.close();

        return read == sizeof(T);
    }

    /**
     * @brief Read entries in range (for streaming large datasets)
     * @param startIndex Starting index
     * @param count Number of entries to read
     * @param callback Function to call for each entry
     * @return Number of entries successfully read
     * 
     * @example
     * logs->forEach(0, 100, [](const LogEntry& entry) {
     *     Serial.println(entry.timestamp);
     * });
     */
    uint32_t forEach(uint32_t startIndex, uint32_t count,
                     std::function<void(const T&)> callback)
    {
        flush();

        if (startIndex >= this->count())
            return 0;

        uint32_t totalEntries = this->count();
        uint32_t endIndex = (startIndex + count > totalEntries) ? totalEntries : startIndex + count;
        uint32_t readCount = 0;

        File f = LittleFS.open(getDataFilePath(), "r");
        if (!f)
            return 0;

        if (!f.seek(startIndex * sizeof(T)))
        {
            f.close();
            return 0;
        }

        T entry;
        for (uint32_t i = startIndex; i < endIndex; i++)
        {
            if (f.read((uint8_t*)&entry, sizeof(T)) == sizeof(T))
            {
                callback(entry);
                readCount++;
            }
            else
            {
                break;
            }
        }

        f.close();
        return readCount;
    }

    /**
     * @brief Delete all entries in collection
     * @return true if successful
     */
    bool clear()
    {
        if (_dataFile)
            _dataFile.close();

        _entryCount = 0;
        _currentOffset = 0;
        _pendingCount = 0;

        LittleFS.remove(getDataFilePath());

        _dataFile = LittleFS.open(getDataFilePath(), "a");
        if (!_dataFile)
            return false;

        return _saveMetadata();
    }

    /**
     * @brief Set batch size for performance tuning
     * @param size Number of entries to buffer before flush (1-1000 recommended)
     */
    void setBatchSize(uint16_t size)
    {
        if (size < 1 || size > 1000)
            return;

        flush();

        delete[] _buffer;
        _batchSize = size;
        _buffer = new T[_batchSize];
    }

    /**
     * @brief Print collection statistics
     */
    void printStats() override
    {
        Serial.printf("[%s] Entries: %lu | Pending: %u | Batch Size: %u | Size: ~%lu bytes\n",
                      _name.c_str(),
                      _entryCount,
                      _pendingCount,
                      _batchSize,
                      _entryCount * sizeof(T));
    }

private:
    uint16_t _batchSize;
    uint16_t _pendingCount;
    T* _buffer;
    File _dataFile;

    /**
     * @brief Flush buffered entries to flash storage
     */
    bool _flushBuffer()
    {
        if (_pendingCount == 0)
            return true;

        if (!_dataFile)
            return false;

        // Write all pending entries in one operation
        size_t written = _dataFile.write((const uint8_t*)_buffer, _pendingCount * sizeof(T));
        if (written != _pendingCount * sizeof(T))
        {
            Serial.printf("[DB] Failed to write to file. Expected: %u, Written: %u\n",
                          _pendingCount * sizeof(T), written);
            return false;
        }

        _dataFile.flush();

        _entryCount += _pendingCount;
        _currentOffset += _pendingCount * sizeof(T);
        _pendingCount = 0;

        // Save metadata
        if (!_saveMetadata())
            return false;

        // Check if file rotation is needed
        if (_currentOffset >= MAX_FILE_SIZE)
        {
            return _rotateFile();
        }

        return true;
    }

    /**
     * @brief Rotate file when size limit reached
     */
    bool _rotateFile()
    {
        if (_dataFile)
            _dataFile.close();

        // Generate rotated filename with timestamp
        String oldName = getDataFilePath();
        String newName = getDir() + "/data_" + String(millis() / 1000) + ".bin";

        if (!LittleFS.rename(oldName, newName))
        {
            Serial.printf("[DB] Failed to rotate file\n");
            return false;
        }

        // Reset offset for new file
        _currentOffset = 0;

        // Open new file
        _dataFile = LittleFS.open(oldName, "a");
        if (!_dataFile)
            return false;

        return _saveMetadata();
    }

    /**
     * @brief Load metadata from file
     */
    bool _loadMetadata()
    {
        File meta = LittleFS.open(getMetadataFilePath(), "r");
        if (!meta)
            return false;

        if (meta.read((uint8_t*)&_entryCount, sizeof(_entryCount)) != sizeof(_entryCount))
        {
            meta.close();
            return false;
        }

        if (meta.read((uint8_t*)&_currentOffset, sizeof(_currentOffset)) != sizeof(_currentOffset))
        {
            meta.close();
            return false;
        }

        meta.close();
        return true;
    }

    /**
     * @brief Save metadata to file
     */
    bool _saveMetadata()
    {
        File meta = LittleFS.open(getMetadataFilePath(), "w");
        if (!meta)
            return false;

        if (meta.write((const uint8_t*)&_entryCount, sizeof(_entryCount)) != sizeof(_entryCount))
        {
            meta.close();
            return false;
        }

        if (meta.write((const uint8_t*)&_currentOffset, sizeof(_currentOffset)) != sizeof(_currentOffset))
        {
            meta.close();
            return false;
        }

        meta.flush();
        meta.close();
        return true;
    }
};

#endif // COLLECTION_H
