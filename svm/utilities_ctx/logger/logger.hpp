#pragma once
#include "../includes.h"

class logs {
private:
    static constexpr u32 max_msg_length = 128;
    static constexpr u32 max_msg_count = 50000;

    struct log_message {
        u64 id;                                // Message ID
        u64 tsc;                              // Timestamp counter
        u32 cpu_id;                           // CPU ID that generated the message
        char data[max_msg_length];            // Message content
    };

    struct logger_data {
        char signature[16];                    // "hvloggerhvlogger"
        spin_lock lock;                        // Synchronization lock
        u32 msg_start;                        // Start index of the ring buffer
        u32 msg_count;                        // Current number of messages
        u64 total_msg_count;                  // Total messages logged
        log_message messages[max_msg_count];   // Ring buffer of messages
    };

    logger_data log_data;

    // Private constructor for singleton
    logs();

    // Helper functions for string formatting
    template <typename T>
    static char* custom_itoa(T value, char* result, int base, bool upper = false);
    static bool logger_format_copy_str(char* const buffer, char const* const src, u32& idx);
    void format_message(char* buffer, const char* format, va_list args);

public:
    // Delete copy constructor and assignment
    logs(const logs&) = delete;
    logs& operator=(const logs&) = delete;

    // Singleton instance getter
    static logs* get();

    // Initialize the logger
    void initialize();

    // Write a message to the log
    void write(const char* format, ...);

    // Flush logs to usermode
    svm::svmroot_error_t flush_logs(svm::pvcpu_t vcpu, u64 buffer_gva, u32& count);

    using message_t = log_message;
};

// Helper macros for different log levels
#define LOG_INFO(fmt, ...)    logs::get()->write("[INFO] " fmt, __VA_ARGS__)
#define LOG_ERROR(fmt, ...)   logs::get()->write("[ERROR] " fmt, __VA_ARGS__)
#define LOG_DEBUG(fmt, ...)   logs::get()->write("[DEBUG] " fmt, __VA_ARGS__)
#define LOG_VMCALL(fmt, ...)  logs::get()->write("[VMCALL] " fmt, __VA_ARGS__)
#define LOG_NPT(fmt, ...)     logs::get()->write("[NPT] " fmt, __VA_ARGS__)
#define LOG_MEMORY(fmt, ...)  logs::get()->write("[MEMORY] " fmt, __VA_ARGS__)