# ESP32 Dynamic Database Library (Alpha)

A high-performance, flexible database library for ESP32 devices using littlefs. Designed to be generic enough for **any** data storage need—logs, settings, user data, sensor readings, etc.

## Key Features

✅ **Generic Template Design** - Works with any C++ struct (users define their own structures)  
✅ **High Performance** - Append-only writes with automatic batching (ms, not seconds)  
✅ **Multiple Collections** - Store different data types independently  
✅ **Minimal Overhead** - Lightweight metadata, no unnecessary indexing  
✅ **File Rotation** - Automatic rotation when collections exceed 1MB  
✅ **RAM-Efficient** - Configurable batch sizes for different constraints  
✅ **Fast Reads** - Stream from arbitrary positions without loading entire file  

## Architecture Overview

```
Database
├── Collection<LogEntry>    → /db/logs/
│   ├── data.bin           (binary entries, append-only)
│   └── meta.bin           (entry count, offset)
├── Collection<Settings>   → /db/settings/
│   ├── data.bin
│   └── meta.bin
└── Collection<User>       → /db/users/
    ├── data.bin
    └── meta.bin
```

## Why This Design?

### Problem: Old LogManager
- ❌ Circular buffer with frequent header updates
- ❌ Random seeks in file (slow on flash)
- ❌ 2-3 seconds per write with 100+ entries
- ❌ Tightly coupled to log events only

### Solution: New Architecture
- ✅ Append-only writes (native littlefs strength)
- ✅ Automatic buffering (32 entries batched = 1 write)
- ✅ No seeking/header thrashing
- ✅ Generic—works for ANY data type

**Performance Improvement:** From 2-3 seconds to **1-2ms per entry** with batching.

## Usage

### Basic Example: Create & Store Data

```cpp
#include "Database.h"

// Define your structure
struct LogEntry {
    uint32_t timestamp;
    uint8_t level;
    char message[96];
} __attribute__((packed));

// In setup()
void setup() {
    db.begin();
    
    // Add entries
    auto logs = db.collection<LogEntry>("logs");
    LogEntry entry = {millis(), 1, "System started"};
    logs->add(entry);
    
    // Automatic flushing happens when buffer fills
    // Or manually flush:
    logs->flush();
}

// In loop()
void loop() {
    // Add entries continuously - they buffer automatically
    LogEntry entry = {millis(), 2, "Loop iteration"};
    db.collection<LogEntry>("logs")->add(entry);
    
    delay(100);
    
    // Periodic flush for safety
    if (millis() % 10000 == 0) {
        db.flush();
    }
}
```

### Reading Data Back

```cpp
// Get single entry
LogEntry entry;
logs->get(5, entry);  // Get 5th entry
Serial.println(entry.message);

// Stream multiple entries efficiently
logs->forEach(0, 50, [](const LogEntry& e) {
    Serial.printf("[%u] %s\n", e.timestamp, e.message);
});

// Get count
Serial.printf("Total entries: %lu\n", logs->count());
```

### Multiple Data Types in Same Database

```cpp
// Different collections, different types
auto logs = db.collection<LogEntry>("logs");
auto settings = db.collection<Settings>("settings");  
auto users = db.collection<User>("users");

// Each works independently
logs->add(logEntry);
settings->add(config);
users->addBatch(userArray, userCount);

// All can be flushed together
db.flush();
```

## Performance Tuning

### Default Settings
- **Batch Size:** 32 entries per flush
- **Max File Size:** 1MB per collection
- **Data Type Size:** User-defined (as small as 11 bytes, as large as needed)

### Optimize for Your Scenario

```cpp
auto sensor = db.collection<SensorReading>("sensors");

// High-frequency sensor data (100s/sec) → bigger batch
sensor->setBatchSize(256);  // Flush every 256 reads (faster overall)

// Low-frequency data (once per hour) → smaller batch
auto config = db.collection<Settings>("config");
config->setBatchSize(4);    // Flush often (more durable)
```

### Size Estimation

With default batch size (32):

| Data Type | Size/Entry | 32-Entry Batch | Flash Writes/Hour |
|-----------|-----------|--|--|
| LogEntry (101 bytes) | 101B | 3.2KB | 1-10 |
| SensorReading (11 bytes) | 11B | 352B | 1-100 |
| User (210 bytes) | 210B | 6.7KB | <1 |

## File Structure

```
/db/
├── logs/
│   ├── data.bin          (all log entries append-only)
│   ├── meta.bin          (8 bytes: entry_count + offset)
│   ├── data_12345.bin    (rotated old file)
│   └── data_67890.bin    (rotated old file)
├── settings/
│   ├── data.bin
│   └── meta.bin
└── users/
    ├── data.bin
    └── meta.bin
```

## API Reference

### Database Initialization

```cpp
bool Database::begin()
```
Initialize littlefs and create /db directory. **Must be called first.**

```cpp
db.begin();
```

### Get/Create Collection

```cpp
template<typename T>
Collection<T>* Database::collection(const char* name)
```
Returns pointer to collection. Creates if doesn't exist.

**Type Safety Note:** Always use same T for same collection name.

```cpp
auto logs = db.collection<LogEntry>("logs");
```

### Add Data

```cpp
bool Collection<T>::add(const T& entry)
```
Add single entry (buffered in RAM).

```cpp
bool Collection<T>::addBatch(const T* array, uint16_t count)
```
Add multiple entries at once (efficient for bulk imports).

### Flushing

```cpp
bool Collection<T>::flush()
bool Database::flush()
```
Write all buffered entries to flash. Called automatically when buffer fills, but can be manual.

### Reading

```cpp
bool Collection<T>::get(uint32_t index, T& entry)
```
Get single entry by index.

```cpp
uint32_t Collection<T>::forEach(uint32_t start, uint32_t count, 
                                std::function<void(const T&)> callback)
```
Stream entries from index without loading entire file. Callback invoked per entry.

```cpp
uint32_t Collection<T>::count()
```
Get total entries (buffered + persisted).

### Configuration

```cpp
void Collection<T>::setBatchSize(uint16_t size)
```
Tune batch size for performance. 1-1000 valid.

```cpp
void Collection<T>::clear()
```
Delete all entries in collection.

```cpp
void Database::printStats()
```
Print statistics for all collections (for debugging).

## Design Decisions

### 1. Why Append-Only?
- littlefs optimized for sequential writes
- No seeking = faster, less wear
- Simple, predictable behavior

### 2. Why Separate Collections?
- Independent file management
- No mixing data types (reduces errors)
- Easy to clear/rotate one without touching others

### 3. Why Manual Flush?
- User controls when data hits flash
- Option for asynchronous flushing later
- Balances durability vs. performance

### 4. Why Store Metadata Separately?
- Quick access to entry count
- Handles crashes gracefully (can rescan if needed)
- Only 8 bytes overhead per collection

### 5. Why No Built-in Encryption/Compression?
- ESP32 RAM is limited
- Users choose what they need
- Library stays lightweight and flexible

## Limitations & Future Work

### Current Alpha (v0.1)
- ❌ No transactions/rollback
- ❌ No built-in indexing/searching
- ❌ No encryption
- ❌ No JSON serialization (only binary)

### Planned (v0.2+)
- ✅ Optional JSON backend
- ✅ Simple index support (timestamp ranges)
- ✅ Optional compression
- ✅ Async write queue
- ✅ Query builder for filtering

## Memory Usage

Predefined/Predictable:
- Base Database class: ~100 bytes
- Per Collection: Size of batch buffer + metadata (~4KB for 32-entry buffer with 128B entries)
- No dynamic allocations after initialization

## Building & Integration

### Arduino IDE
1. Copy `src/` folder to your Arduino project
2. Include headers:
```cpp
#include "Database.h"
#include "DatabaseExamples.h"  // optional
```

### PlatformIO
Copy to `lib/Database/` and PlatformIO will find headers automatically.

## Examples Included

- `DatabaseExamples.h` contains 5 complete examples:
  1. Simple logging
  2. Configuration storage
  3. User management
  4. Time-series sensor data
  5. Performance tuning

Run with: `database_setup()`

## Testing on ESP32

```cpp
void setup() {
    Serial.begin(115200);
    delay(2000);
    
    database_setup();  // Runs all examples
}

void loop() {}
```

## Performance Benchmarks (Expected)

| Operation | Old LogManager | New Library |
|-----------|---|---|
| Add single entry | 2-3 seconds | <1ms (buffered) |
| Add 100 entries | 200-300s ❌ | 1-2ms ✅ |
| Flush 32 entries | N/A | ~5-10ms |
| Read entry | Scan entire file | Seek + read (~10ms) |
| Stream 100 entries | Not efficient | ~50ms |

## Troubleshooting

**"Failed to initialize LittleFS"**
- Check if littlefs library is installed
- Verify ESP32 partition scheme includes littlefs

**"Collection not found after reboot"**
- Collections are recreated from metadata
- Data persists in `/db/` directory

**Slow performance**
- Try increasing batch size: `collection->setBatchSize(64)`
- Ensure you're calling `flush()` periodically

**Memory growing**
- Check batch size isn't impossibly large
- Verify no memory leaks in your code

## License & Notes

This is an **Alpha** (v0.1) release. API may change. Feedback welcome.

Inspired by limitations of previous LogManager implementation but completely redesigned for flexibility and performance.
