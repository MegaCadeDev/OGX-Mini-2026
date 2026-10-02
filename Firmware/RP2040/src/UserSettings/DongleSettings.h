#ifndef _OGXM_DONGLE_SETTINGS_H_
#define _OGXM_DONGLE_SETTINGS_H_

#include <cstddef>
#include <cstdint>

/*  Adapter options: dongle-wide settings kept in flash and changed from the web app (over USB
 *  and Bluetooth). Each option is one byte (0 / 1) at a fixed position; its default comes from
 *  a CMake option, so a build can pick the defaults and users can still change them.
 *
 *  Settings is also the wire format (web app) and the flash format: 8 bytes, a version byte
 *  then one byte per option.
 *
 *  Adding an option: give it the next free index in Option, set its default in defaults()
 *  from a CMake option, and read it with enabled(). Indices are part of the wire format and
 *  of stored settings: never renumber them.
 */
namespace dongle_settings {

    constexpr uint8_t kVersion = 1;
    constexpr size_t kOptionCount = 7;

    enum class Option : uint8_t {
        // Indices into Settings::option (wire bytes 1..7), added by the features using them.
    };

#pragma pack(push, 1)
    struct Settings {
        uint8_t version;
        uint8_t option[kOptionCount];
    };
#pragma pack(pop)
    static_assert(sizeof(Settings) == 8, "dongle_settings::Settings is a wire format");

    // Build-time defaults.
    Settings defaults();

    // Parse stored / received bytes. False (out untouched) for a short buffer or another
    // version; option bytes are normalised to 0 / 1.
    bool decode(const uint8_t* data, size_t len, Settings& out);

    // Current settings (defaults until set() is called at boot with the stored ones).
    const Settings& get();
    void set(const Settings& settings);

    inline bool enabled(Option option)
    {
        return get().option[static_cast<uint8_t>(option)] != 0;
    }

} // namespace dongle_settings

#endif // _OGXM_DONGLE_SETTINGS_H_
