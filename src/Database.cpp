#include "Database.h"

Database db;

Database::Database()
    : _initialized(false)
{
}

Database::~Database()
{
    for (auto& pair : _collections)
    {
        delete static_cast<CollectionBase*>(pair.second);
    }
    _collections.clear();
}

bool Database::begin()
{
    if (!LittleFS.begin())
    {
        Serial.println("[DB] Failed to initialize LittleFS!");
        return false;
    }

    // Create database root directory
    if (!LittleFS.exists("/db"))
    {
        if (!LittleFS.mkdir("/db"))
        {
            Serial.println("[DB] Failed to create /db directory!");
            return false;
        }
    }

    _initialized = true;
    Serial.println("[DB] Database initialized successfully");
    return true;
}

bool Database::flush()
{
    bool success = true;
    for (auto& pair : _collections)
    {
        CollectionBase* col = static_cast<CollectionBase*>(pair.second);
        if (!col->flush())
        {
            Serial.printf("[DB] Failed to flush collection: %s\n", pair.first.c_str());
            success = false;
        }
    }
    return success;
}

bool Database::clearCollection(const char* name)
{
    auto it = _collections.find(name);
    if (it == _collections.end())
    {
        Serial.printf("[DB] Collection not found: %s\n", name);
        return false;
    }

    CollectionBase* col = static_cast<CollectionBase*>(it->second);
    // Note: Would need a virtual clear() method in CollectionBase for this
    // For now, this is a placeholder
    return true;
}

void Database::printStats()
{
    Serial.println("\n========== Database Statistics ==========");
    for (auto& pair : _collections)
    {
        CollectionBase* col = static_cast<CollectionBase*>(pair.second);
        col->printStats();
    }
    Serial.println("=========================================\n");
}
