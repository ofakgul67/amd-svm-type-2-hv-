#include "logger.hpp"

logs::logs() {
    initialize();
}

logs* logs::get() {
    static logs instance;
    return &instance;
}

void logs::initialize() {
    auto& l = log_data;

    memcpy(l.signature, "hvloggerhvlogger", 16);

    l.lock.initialize();
    l.msg_start = 0;
    l.msg_count = 0;
    l.total_msg_count = 0;

    write("Logger initialized\n");
}

template <typename T>
char* logs::custom_itoa(T value, char* result, int base, bool upper) {
    // check that the base is valid
    if (base < 2 || base > 36) {
        *result = '\0';
        return result;
    }

    char* ptr = result, * ptr1 = result, tmp_char;
    T tmp_value;

    const char* digits_upper = "ZYXWVUTSRQPONMLKJIHGFEDCBA9876543210123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char* digits_lower = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz";

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = (upper ? digits_upper : digits_lower)[35 + (tmp_value - value * base)];
    } while (value);

    // Apply negative sign
    if (tmp_value < 0)
        *ptr++ = '-';

    *ptr-- = '\0';
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
    return result;
}

bool logs::logger_format_copy_str(char* const buffer, char const* const src, u32& idx) {
    for (u32 i = 0; src[i]; ++i) {
        buffer[idx++] = src[i];

        // buffer end has been reached
        if (idx >= max_msg_length - 1) {
            buffer[max_msg_length - 1] = '\0';
            return true;
        }
    }

    return false;
}

void logs::format_message(char* buffer, const char* format, va_list args) {
    u32 buffer_idx = 0;
    u32 format_idx = 0;

    // true if the last character was a '%'
    bool specifying = false;

    while (true) {
        auto const c = format[format_idx++];

        // format end has been reached
        if (c == '\0')
            break;

        if (c == '%') {
            specifying = true;
            continue;
        }

        // just copy the character directly
        if (!specifying) {
            buffer[buffer_idx++] = c;

            // buffer end has been reached
            if (buffer_idx >= max_msg_length - 1)
                break;

            specifying = false;
            continue;
        }

        char num_buffer[128];

        // format the string according to the specifier
        switch (c) {
        case 's': {
            if (logger_format_copy_str(buffer, va_arg(args, char const*), buffer_idx))
                return;
            break;
        }
        case 'd':
        case 'i': {
            if (logger_format_copy_str(buffer,
                custom_itoa(va_arg(args, int), num_buffer, 10), buffer_idx))
                return;
            break;
        }
        case 'x': {
            if (logger_format_copy_str(buffer, "0x", buffer_idx))
                return;
            if (logger_format_copy_str(buffer,
                custom_itoa(va_arg(args, u64), num_buffer, 16, false), buffer_idx))
                return;
            break;
        }
        case 'X': {
            if (logger_format_copy_str(buffer, "0x", buffer_idx))
                return;
            if (logger_format_copy_str(buffer,
                custom_itoa(va_arg(args, u64), num_buffer, 16, true), buffer_idx))
                return;
            break;
        }
        case 'p': {
            if (logger_format_copy_str(buffer, "0x", buffer_idx))
                return;
            if (logger_format_copy_str(buffer,
                custom_itoa((u64)va_arg(args, void*), num_buffer, 16, true), buffer_idx))
                return;
            break;
        }
        }

        specifying = false;
    }

    buffer[buffer_idx] = '\0';
}

void logs::write(const char* format, ...) {
    char str[max_msg_length];

    // Format the message
    va_list args;
    va_start(args, format);
    format_message(str, format, args);
    va_end(args);

    // Use direct lock scope
    log_data.lock.acquire();

    auto& msg = log_data.messages[(log_data.msg_start + log_data.msg_count) % max_msg_count];

    if (log_data.msg_count < max_msg_count)
        log_data.msg_count++;
    else
        log_data.msg_start = (log_data.msg_start + 1) % max_msg_count;

    // Copy string directly like they do
    memset(msg.data, 0, max_msg_length);
    for (size_t i = 0; (i < max_msg_length - 1) && str[i]; ++i)
        msg.data[i] = str[i];

    // Set metadata
    msg.id = ++log_data.total_msg_count;
    msg.tsc = __rdtsc();
    msg.cpu_id = KeGetCurrentProcessorNumber();

    log_data.lock.release();
}

svm::svmroot_error_t logs::flush_logs(svm::pvcpu_t vcpu, u64 buffer_gva, u32& count) {
    if (count == 0) {
        return svm::svmroot_error_t::error_failed;
    }

    log_data.lock.acquire();
    count = min(count, log_data.msg_count);

    if (count > 0) {
        // Calculate first chunk before wraparound
        auto start = reinterpret_cast<u8*>(&log_data.messages[log_data.msg_start]);
        auto first_chunk_count = min(max_msg_count - log_data.msg_start, count);
        auto size = first_chunk_count * sizeof(log_message);

        // Copy first chunk before wraparound
        for (size_t bytes_copied = 0; bytes_copied < size;) {
            size_t curr_size = 0;

            // Get host virtual address for current position
            auto curr_hva = page_table::get()->gva_to_hva(
                vcpu->guest_vmcb.state_save_area.cr3,
                buffer_gva + bytes_copied,
                curr_size);

            if (!curr_hva) {
                log_data.lock.release();
                return svm::svmroot_error_t::error_failed;
            }

            // Calculate how much we can copy in this iteration
            auto copy_size = min(size - bytes_copied, curr_size);

            // Copy the chunk
            memcpy(
                curr_hva,
                start + bytes_copied,
                copy_size
            );

            bytes_copied += copy_size;
        }

        // Handle wraparound if needed
        if (first_chunk_count < count) {
            buffer_gva += size;
            start = reinterpret_cast<u8*>(&log_data.messages[0]);
            size = (count - first_chunk_count) * sizeof(log_message);

            // Copy remaining data after wraparound
            for (size_t bytes_copied = 0; bytes_copied < size;) {
                size_t curr_size = 0;

                // Get host virtual address for current position
                auto curr_hva = page_table::get()->gva_to_hva(
                    vcpu->guest_vmcb.state_save_area.cr3,
                    buffer_gva + bytes_copied,
                    curr_size);

                if (!curr_hva) {
                    log_data.lock.release();
                    return svm::svmroot_error_t::error_failed;
                }

                // Calculate how much we can copy in this iteration
                auto copy_size = min(size - bytes_copied, curr_size);

                // Copy the chunk
                memcpy(
                    curr_hva,
                    start + bytes_copied,
                    copy_size
                );

                bytes_copied += copy_size;
            }
        }

        // Update ring buffer state
        log_data.msg_count -= count;
        log_data.msg_start = (log_data.msg_start + count) % max_msg_count;
    }

    log_data.lock.release();
    return svm::svmroot_error_t::error_success;
}
