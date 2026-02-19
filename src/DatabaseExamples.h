#ifndef DATABASE_EXAMPLES_H
#define DATABASE_EXAMPLES_H

#include <Arduino.h>
#include "Database.h"

/**
 * ============================================================================
 * DATABASE LIBRARY - USAGE EXAMPLES
 * ============================================================================
 * 
 * This file demonstrates how to use the dynamic database library for
 * different data types and use cases.
 */

// ============================================================================
// EXAMPLE 1: Simple Logging
// ============================================================================

struct LogEntry
{
    uint32_t timestamp;      // Unix timestamp (4 bytes)
    uint8_t level;           // 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR (1 byte)
    char message[96];        // Message text (96 bytes)
    // Total: 101 bytes per entry
} __attribute__((packed));

void example_logging()
{
    Serial.println("\n=== LOG EXAMPLE ===");

    // Get or create logs collection
    auto logs = db.collection<LogEntry>("logs");
    if (!logs)
    {
        Serial.println("Failed to get logs collection");
        return;
    }

    // Log some events
    LogEntry entry1 = {
        .timestamp = 1708354800,
        .level = 2,
        .message = "System started"};
    logs->add(entry1);

    LogEntry entry2 = {
        .timestamp = 1708354810,
        .level = 3,
        .message = "Error: WiFi connection failed"};
    logs->add(entry2);

    // Add more entries efficiently
    LogEntry entry3 = {
        .timestamp = 1708354820,
        .level = 1,
        .message = "WiFi reconnected successfully"};
    logs->add(entry3);

    // Flush to ensure data is persisted
    logs->flush();

    // Query: Read first 10 entries
    Serial.println("First 10 log entries:");
    logs->forEach(0, 10, [](const LogEntry& entry) {
        Serial.printf("  [%lu] Level:%u - %s\n", entry.timestamp, entry.level, entry.message);
    });

    logs->printStats();
}

// ============================================================================
// EXAMPLE 2: Configuration Storage
// ============================================================================

struct Settings
{
    uint16_t updateInterval;    // Seconds (2 bytes)
    uint8_t maxLogEntries;      // Max logs before rotation (1 byte)
    bool enableDebug;           // Debug mode (1 byte)
    char wifiSSID[32];          // WiFi SSID (32 bytes)
    char wifiPassword[64];      // WiFi password (64 bytes)
    uint32_t lastSyncTime;      // Last sync timestamp (4 bytes)
    // Total: 104 bytes
} __attribute__((packed));

void example_settings()
{
    Serial.println("\n=== SETTINGS EXAMPLE ===");

    auto settings = db.collection<Settings>("settings");
    if (!settings)
    {
        Serial.println("Failed to get settings collection");
        return;
    }

    // Create default settings
    Settings defaultConfig = {
        .updateInterval = 300,
        .maxLogEntries = 100,
        .enableDebug = true,
        .wifiSSID = "MyNetworkSSID",
        .wifiPassword = "securepwd123",
        .lastSyncTime = 0};

    settings->add(defaultConfig);
    settings->flush();

    // Read settings back
    Settings loaded;
    if (settings->get(0, loaded))
    {
        Serial.printf("Loaded settings: updateInterval=%u, maxLogs=%u\n",
                      loaded.updateInterval, loaded.maxLogEntries);
        Serial.printf("WiFi SSID: %s\n", loaded.wifiSSID);
    }

    settings->printStats();
}

// ============================================================================
// EXAMPLE 3: User Management with Database
// ============================================================================

struct User
{
    uint16_t id;                    // User ID (2 bytes)
    uint64_t createdAt;             // Creation timestamp (8 bytes)
    uint64_t lastLogin;             // Last login time (8 bytes)
    char username[32];              // Username (32 bytes)
    char passwordHash[64];          // Password hash (64 bytes)
    uint8_t permissions;            // Bitmask (1 byte)
    bool isActive;                  // Active status (1 byte)
    // Total: 210 bytes per user
} __attribute__((packed));

void example_users()
{
    Serial.println("\n=== USER MANAGEMENT EXAMPLE ===");

    auto users = db.collection<User>("users");
    if (!users)
    {
        Serial.println("Failed to get users collection");
        return;
    }

    // Add multiple users efficiently using batch add
    User userList[] = {
        {.id = 1, .createdAt = 1700000000, .lastLogin = 1708354800,
         .username = "admin", .passwordHash = "hash123...", .permissions = 0xFF, .isActive = true},
        {.id = 2, .createdAt = 1700000000, .lastLogin = 1708354700,
         .username = "user1", .passwordHash = "hash456...", .permissions = 0x0F, .isActive = true},
        {.id = 3, .createdAt = 1700000100, .lastLogin = 0,
         .username = "guest", .passwordHash = "hash789...", .permissions = 0x01, .isActive = false}};

    users->addBatch(userList, 3);
    users->flush();

    Serial.printf("Total users: %lu\n", users->count());

    // Stream user list efficiently
    Serial.println("User list:");
    users->forEach(0, users->count(), [](const User& user) {
        Serial.printf("  ID:%u - %s (Active:%s, Permissions:0x%02X)\n",
                      user.id, user.username, user.isActive ? "Y" : "N", user.permissions);
    });

    users->printStats();
}

// ============================================================================
// EXAMPLE 4: Sensor Data Time Series
// ============================================================================

struct SensorReading
{
    uint32_t timestamp;         // Reading time (4 bytes)
    int16_t temperature;        // Temp in 0.01°C (2 bytes) - range: -327 to +327°C
    uint16_t humidity;          // Humidity in 0.01% (2 bytes) - range: 0-100%
    uint16_t pressure;          // Pressure in Pa (2 bytes)
    uint8_t status;             // Sensor status (1 byte)
    // Total: 11 bytes per reading (compact!)
} __attribute__((packed));

void example_sensor_data()
{
    Serial.println("\n=== SENSOR DATA EXAMPLE ===");

    auto sensorData = db.collection<SensorReading>("sensor_data");
    if (!sensorData)
    {
        Serial.println("Failed to get sensor_data collection");
        return;
    }

    // Simulate adding many sensor readings (very fast with batching)
    uint32_t now = 1708354800;
    for (int i = 0; i < 100; i++)
    {
        SensorReading reading = {
            .timestamp = now + (i * 60),           // Every minute
            .temperature = 2200 + random(-50, 50), // 22°C ± 0.5°C
            .humidity = 5500 + random(-100, 100),  // 55% ± 1%
            .pressure = 101325,                     // Standard pressure
            .status = 0                             // OK
        };
        sensorData->add(reading);
    }

    // Automatic batching makes this fast!
    sensorData->flush();

    Serial.printf("Total readings stored: %lu\n", sensorData->count());

    // Read latest 5 readings
    Serial.println("Latest 5 readings:");
    uint32_t total = sensorData->count();
    sensorData->forEach(
        total > 5 ? total - 5 : 0, 5,
        [](const SensorReading& reading) {
            float temp = reading.temperature / 100.0f;
            float humidity = reading.humidity / 100.0f;
            Serial.printf("  [%u] Temp:%.2f°C Humidity:%.1f%% Pressure:%u Pa\n",
                          reading.timestamp, temp, humidity, reading.pressure);
        });

    sensorData->printStats();
}

// ============================================================================
// EXAMPLE 5: Custom Performance Tuning
// ============================================================================

void example_performance_tuning()
{
    Serial.println("\n=== PERFORMANCE TUNING EXAMPLE ===");

    // For high-frequency data (hundreds/sec), increase batch size
    auto highFreq = db.collection<SensorReading>("high_freq_sensor");
    if (highFreq)
    {
        highFreq->setBatchSize(128); // Larger batch = fewer flushes = better performance
        Serial.println("Set batch size to 128 for high-frequency sensor");
    }

    // For low-frequency data (once per hour), keep small batch
    auto hourlyData = db.collection<LogEntry>("hourly_logs");
    if (hourlyData)
    {
        hourlyData->setBatchSize(8); // Small batch = frequent saves = more durable
        Serial.println("Set batch size to 8 for hourly logs");
    }
}

// ============================================================================
// COMPLETE SETUP EXAMPLE
// ============================================================================

void database_setup()
{
    Serial.println("\n========== DATABASE SETUP ==========");

    // Initialize database
    if (!db.begin())
    {
        Serial.println("FATAL: Database initialization failed!");
        while (true)
            delay(1000);
    }

    // Run examples
    example_logging();
    example_settings();
    example_users();
    example_sensor_data();
    example_performance_tuning();

    // Print overall stats
    db.printStats();

    Serial.println("Database examples completed successfully!");
}

#endif // DATABASE_EXAMPLES_H
