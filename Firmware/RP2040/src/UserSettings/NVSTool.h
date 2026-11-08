#ifndef _NVS_TOOL_H_
#define _NVS_TOOL_H_

#include <atomic>
#include <cstdint>
#include <string>
#include <array>
#include <cstring>
#include <hardware/flash.h>
#include <hardware/sync.h>
#include <pico/flash.h>
#include <pico/mutex.h>

/* Define NVS_SECTORS (number of sectors to allocate to storage) either here or with CMake */

class NVSTool
{
public:
    static constexpr size_t   KEY_LEN_MAX = 16; //Including null terminator
    static constexpr size_t   VALUE_LEN_MAX = FLASH_PAGE_SIZE - KEY_LEN_MAX;
    static constexpr uint32_t MAX_ENTRIES = ((NVS_SECTORS * FLASH_SECTOR_SIZE) / FLASH_PAGE_SIZE) - 1;

    static NVSTool& get_instance()
    {
        static NVSTool instance;
        return instance;
    }

    /* board_api::usb::disconnect_all() has reset the other core (a flash write and a reboot
     * follow). Flash is then written directly: flash_safe_execute() would wait for the dead
     * core to acknowledge a lockout, time out and skip the write. */
    static void set_other_core_halted()
    {
        other_core_halted_.store(true, std::memory_order_release);
    }

    bool write(const std::string& key, const void* value, size_t len)
    {
        if (!valid_args(key, len))
        {
            return false;
        }

        mutex_enter_blocking(&nvs_mutex_);

        for (uint32_t i = 1; i < MAX_ENTRIES; ++i) 
        {
            Entry* read_entry = get_entry(i);

            if (!is_valid_entry(read_entry) || std::strcmp(read_entry->key, key.c_str()) == 0)
            {
                const bool ok = update_entry(i, key, value, len);

                mutex_exit(&nvs_mutex_);
                return ok;
            }
        }

        mutex_exit(&nvs_mutex_);
        return false; // No space for new entry
    }

    bool read(const std::string& key, void* value, size_t len)
    {
        if (!valid_args(key, len))
        {
            return false;
        }

        mutex_enter_blocking(&nvs_mutex_);

        for (uint32_t i = 1; i < MAX_ENTRIES; ++i) 
        {
            Entry* read_entry = get_entry(i);

            if (std::strcmp(read_entry->key, key.c_str()) == 0)
            {
                std::memcpy(value, read_entry->value, len);

                mutex_exit(&nvs_mutex_);
                return true;
            }
            else if (!is_valid_entry(read_entry))
            {
                // Key not found
                break;
            }
        }

        mutex_exit(&nvs_mutex_);
        return false;
    }

    bool erase_all()
    {
        mutex_enter_blocking(&nvs_mutex_);

        const bool ok = run_flash_op([]() {
            Entry entry;
            for (uint32_t i = 0; i < NVS_SECTORS; ++i)
            {
                flash_range_erase(NVS_START_OFFSET + i * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
            }
            for (uint32_t i = 0; i < MAX_ENTRIES + 1; ++i)
            {
                flash_range_program(NVS_START_OFFSET + i * sizeof(Entry),
                                    reinterpret_cast<const uint8_t*>(&entry),
                                    sizeof(Entry));
            }
        });

        mutex_exit(&nvs_mutex_);
        return ok;
    }

private:
    NVSTool()
    {
        mutex_init(&nvs_mutex_);

        Entry* initial_entry = get_entry(0);

        if (std::strcmp(initial_entry->key, INVALID_KEY) != 0) 
        {
            erase_all();
        }
    }

    ~NVSTool() = default;
    NVSTool(const NVSTool&) = delete;
    NVSTool& operator=(const NVSTool&) = delete;

    struct Entry 
    {
        char key[KEY_LEN_MAX];
        uint8_t value[VALUE_LEN_MAX];

        Entry()
        {
            std::fill(std::begin(key), std::end(key), '\0');
            std::strncpy(key, INVALID_KEY, sizeof(key) - 1);
            std::fill(std::begin(value), std::end(value), 0xFF);
        }
    };
    static_assert(sizeof(Entry) == FLASH_PAGE_SIZE, "NVSTool::Entry size mismatch");

    static constexpr const char INVALID_KEY[KEY_LEN_MAX] = "INVALID";
    static constexpr uint32_t NVS_START_OFFSET = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE * NVS_SECTORS;

    mutex_t nvs_mutex_;
    static inline std::atomic<bool> other_core_halted_{false};

    /* How long flash_safe_execute() waits for the other core to park. */
    static constexpr uint32_t SAFE_EXECUTE_TIMEOUT_MS = 500;

    /* Run a flash erase/program sequence safely. XIP is unavailable meanwhile, so
     * nothing may execute from flash: not an IRQ handler on this core, and not the other core.
     * Interrupts off only covered this core; a write while the other core ran (Bluetooth on
     * Core1) could hard-fault it. Now:
     *  - other core already halted (mode change): interrupts off and write;
     *  - other core registered for lockout: flash_safe_execute() parks it and masks IRQs;
     *  - other core not registered (boards whose Core1 never calls flash_safe_execute_core_init
     *    / multicore_lockout_victim_init): interrupts off and write, as before;
     *  - other core does not answer the lockout: skip the write rather than crash. */
    template <typename Fn>
    static bool run_flash_op(Fn fn)
    {
        if (!other_core_halted_.load(std::memory_order_acquire))
        {
            const int rc = flash_safe_execute([](void* p) { (*static_cast<Fn*>(p))(); }, &fn,
                                              SAFE_EXECUTE_TIMEOUT_MS);
            if (rc == PICO_OK)
            {
                return true;
            }
            if (rc != PICO_ERROR_NOT_PERMITTED)
            {
                return false;
            }
        }
        const uint32_t irq_state = save_and_disable_interrupts();
        fn();
        restore_interrupts(irq_state);
        return true;
    }

    inline Entry* get_entry(const uint32_t index) 
    {
        return reinterpret_cast<Entry*>(XIP_BASE + NVS_START_OFFSET + index * sizeof(Entry));
    }

    inline bool is_valid_entry(const Entry* entry) 
    {
        return std::strcmp(entry->key, INVALID_KEY) != 0;
    }

    inline bool valid_args(const std::string& key, size_t len)
    {
        return (key.size() < KEY_LEN_MAX - 1 && len <= sizeof(Entry::value) && std::strcmp(key.c_str(), INVALID_KEY) != 0);
    }

    bool update_entry(uint32_t index, const std::string& key, const void* buffer, size_t len)
    {
        uint32_t entry_offset = index * sizeof(Entry);
        uint32_t sector_offset = ((NVS_START_OFFSET + entry_offset) / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;

        /* 4 KB does not fit the 2 KB core stack; keep it static (guarded by nvs_mutex_). */
        static std::array<uint8_t, FLASH_SECTOR_SIZE> sector_buffer;
        std::memcpy(sector_buffer.data(), reinterpret_cast<const uint8_t*>(XIP_BASE + sector_offset), FLASH_SECTOR_SIZE);

        /* Entry offset is relative to the sector, not to NVS_START_OFFSET. */
        Entry* entry_to_write = reinterpret_cast<Entry*>(
            sector_buffer.data() + ((NVS_START_OFFSET + entry_offset) - sector_offset));

        *entry_to_write = Entry();
        std::strncpy(entry_to_write->key, key.c_str(), key.size());
        entry_to_write->key[key.size()] = '\0';
        std::memcpy(entry_to_write->value, buffer, len);

        return run_flash_op([sector_offset]() {
            flash_range_erase(sector_offset, FLASH_SECTOR_SIZE);
            for (uint32_t i = 0; i < FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE; ++i)
            {
                flash_range_program(sector_offset + i * FLASH_PAGE_SIZE,
                                    sector_buffer.data() + i * FLASH_PAGE_SIZE, FLASH_PAGE_SIZE);
            }
        });
    }

}; // class NVSTool

#endif // _NVS_TOOL_H_