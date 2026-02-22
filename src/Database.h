#ifndef DATABASE_H
#define DATABASE_H

#include <Arduino.h>
#include <LittleFS.h>
#include <map>
#include "Collection.h"

/**
 * @class Database
 * @brief Main database manager for ESP32 using littlefs
 * 
 * Manages multiple collections with support for dynamic data types,
 * batching, and automatic file rotation for optimal performance.
 */
class Database
{
public:
    Database();
    ~Database();

    /**
     * @brief Initialize the database and littlefs
     * @return true if successful
     */
    bool begin();

    /**
     * @brief Get or create a collection
     * @tparam T Data type to store in collection
     * @param name Collection name (e.g., "logs", "settings")
     * @return Pointer to Collection<T>
     * 
     * @example
     * auto logs = db.collection<LogEntry>("logs");
     * logs->add(myLogEntry);
     */
    template <typename T>
    Collection<T>* collection(const char* name)
    {
        if (!_initialized)
        {
            Serial.println("[DB] Cannot get collection: Database not initialized");
            return nullptr;
        }
        // Note: Type safety is user's responsibility
        // Always use the same T for a given collection name
        auto it = _collections.find(name);
        if (it != _collections.end())
        {
            return static_cast<Collection<T>*>(it->second);
        }

        Collection<T>* col = new Collection<T>(name, this);
        if (!col->initialize())
        {
            delete col;
            return nullptr;
        }
        _collections[name] = col;
        return col;
    }

    /**
     * @brief Flush all pending writes to storage
     * @return true if successful
     */
    bool flush();

    /**
     * @brief Delete all data in a collection
     * @param name Collection name
     * @return true if successful
     */
    bool clearCollection(const char* name);

    /**
     * @brief Get database statistics
     */
    void printStats();

private:
    bool _initialized;
    std::map<String, void*> _collections;

    friend class CollectionBase;
};

extern Database db;

#endif // DATABASE_H
