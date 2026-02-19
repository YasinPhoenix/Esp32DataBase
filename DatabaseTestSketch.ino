/**
 * ============================================================================
 * ESP32 DATABASE LIBRARY - ALPHA TEST SKETCH
 * ============================================================================
 * 
 * This is a complete, compilable Arduino sketch that demonstrates the
 * database library and can be used for testing/verification on real hardware.
 * 
 * REQUIREMENTS:
 * - ESP32 board with LittleFS support
 * - LittleFS library (install via Arduino IDE)
 * - Copy src/ folder contents to project root or lib/
 * 
 * TO RUN:
 * 1. Select ESP32 board and upload
 * 2. Open Serial Monitor (115200 baud)
 * 3. Watch test output
 */

#include "Database.h"

// ============================================================================
// TEST STRUCTURES
// ============================================================================

struct TestLog
{
    uint32_t timestamp;
    uint8_t level;
    char message[80];
} __attribute__((packed));

struct TestMetric
{
    uint32_t timestamp;
    int16_t value;
} __attribute__((packed));

struct TestConfig
{
    uint16_t timeout;
    uint8_t retries;
    uint32_t lastUpdate;
} __attribute__((packed));

// ============================================================================
// TIMING UTILITIES
// ============================================================================

unsigned long testStart = 0;

void startTimer()
{
    testStart = micros();
}

unsigned long getElapsedUs()
{
    return micros() - testStart;
}

void printElapsed(const char *label)
{
    unsigned long elapsed = getElapsedUs();
    Serial.printf("%s: %lu microseconds (%.2f ms)\n", label, elapsed, elapsed / 1000.0f);
}

// ============================================================================
// TEST FUNCTIONS
// ============================================================================

bool test_initialization()
{
    Serial.println("\n[TEST 1] Database Initialization");
    Serial.println("================================");

    startTimer();
    bool result = db.begin();
    printElapsed("Database::begin()");

    if (result)
    {
        Serial.println("✅ PASS: Database initialized successfully");
        return true;
    }
    else
    {
        Serial.println("❌ FAIL: Database initialization failed");
        return false;
    }
}

bool test_single_entry_write()
{
    Serial.println("\n[TEST 2] Single Entry Write");
    Serial.println("============================");

    auto logs = db.collection<TestLog>("test_logs");
    if (!logs)
    {
        Serial.println("❌ FAIL: Failed to get collection");
        return false;
    }

    startTimer();
    TestLog entry = {
        .timestamp = time(nullptr),
        .level = 1,
        .message = "Test log entry"};
    bool result = logs->add(entry);
    unsigned long addTime = getElapsedUs();

    if (!result)
    {
        Serial.println("❌ FAIL: Failed to add entry");
        return false;
    }

    Serial.printf("Write time: %lu microseconds (%.3f ms)\n", addTime, addTime / 1000.0f);
    Serial.println("✅ PASS: Single entry written to buffer");
    return true;
}

bool test_batch_write()
{
    Serial.println("\n[TEST 3] Batch Write (32 entries)");
    Serial.println("==================================");

    auto logs = db.collection<TestLog>("test_logs");
    if (!logs)
    {
        Serial.println("❌ FAIL: Failed to get collection");
        return false;
    }

    // Pre-create batch
    TestLog entries[32];
    for (int i = 0; i < 32; i++)
    {
        entries[i] = {
            .timestamp = time(nullptr) + i,
            .level = (uint8_t)(i % 3),
            .message = {0}};
        snprintf(entries[i].message, sizeof(entries[i].message),
                 "Batch entry #%d", i);
    }

    startTimer();
    bool result = logs->addBatch(entries, 32);
    unsigned long addTime = getElapsedUs();

    if (!result)
    {
        Serial.println("❌ FAIL: Failed to add batch");
        return false;
    }

    Serial.printf("Write 32 entries: %lu microseconds (%.2f ms)\n", addTime, addTime / 1000.0f);
    Serial.printf("Avg per entry: %.3f ms\n", addTime / 32000.0f);
    Serial.println("✅ PASS: Batch write successful");
    return true;
}

bool test_flush()
{
    Serial.println("\n[TEST 4] Flush to Flash");
    Serial.println("========================");

    auto logs = db.collection<TestLog>("test_logs");

    startTimer();
    bool result = logs->flush();
    unsigned long flushTime = getElapsedUs();

    if (!result)
    {
        Serial.println("❌ FAIL: Flush operation failed");
        return false;
    }

    Serial.printf("Flush time: %lu microseconds (%.2f ms)\n", flushTime, flushTime / 1000.0f);
    Serial.println("✅ PASS: Data flushed to flash");
    return true;
}

bool test_read_single()
{
    Serial.println("\n[TEST 5] Read Single Entry");
    Serial.println("===========================");

    auto logs = db.collection<TestLog>("test_logs");

    startTimer();
    TestLog entry;
    bool result = logs->get(0, entry);
    unsigned long readTime = getElapsedUs();

    if (!result)
    {
        Serial.println("❌ FAIL: Failed to read entry");
        return false;
    }

    Serial.printf("Read time: %lu microseconds (%.3f ms)\n", readTime, readTime / 1000.0f);
    Serial.printf("Entry: [%lu] Level:%u - %s\n", entry.timestamp, entry.level, entry.message);
    Serial.println("✅ PASS: Entry read successfully");
    return true;
}

bool test_count()
{
    Serial.println("\n[TEST 6] Entry Count");
    Serial.println("====================");

    auto logs = db.collection<TestLog>("test_logs");
    uint32_t count = logs->count();

    Serial.printf("Total entries: %lu\n", count);

    if (count > 0)
    {
        Serial.println("✅ PASS: Entries counted");
        return true;
    }
    else
    {
        Serial.println("❌ FAIL: No entries found");
        return false;
    }
}

bool test_stream()
{
    Serial.println("\n[TEST 7] Stream Entries");
    Serial.println("======================");

    auto logs = db.collection<TestLog>("test_logs");
    uint32_t streamCount = 0;

    startTimer();
    logs->forEach(0, 5, [&streamCount](const TestLog &entry) {
        streamCount++;
        Serial.printf("  [%lu] Level:%u - %s\n", entry.timestamp, entry.level, entry.message);
    });
    unsigned long streamTime = getElapsedUs();

    Serial.printf("Streamed %u entries in %lu microseconds (%.2f ms)\n",
                  streamCount, streamTime, streamTime / 1000.0f);

    if (streamCount > 0)
    {
        Serial.println("✅ PASS: Streaming successful");
        return true;
    }
    else
    {
        Serial.println("❌ FAIL: No entries streamed");
        return false;
    }
}

bool test_multiple_collections()
{
    Serial.println("\n[TEST 8] Multiple Collections");
    Serial.println("=============================");

    auto logs = db.collection<TestLog>("test_logs");
    auto metrics = db.collection<TestMetric>("test_metrics");
    auto config = db.collection<TestConfig>("test_config");

    if (!logs || !metrics || !config)
    {
        Serial.println("❌ FAIL: Failed to create collections");
        return false;
    }

    // Add data to each
    TestLog log = {time(nullptr), 1, "Test"};
    logs->add(log);

    TestMetric metric = {time(nullptr), 42};
    metrics->add(metric);

    TestConfig cfg = {300, 5, time(nullptr)};
    config->add(cfg);

    db.flush();

    Serial.printf("Logs count: %lu\n", logs->count());
    Serial.printf("Metrics count: %lu\n", metrics->count());
    Serial.printf("Config count: %lu\n", config->count());

    if (logs->count() > 0 && metrics->count() > 0 && config->count() > 0)
    {
        Serial.println("✅ PASS: Multiple collections working");
        return true;
    }
    else
    {
        Serial.println("❌ FAIL: One or more collections empty");
        return false;
    }
}

bool test_stats()
{
    Serial.println("\n[TEST 9] Database Statistics");
    Serial.println("=============================");

    db.printStats();
    Serial.println("✅ PASS: Statistics printed");
    return true;
}

bool test_performance_extended()
{
    Serial.println("\n[TEST 10] Extended Performance (100 entries)");
    Serial.println("============================================");

    auto logs = db.collection<TestLog>("perf_logs");
    logs->clear();  // Start fresh

    Serial.println("Writing 100 entries...");
    startTimer();

    for (int i = 0; i < 100; i++)
    {
        TestLog entry = {
            .timestamp = time(nullptr) + i,
            .level = (uint8_t)(i % 4),
            .message = {0}};
        snprintf(entry.message, sizeof(entry.message),
                 "Perf test entry %d", i);
        logs->add(entry);
    }

    logs->flush();
    unsigned long totalTime = getElapsedUs();

    Serial.printf("Total time for 100 entries: %lu microseconds (%.2f ms)\n",
                  totalTime, totalTime / 1000.0f);
    Serial.printf("Average per entry: %.3f ms\n", totalTime / 100000.0f);

    if (totalTime < 500000)  // Should be < 500ms
    {
        Serial.println("✅ PASS: Performance excellent (fast!)");
        return true;
    }
    else if (totalTime < 1000000)  // Should be < 1s
    {
        Serial.println("⚠️  WARN: Performance acceptable");
        return true;
    }
    else
    {
        Serial.println("❌ FAIL: Performance too slow");
        return false;
    }
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

void setup()
{
    Serial.begin(115200);
    delay(2000);  // Wait for serial monitor

    Serial.println("\n╔════════════════════════════════════════════════════════════╗");
    Serial.println("║     ESP32 DATABASE LIBRARY - ALPHA TEST SUITE              ║");
    Serial.println("║                    Version 0.1                             ║");
    Serial.println("╚════════════════════════════════════════════════════════════╝");

    Serial.printf("\nTest Start Time: %lu\n", time(nullptr));
    Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());

    // Run all tests
    int passCount = 0;
    int totalTests = 0;

    // Test sequence
    if (test_initialization())
        passCount++;
    totalTests++;

    if (test_single_entry_write())
        passCount++;
    totalTests++;

    if (test_batch_write())
        passCount++;
    totalTests++;

    if (test_flush())
        passCount++;
    totalTests++;

    if (test_read_single())
        passCount++;
    totalTests++;

    if (test_count())
        passCount++;
    totalTests++;

    if (test_stream())
        passCount++;
    totalTests++;

    if (test_multiple_collections())
        passCount++;
    totalTests++;

    if (test_stats())
        passCount++;
    totalTests++;

    if (test_performance_extended())
        passCount++;
    totalTests++;

    // Final report
    Serial.println("\n╔════════════════════════════════════════════════════════════╗");
    Serial.printf("║ TESTS PASSED: %d/%d                                           ║\n", passCount, totalTests);
    Serial.println("╚════════════════════════════════════════════════════════════╝");

    if (passCount == totalTests)
    {
        Serial.println("\n🎉 ALL TESTS PASSED! Database library is ready for use.");
    }
    else
    {
        Serial.printf("\n⚠️  %d tests failed. Check output above for details.\n",
                      totalTests - passCount);
    }

    Serial.printf("\nFinal Free Heap: %u bytes\n", ESP.getFreeHeap());
}

void loop()
{
    // Test completed, nothing to do
    delay(10000);
}

/**
 * ============================================================================
 * EXPECTED OUTPUT (successful run):
 * 
 * [TEST 1] Database Initialization
 * Database::begin(): XXX microseconds (X.XX ms)
 * ✅ PASS: Database initialized successfully
 * 
 * [TEST 2] Single Entry Write
 * Write time: XXX microseconds (0.XXX ms)
 * ✅ PASS: Single entry written to buffer
 * 
 * [TEST 3] Batch Write (32 entries)
 * Write 32 entries: XXXX microseconds (X.XX ms)
 * Avg per entry: 0.XXX ms
 * ✅ PASS: Batch write successful
 * 
 * ... etc ...
 * 
 * 🎉 ALL TESTS PASSED! Database library is ready for use.
 * ============================================================================
 */
