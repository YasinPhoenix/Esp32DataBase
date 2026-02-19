#ifndef DATABASE_QUICK_REFERENCE_H
#define DATABASE_QUICK_REFERENCE_H

/**
 * ============================================================================
 * DATABASE LIBRARY - QUICK REFERENCE CHEAT SHEET
 * ============================================================================
 * 
 * Copy-paste ready code snippets for common use cases.
 */

// ============================================================================
// 1. BASIC SETUP
// ============================================================================

#include "Database.h"

// Define your structure
struct MyData {
    uint32_t timestamp;
    char value[64];
} __attribute__((packed));

// In setup()
void setup() {
    db.begin();  // Initialize
    auto col = db.collection<MyData>("mydata");  // Get collection
}

// ============================================================================
// 2. WRITING DATA
// ============================================================================

// Single add (buffered)
auto col = db.collection<MyData>("mydata");
MyData entry = {millis(), "Hello"};
col->add(entry);

// Batch add
MyData entries[10] = {...};
col->addBatch(entries, 10);

// Explicit flush (optional, automatic when buffer fills)
col->flush();

// Flush all collections
db.flush();

// ============================================================================
// 3. READING DATA
// ============================================================================

// Get single entry
MyData data;
if (col->get(5, data)) {
    Serial.println(data.value);
}

// Stream entries (memory efficient)
col->forEach(0, 50, [](const MyData& d) {
    Serial.println(d.value);
});

// Stream from index to end
col->forEach(100, col->count() - 100, [](const MyData& d) {
    Serial.println(d.value);
});

// Get count
uint32_t total = col->count();
Serial.printf("Total entries: %u\n", total);

// ============================================================================
// 4. PERFORMANCE TUNING
// ============================================================================

// For high-frequency writes (increase batch)
col->setBatchSize(128);

// For low-frequency writes (keep batch small)
col->setBatchSize(4);

// For ultra-compact storage
struct TinyData {
    uint16_t a;
    uint8_t b;
} __attribute__((packed));  // Always pack structs!

// ============================================================================
// 5. DATA MANAGEMENT
// ============================================================================

// Clear all entries
col->clear();

// Print statistics
col->printStats();

// Print all collections stats
db.printStats();

// ============================================================================
// 6. COMMON PATTERNS
// ============================================================================

// Pattern A: Counters and metrics
struct Metric {
    uint32_t timestamp;
    uint16_t value;
} __attribute__((packed));

auto metrics = db.collection<Metric>("metrics");
metrics->add({millis(), sensorValue});

// Pattern B: Event logging
struct Event {
    uint32_t timestamp;
    uint8_t type;
    uint16_t userId;
    char description[64];
} __attribute__((packed));

auto events = db.collection<Event>("events");
events->add({time(nullptr), EVENT_LOGIN, userid, "User logged in"});

// Pattern C: Key-value style settings (one entry = one config)
struct Config {
    uint16_t timeout;
    uint8_t maxRetries;
    bool enabled;
} __attribute__((packed));

auto config = db.collection<Config>("config");
config->add({300, 5, true});

// To update: clear and add new
config->clear();
config->add({newTimeout, newRetries, newEnabled});

// Pattern D: Time-series data
struct Reading {
    uint32_t timestamp;
    int16_t temp;      // Temperature * 100
    uint16_t humidity; // Humidity * 100
} __attribute__((packed));

auto readings = db.collection<Reading>("readings");
readings->add({now, 2250, 5500});  // 22.5°C, 55% humidity

// Pattern E: Circular buffer style (keep last N entries)
struct HistoryEntry {
    uint32_t timestamp;
    uint8_t value;
} __attribute__((packed));

auto history = db.collection<HistoryEntry>("history");

// Add new entry
history->add({millis(), value});

// Keep only last 1000 entries
if (history->count() > 1000) {
    // Note: Would need a delete() function for this
    // Workaround: clear entire and re-add last 1000
}

// ============================================================================
// 7. ERROR HANDLING
// ============================================================================

// Check initialization
if (!db.begin()) {
    Serial.println("Database failed to initialize!");
    return;
}

// Check collection creation
auto col = db.collection<MyData>("mydata");
if (!col) {
    Serial.println("Failed to create collection!");
    return;
}

// Check add operation (always returns buffered state)
if (!col->add(entry)) {
    Serial.println("Failed to add entry!");
}

// Check flush operation
if (!col->flush()) {
    Serial.println("Failed to flush!");
}

// ============================================================================
// 8. MEMORY CONSIDERATIONS
// ============================================================================

// Structure size matters
// Every entry takes THIS MUCH space on flash
struct BadSize {
    uint32_t a;
    uint8_t b;
    // Likely padded to 8 bytes by compiler
    // TOTAL: 8 bytes per entry
} __attribute__((packed));

// With batch size 32: 32 * 8 = 256 bytes of RAM for buffer
// With batch size 128: 128 * 8 = 1024 bytes of RAM for buffer

// Keep structures as small as possible!
// Use uint16_t instead of uint32_t when possible
// Use uint8_t for bool, enums, small numbers
// Always use __attribute__((packed))

// ============================================================================
// 9. PRACTICAL EXAMPLES
// ============================================================================

// EXAMPLE 1: Door Access Log
struct AccessLog {
    uint32_t timestamp;
    uint16_t userId;
    uint8_t accessType;  // 0=NFC, 1=PIN, 2=Admin
    bool granted;
} __attribute__((packed));  // 11 bytes

void logAccess(uint16_t userId, uint8_t type, bool granted) {
    auto logs = db.collection<AccessLog>("access");
    logs->add({time(nullptr), userId, type, granted});
    // Automatically flushed every 32 entries
}

// EXAMPLE 2: Sensor Dashboard
struct SensorSnapshot {
    uint32_t timestamp;
    int16_t tempC_x100;      // e.g., 2250 = 22.50°C
    uint16_t humidity_x100;  // e.g., 5550 = 55.50%
    uint16_t co2_ppm;
} __attribute__((packed));  // 12 bytes

void recordSensor() {
    auto data = db.collection<SensorSnapshot>("sensors");
    data->add({
        millis(),
        getTemperature() * 100,
        getHumidity() * 100,
        getCO2()
    });
}

// Display last 24 readings
void showRecent() {
    auto data = db.collection<SensorSnapshot>("sensors");
    uint32_t total = data->count();
    
    if (total < 24) {
        data->forEach(0, total, [](const SensorSnapshot& s) {
            Serial.printf("T: %.2f°C H: %.2f%% CO2: %u\n",
                          s.tempC_x100/100.f, s.humidity_x100/100.f, s.co2_ppm);
        });
    } else {
        data->forEach(total - 24, 24, [](const SensorSnapshot& s) {
            Serial.printf("T: %.2f°C H: %.2f%% CO2: %u\n",
                          s.tempC_x100/100.f, s.humidity_x100/100.f, s.co2_ppm);
        });
    }
}

// EXAMPLE 3: User Sessions
struct Session {
    uint32_t loginTime;
    uint32_t logoutTime;
    uint16_t userId;
} __attribute__((packed));  // 10 bytes

void startSession(uint16_t userId) {
    auto sessions = db.collection<Session>("sessions");
    sessions->add({time(nullptr), 0, userId});
}

void endSession() {
    auto sessions = db.collection<Session>("sessions");
    uint32_t total = sessions->count();
    
    // Get last session
    Session lastSession;
    if (sessions->get(total - 1, lastSession)) {
        // Would need update() function to modify
        // Workaround: track session ID and clear/readd all
    }
}

// ============================================================================
// 10. SAFETY & BEST PRACTICES
// ============================================================================

// ✅ DO: Flush before important operations
void updateSettings(const Config& newConfig) {
    auto cfg = db.collection<Config>("config");
    cfg->add(newConfig);
    cfg->flush();  // Ensure it's saved before returning
}

// ✅ DO: Use lambdas for forEach
data->forEach(0, 100, [](const MyData& d) {
    Serial.println(d.value);
});

// ✅ DO: Keep structures small
struct Good { uint8_t a; uint16_t b; } __attribute__((packed));

// ❌ DON'T: Use large strings in structures
struct Bad { char description[512]; };  // Too big!

// ❌ DON'T: Forget __attribute__((packed))
struct VeryBad { uint8_t a; uint32_t b; };  // Padding added, wasteful

// ❌ DON'T: Change data type of collection
auto col = db.collection<LogType>("logs");
// ... later ...
auto col2 = db.collection<DifferentType>("logs");  // WRONG!

// ❌ DON'T: Assume data persists without flush
col->add(entry);
// Power loss here = entry might be lost!
col->flush();  // Now safe

// ============================================================================
// 11. DEBUGGING
// ============================================================================

// Print all data in collection (use carefully on large collections!)
void dumpAll() {
    auto col = db.collection<MyData>("mydata");
    uint32_t idx = 0;
    col->forEach(0, col->count(), [&](const MyData& d) {
        Serial.printf("[%u] %u %s\n", idx++, d.timestamp, d.value);
    });
}

// Check collection metadata
void checkMetadata() {
    auto col = db.collection<MyData>("mydata");
    Serial.printf("Entries: %lu\n", col->count());
    col->printStats();
}

// Check littlefs filesystem
void checkFilesystem() {
    FSInfo fsInfo;
    LittleFS.info(fsInfo);
    Serial.printf("Total: %u KB\n", fsInfo.totalBytes / 1024);
    Serial.printf("Used: %u KB\n", fsInfo.usedBytes / 1024);
    Serial.printf("Free: %u KB\n", (fsInfo.totalBytes - fsInfo.usedBytes) / 1024);
}

// ============================================================================
// 12. MIGRATION FROM LOGMANAGER
// ============================================================================

// Old way:
// LogManager::logEntry(DOOR_OPENED, "User entered");

// New way:
struct DoorLog { uint32_t ts; uint8_t evt; char detail[80]; } __attribute__((packed));
auto logs = db.collection<DoorLog>("door_logs");
logs->add({time(nullptr), DOOR_OPENED, "User entered"});

#endif // DATABASE_QUICK_REFERENCE_H
