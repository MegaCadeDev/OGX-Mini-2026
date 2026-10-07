#include "Diagnostics/Diagnostics.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "pico/critical_section.h"

namespace diag {

namespace {

constexpr size_t kHistBins = 102;  // 1 ms bins 0..100, then "more"

struct Event {
    uint32_t ms;
    uint8_t boot;  // low byte of the boot count when it was logged
    char text[kEventText];
};

// Report timing shared by Bluetooth and wired controllers.
struct Timing {
    uint32_t reports;
    uint32_t last_report_ms;
    uint32_t rate_hz;
    uint32_t rate_count;
    uint32_t gap_cur, gap_prev, gap_max;
    uint16_t hist[2][kHistBins];  // [current window, previous window]

    void report(uint32_t now_ms)
    {
        if (reports > 0) {
            const uint32_t gap = now_ms - last_report_ms;
            if (gap > gap_cur) gap_cur = gap;
            if (gap > gap_max) gap_max = gap;
            uint16_t& bin = hist[0][gap < kHistBins - 1 ? gap : kHistBins - 1];
            if (bin < 0xFFFF) ++bin;
        }
        last_report_ms = now_ms;
        ++reports;
        ++rate_count;
    }
    void second() { rate_hz = rate_count; rate_count = 0; }
    void window()
    {
        gap_prev = gap_cur;
        gap_cur = 0;
        std::memcpy(hist[1], hist[0], sizeof(hist[0]));
        std::memset(hist[0], 0, sizeof(hist[0]));
    }
    // Typical interval (median, ms) and the share of intervals longer than twice it (x10 %).
    void lateness(uint32_t& median_ms, uint32_t& late_pct_x10) const
    {
        uint32_t total = 0;
        for (size_t b = 0; b < kHistBins; ++b) total += hist[0][b] + hist[1][b];
        median_ms = 0;
        late_pct_x10 = 0;
        if (total < 10) return;
        uint32_t acc = 0;
        for (size_t b = 0; b < kHistBins; ++b) {
            acc += hist[0][b] + hist[1][b];
            if (acc * 2 >= total) { median_ms = static_cast<uint32_t>(b); break; }
        }
        const uint32_t limit = 2 * (median_ms ? median_ms : 1);
        uint32_t late = 0;
        for (size_t b = limit + 1; b < kHistBins; ++b) late += hist[0][b] + hist[1][b];
        late_pct_x10 = late * 1000 / total;
    }
    uint32_t recent_max_gap() const { return gap_cur > gap_prev ? gap_cur : gap_prev; }
};

// Values keyed by connection handle; some arrive before the slot is ready.
struct Link {
    uint16_t handle;
    bool used;
    uint16_t interval, latency, timeout;  // LE, 0 = unknown
    char manufacturer[kInfoText], model[kInfoText], firmware[kInfoText], hardware[kInfoText], software[kInfoText];
    bool pnp_valid;
    uint8_t pnp_source;
    uint16_t pnp_vid, pnp_pid, pnp_version;
    bool mode_known;
    uint8_t mode;              // 0 active, 1 hold, 2 sniff, 3 park
    uint16_t sniff_interval;   // 0.625 ms slots
    uint16_t mode_changes;
};

struct Slot {
    bool active;
    char name[kNameLength];
    uint16_t vid, pid;
    uint8_t controller_type;
    bool le;
    uint8_t oui[3];
    uint32_t connected_ms;
    Timing timing;
    uint16_t battery;  // 0xFFFF unknown, else 0-255
    // Controller's own counter.
    bool counter_seen;
    uint32_t counter_last, counter_received, counter_lost, counter_steps, counter_big_steps;
    uint32_t counter_step_hist[5];  // steps of 1, 2, 3, 4, 5 or more
    Link link;
};

struct UsbDevice {
    bool active;
    uint8_t address;
    uint16_t vid, pid, bcd;
    uint8_t speed;
    char driver[16];
    uint32_t connected_ms;
    Timing timing;
};

#pragma pack(push, 1)
struct SessionCtrl {
    uint16_t vid, pid;
    uint8_t le;
    uint16_t interval;
    uint16_t reports_per_s;
    uint16_t late_pct_x10;
    uint16_t lost_pct_x10;  // 0xFFFF unknown
    uint16_t max_gap_ms;
    uint8_t mode;              // 0xFF unknown
    uint16_t sniff_interval;
};
struct Session {
    uint8_t version;
    char mode[11];
    uint32_t uptime_s;
    uint32_t usb_reports_sent;
    uint32_t usb_configured_s;
    uint32_t latency_samples, latency_avg_us, latency_max_us;
    uint8_t controllers;
    uint8_t wired;
    SessionCtrl ctrl[2];
    uint16_t wired_vid, wired_pid, wired_reports_per_s, wired_max_gap_ms;
    // The last events of the session (kept in the same flash entry, no extra write).
    struct { uint32_t ms; char text[32]; } events[kSessionEvents];
};
#pragma pack(pop)
static_assert(sizeof(Session) <= kSessionBytes, "session summary fits its flash entry");
constexpr uint8_t kSessionVersion = 1;

critical_section_t s_lock;
bool s_lock_ready = false;
BoardInfo s_info{};
/* Kept across reboots that keep the RAM (see init()). On the device it sits in the SDK's
 * .uninitialized_data section, which the C runtime never clears. */
#if defined(PICO_ON_DEVICE) && PICO_ON_DEVICE
#define DIAG_PERSISTENT __attribute__((section(".uninitialized_data")))
#else
#define DIAG_PERSISTENT
#endif
constexpr uint32_t kPersistMagic = 0x4F474443;  // "OGDC"
constexpr uint32_t kCrashMagic = 0x43525348;    // "CRSH"
constexpr uint32_t kSessionMagic = 0x53455353;  // "SESS"
struct Persist {
    uint32_t magic;
    uint32_t boot;
    uint32_t next, count;
    uint32_t check;
    uint32_t crash_magic;
    CrashInfo crash;
    uint32_t session_magic;
    uint32_t session_stored;  // already written to flash
    Session session;  // the latest relevant session, kept until a newer one replaces it
    Event events[kEvents];
};
DIAG_PERSISTENT Persist s_p;
Event* const s_events = s_p.events;
uint32_t& s_event_next = s_p.next;
uint32_t& s_event_count = s_p.count;

uint32_t persist_check()
{
    return s_p.magic ^ s_p.boot ^ (s_p.next << 8) ^ (s_p.count << 16) ^ 0x5A5A5A5Au;
}

// Last crash for the report: from the boot before (recorded in RAM) or stored in flash.
CrashInfo s_last_crash{};
bool s_last_crash_valid = false;
bool s_last_crash_previous_boot = false;
Slot s_slots[kSlots];
Link s_pending[kSlots];
UsbDevice s_usb[kUsbDevices];
bool s_searching = false;
bool s_searching_known = false;
bool s_inquiry_seen = false;
uint32_t s_last_inquiry_ms = 0;
uint32_t s_inquiries = 0;
uint32_t s_adv_count = 0, s_adv_rate = 0;
constexpr uint32_t kInquiryRecentMs = 15000;  // periodic inquiry: one every 5-10 s while searching
uint32_t s_second_start = 0;
uint32_t s_window_start = 0;
// USB output
bool s_usb_configured = false, s_usb_suspended = false;
uint32_t s_usb_configured_since = 0, s_usb_configured_ms = 0;
uint32_t s_usb_sent = 0, s_usb_sent_rate = 0, s_usb_sent_count = 0;
LatencySource s_latency_source = nullptr;
/* Sessions for the report: [0] the latest relevant one, kept in RAM across reboots; [1] the one
 * stored in flash. */
Session s_sessions[2]{};
bool s_sessions_valid[2]{};
bool s_ram_session_stored = false;  // [0] is already in flash
// The RAM session a newer one replaced at this reboot, still to be stored in flash.
Session s_replaced{};
bool s_replaced_pending = false;
// The session that just ended (frozen before the pads go away); one per boot.
Session s_frozen{};
bool s_frozen_valid = false;

struct Lock {
    Lock() { if (s_lock_ready) critical_section_enter_blocking(&s_lock); }
    ~Lock() { if (s_lock_ready) critical_section_exit(&s_lock); }
};

Link* link_for_handle(uint16_t handle, bool create)
{
    for (auto& s : s_slots)
        if (s.active && s.link.used && s.link.handle == handle)
            return &s.link;
    for (auto& p : s_pending)
        if (p.used && p.handle == handle)
            return &p;
    if (!create)
        return nullptr;
    for (auto& p : s_pending) {
        if (!p.used) {
            p = Link{};
            p.used = true;
            p.handle = handle;
            return &p;
        }
    }
    s_pending[0] = Link{};
    s_pending[0].used = true;
    s_pending[0].handle = handle;
    return &s_pending[0];
}

void copy_text(char* dst, size_t len, const char* src)
{
    std::snprintf(dst, len, "%s", src ? src : "");
}

struct Writer {
    char* out;
    size_t cap;
    size_t len;

    void raw(const char* fmt, ...) __attribute__((format(printf, 2, 3)))
    {
        if (len + 1 >= cap)
            return;
        va_list ap;
        va_start(ap, fmt);
        const int n = std::vsnprintf(out + len, cap - len, fmt, ap);
        va_end(ap);
        if (n > 0)
            len += (static_cast<size_t>(n) < cap - len) ? static_cast<size_t>(n) : cap - len - 1;
    }
    void str(const char* s)
    {
        raw("\"");
        for (; s && *s; ++s) {
            const unsigned char c = static_cast<unsigned char>(*s);
            if (c == '"' || c == '\\') raw("\\%c", c);
            else if (c < 0x20 || c >= 0x7F) raw("\\u%04x", c);
            else raw("%c", c);
        }
        raw("\"");
    }
    void key_str(const char* key, const char* value) { raw(",\"%s\":", key); str(value); }
    void pct(const char* key, uint32_t x10) { raw(",\"%s\":%lu.%lu", key, static_cast<unsigned long>(x10 / 10), static_cast<unsigned long>(x10 % 10)); }
    void ms_0_625(const char* key, uint16_t slots)
    {
        const uint32_t t = static_cast<uint32_t>(slots) * 625;
        raw(",\"%s\":%lu.%02lu", key, static_cast<unsigned long>(t / 1000), static_cast<unsigned long>(t % 1000 / 10));
    }
    void ms_1_25(const char* key, uint16_t units)
    {
        const uint32_t h = static_cast<uint32_t>(units) * 125;
        raw(",\"%s\":%lu.%02lu", key, static_cast<unsigned long>(h / 100), static_cast<unsigned long>(h % 100));
    }
};

void write_timing(Writer& w, const Timing& t)
{
    uint32_t median = 0, late = 0;
    t.lateness(median, late);
    w.raw(",\"reports\":%lu,\"reports_per_s\":%lu,\"interval_median_ms\":%lu",
          static_cast<unsigned long>(t.reports), static_cast<unsigned long>(t.rate_hz),
          static_cast<unsigned long>(median));
    w.pct("late_reports_pct", late);
    w.raw(",\"max_gap_ms_recent\":%lu,\"max_gap_ms_since_connect\":%lu",
          static_cast<unsigned long>(t.recent_max_gap()), static_cast<unsigned long>(t.gap_max));
}

uint32_t counter_lost_x10(const Slot& s, bool& usable)
{
    // Lost reports skip counter values (steps of 2, 3...). A byte that is not a counter jumps
    // around (steps of half the range or more): then it is not trusted.
    usable = s.counter_steps >= 50 && s.counter_big_steps * 10 < s.counter_steps;
    const uint32_t total = s.counter_received + s.counter_lost;
    return (usable && total) ? s.counter_lost * 1000 / total : 0;
}

void write_link(Writer& w, const Link& l, bool le)
{
    if (le && l.interval) {
        w.ms_1_25("le_interval_ms", l.interval);
        w.raw(",\"le_latency\":%u,\"le_timeout_ms\":%u", l.latency, static_cast<unsigned>(l.timeout) * 10);
    }
    if (l.manufacturer[0]) w.key_str("manufacturer", l.manufacturer);
    if (l.model[0]) w.key_str("model", l.model);
    if (l.firmware[0]) w.key_str("firmware", l.firmware);
    if (l.hardware[0]) w.key_str("hardware", l.hardware);
    if (l.software[0]) w.key_str("software", l.software);
    if (l.pnp_valid)
        w.raw(",\"pnp\":{\"source\":%u,\"vid\":\"%04x\",\"pid\":\"%04x\",\"version\":\"%04x\"}",
              l.pnp_source, l.pnp_vid, l.pnp_pid, l.pnp_version);
    if (l.mode_known) {
        static const char* const kModes[] = {"active", "hold", "sniff", "park"};
        w.key_str("link_mode", l.mode < 4 ? kModes[l.mode] : "?");
        if (l.mode == 2) w.ms_0_625("sniff_interval_ms", l.sniff_interval);
        w.raw(",\"link_mode_changes\":%u", l.mode_changes);
    }
}

} // namespace

void init(const BoardInfo& info)
{
    /* The ring and the crash record survive a reboot that keeps the RAM; after a power-on (or
     * anything that left them inconsistent) they start empty. */
    const bool kept = s_p.magic == kPersistMagic && s_p.next < kEvents && s_p.count <= kEvents &&
                      s_p.check == persist_check();
    if (kept) {
        ++s_p.boot;
    } else {
        std::memset(&s_p, 0, sizeof(s_p));
        s_p.magic = kPersistMagic;
    }
    s_p.check = persist_check();
    s_last_crash = CrashInfo{};
    s_last_crash_valid = s_last_crash_previous_boot = false;
    for (bool& v : s_sessions_valid) v = false;
    s_replaced_pending = false;
    s_frozen_valid = false;
    if (kept && s_p.session_magic == kSessionMagic && s_p.session.version == kSessionVersion) {
        s_sessions[0] = s_p.session;  // stays in RAM until a newer relevant session replaces it
        s_sessions_valid[0] = true;
        s_ram_session_stored = s_p.session_stored != 0;
    } else {
        s_p.session_magic = 0;
        s_ram_session_stored = false;
    }
    if (kept && s_p.crash_magic == kCrashMagic) {
        s_last_crash = s_p.crash;
        s_last_crash_valid = true;
        s_last_crash_previous_boot = true;
    }
    s_p.crash_magic = 0;

    BoardInfo board = info;
    if (s_last_crash_valid && s_last_crash_previous_boot)
        board.reset_reason = "crash (see last_crash)";  // the crash handler reboots by watchdog

    if (!s_lock_ready) {
        /* A shared ("striped") spin lock, as the SDK's mutexes use: the claimable ones (24-31)
         * are taken by TaskQueue and TinyUSB, and claiming one more panics. */
        critical_section_init_with_lock_num(&s_lock, next_striped_spin_lock_num());
        s_lock_ready = true;
    }
    Lock l;
    s_info = board;
}

void event(uint32_t now_ms, const char* fmt, ...)
{
    char text[kEventText];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    Lock l;
    Event& e = s_events[s_event_next];
    e.ms = now_ms;
    e.boot = static_cast<uint8_t>(s_p.boot);
    std::memcpy(e.text, text, sizeof(text));
    s_event_next = (s_event_next + 1) % kEvents;
    if (s_event_count < kEvents) ++s_event_count;
    s_p.check = persist_check();
}

void crash_record(const CrashInfo& crash)
{
    s_p.crash = crash;
    s_p.crash_magic = kCrashMagic;
}

size_t new_crash(uint8_t* out, size_t out_len)
{
    static_assert(sizeof(CrashInfo) <= kCrashBytes, "crash record fits its flash entry");
    if (!out || out_len < sizeof(CrashInfo) || !s_last_crash_valid || !s_last_crash_previous_boot)
        return 0;
    std::memset(out, 0, out_len);
    std::memcpy(out, &s_last_crash, sizeof(CrashInfo));
    return sizeof(CrashInfo);
}

void set_stored_crash(const uint8_t* data, size_t len)
{
    if (!data || len < sizeof(CrashInfo) || s_last_crash_valid)
        return;
    CrashInfo c;
    std::memcpy(&c, data, sizeof(c));
    if (c.kind != 1 && c.kind != 2)
        return;
    s_last_crash = c;
    s_last_crash_valid = true;
    s_last_crash_previous_boot = false;
}

void slot_connected(size_t slot, uint32_t now_ms, const char* name, uint16_t vid, uint16_t pid,
                    uint8_t controller_type, bool le, uint16_t con_handle, const uint8_t address[6])
{
    if (slot >= kSlots) return;
    Lock l;
    Slot& s = s_slots[slot];
    s = Slot{};
    s.active = true;
    copy_text(s.name, sizeof(s.name), name);
    s.vid = vid;
    s.pid = pid;
    s.controller_type = controller_type;
    s.le = le;
    if (address) std::memcpy(s.oui, address, 3);  // the manufacturer part only (privacy)
    s.connected_ms = now_ms;
    s.battery = 0xFFFF;
    s.link.used = true;
    s.link.handle = con_handle;
    for (auto& p : s_pending) {
        if (p.used && p.handle == con_handle) {
            s.link = p;
            p.used = false;
        }
    }
}

void slot_disconnected(size_t slot, uint32_t now_ms)
{
    (void)now_ms;
    if (slot >= kSlots) return;
    Lock l;
    s_slots[slot].active = false;
}

void slot_report(size_t slot, uint32_t now_ms)
{
    if (slot >= kSlots) return;
    Lock l;
    if (s_slots[slot].active) s_slots[slot].timing.report(now_ms);
}

void slot_counter(size_t slot, uint32_t value, uint8_t bits)
{
    if (slot >= kSlots || bits == 0 || bits > 31) return;
    Lock l;
    Slot& s = s_slots[slot];
    if (!s.active) return;
    const uint32_t mask = (1u << bits) - 1;
    value &= mask;
    if (s.counter_seen) {
        const uint32_t step = (value - s.counter_last) & mask;
        if (step == 0) return;  // repeated report
        ++s.counter_steps;
        if (step >= (mask + 1) / 2) {
            ++s.counter_big_steps;
        } else {
            s.counter_lost += step - 1;
            ++s.counter_step_hist[step < 5 ? step - 1 : 4];
        }
    }
    s.counter_seen = true;
    s.counter_last = value;
    ++s.counter_received;
}

void slot_battery(size_t slot, uint8_t level)
{
    if (slot >= kSlots) return;
    Lock l;
    if (s_slots[slot].active) s_slots[slot].battery = level;
}

void le_parameters(uint16_t con_handle, uint16_t interval, uint16_t latency, uint16_t timeout)
{
    Lock l;
    Link* k = link_for_handle(con_handle, true);
    k->interval = interval;
    k->latency = latency;
    k->timeout = timeout;
}

void device_information(uint16_t con_handle, const char* field, const char* value)
{
    if (!field) return;
    Lock l;
    Link* k = link_for_handle(con_handle, true);
    char* dst = nullptr;
    if (!std::strcmp(field, "manufacturer")) dst = k->manufacturer;
    else if (!std::strcmp(field, "model")) dst = k->model;
    else if (!std::strcmp(field, "firmware")) dst = k->firmware;
    else if (!std::strcmp(field, "hardware")) dst = k->hardware;
    else if (!std::strcmp(field, "software")) dst = k->software;
    if (dst) copy_text(dst, kInfoText, value);
}

void pnp_id(uint16_t con_handle, uint8_t source, uint16_t vid, uint16_t pid, uint16_t version)
{
    Lock l;
    Link* k = link_for_handle(con_handle, true);
    k->pnp_valid = true;
    k->pnp_source = source;
    k->pnp_vid = vid;
    k->pnp_pid = pid;
    k->pnp_version = version;
}

void link_mode(uint16_t con_handle, uint8_t mode, uint16_t interval_slots)
{
    Lock l;
    if (Link* k = link_for_handle(con_handle, true)) {
        if (k->mode_known && k->mode != mode) ++k->mode_changes;
        k->mode_known = true;
        k->mode = mode;
        k->sniff_interval = interval_slots;
    }
}

void searching(bool accepting_new_controllers)
{
    Lock l;
    s_searching = accepting_new_controllers;
    s_searching_known = true;
}

void le_adv_report()
{
    Lock l;
    ++s_adv_count;
}

void inquiry_complete(uint32_t now_ms)
{
    Lock l;
    s_inquiry_seen = true;
    s_last_inquiry_ms = now_ms;
    ++s_inquiries;
}

void usb_mounted(uint8_t address, uint32_t now_ms, uint16_t vid, uint16_t pid, uint16_t bcd_device,
                 uint8_t speed, const char* driver)
{
    Lock l;
    UsbDevice* d = nullptr;
    for (auto& u : s_usb)
        if (u.active && u.address == address) d = &u;
    for (auto& u : s_usb)
        if (!d && !u.active) d = &u;
    if (!d) d = &s_usb[0];
    *d = UsbDevice{};
    d->active = true;
    d->address = address;
    d->vid = vid;
    d->pid = pid;
    d->bcd = bcd_device;
    d->speed = speed;
    copy_text(d->driver, sizeof(d->driver), driver);
    d->connected_ms = now_ms;
}

void usb_set_bcd_device(uint8_t address, uint16_t bcd_device)
{
    Lock l;
    for (auto& u : s_usb)
        if (u.active && u.address == address) u.bcd = bcd_device;
}

void usb_unmounted(uint8_t address)
{
    Lock l;
    for (auto& u : s_usb)
        if (u.active && u.address == address) u.active = false;
}

void usb_report(uint8_t address, uint32_t now_ms)
{
    Lock l;
    for (auto& u : s_usb)
        if (u.active && u.address == address) u.timing.report(now_ms);
}

void usb_output_state(uint32_t now_ms, bool configured, bool suspended)
{
    Lock l;
    if (configured && !s_usb_configured) s_usb_configured_since = now_ms;
    if (!configured && s_usb_configured) s_usb_configured_ms += now_ms - s_usb_configured_since;
    s_usb_configured = configured;
    s_usb_suspended = suspended;
}

void usb_report_sent()
{
    Lock l;
    ++s_usb_sent;
    ++s_usb_sent_count;
}

void tick(uint32_t now_ms)
{
    Lock l;
    if (now_ms - s_second_start >= 1000) {
        for (auto& s : s_slots) s.timing.second();
        for (auto& u : s_usb) u.timing.second();
        s_usb_sent_rate = s_usb_sent_count;
        s_usb_sent_count = 0;
        s_adv_rate = s_adv_count;
        s_adv_count = 0;
        s_second_start = now_ms;
    }
    if (now_ms - s_window_start >= kGapWindowMs) {
        for (auto& s : s_slots) s.timing.window();
        for (auto& u : s_usb) u.timing.window();
        s_window_start = now_ms;
    }
}

namespace {

bool web_app_session();

/* Worth keeping: not a Web App session, and a controller was in it or it lasted 30 s or more (a
 * mode passed through on the way to another one is not). */
bool session_relevant(const Session& ss)
{
    return !web_app_session() && (ss.controllers > 0 || ss.wired || ss.uptime_s >= 30);
}

bool web_app_session()
{
    return s_info.output_mode && std::strcmp(s_info.output_mode, "WEBAPP") == 0;
}

Session build_session(uint32_t now_ms, const Latency& latency);
bool session_relevant(const Session& ss);

Latency read_latency()
{
    return s_latency_source ? s_latency_source() : Latency{};
}
} // namespace

void set_latency_source(LatencySource source)
{
    s_latency_source = source;
}

void session_freeze(uint32_t now_ms)
{
    const Latency latency = read_latency();  // before the lock: it takes the gamepad's mutex
    Lock l;
    if (s_frozen_valid) return;
    s_frozen = build_session(now_ms, latency);
    s_frozen_valid = true;
    if (!session_relevant(s_frozen))
        return;  // the RAM keeps the latest relevant session
    if (s_sessions_valid[0] && !s_ram_session_stored) {
        s_replaced = s_sessions[0];  // to flash: take_replaced_session()
        s_replaced_pending = true;
    }
    s_p.session = s_frozen;  // kept across the reboot in RAM
    s_p.session_magic = kSessionMagic;
    s_p.session_stored = 0;
}

size_t session_capture(uint8_t* out, size_t out_len, uint32_t now_ms)
{
    if (!out || out_len < sizeof(Session)) return 0;
    const Latency latency = read_latency();
    Lock l;
    const Session ss = s_frozen_valid ? s_frozen : build_session(now_ms, latency);
    if (!session_relevant(ss)) return 0;
    std::memcpy(out, &ss, sizeof(ss));
    return sizeof(ss);
}

namespace {
Session build_session(uint32_t now_ms, const Latency& latency)
{
    Session ss{};
    ss.version = kSessionVersion;
    copy_text(ss.mode, sizeof(ss.mode), s_info.output_mode);
    ss.uptime_s = now_ms / 1000;
    ss.usb_reports_sent = s_usb_sent;
    uint32_t configured_ms = s_usb_configured_ms;
    if (s_usb_configured) configured_ms += now_ms - s_usb_configured_since;
    ss.usb_configured_s = configured_ms / 1000;
    ss.latency_samples = latency.samples;
    ss.latency_avg_us = latency.avg_us;
    ss.latency_max_us = latency.max_us;
    for (const auto& s : s_slots) {
        if (!s.active || ss.controllers >= 2) continue;
        SessionCtrl& c = ss.ctrl[ss.controllers++];
        c.vid = s.vid;
        c.pid = s.pid;
        c.le = s.le;
        c.interval = s.link.interval;
        const uint32_t secs = (now_ms - s.connected_ms) / 1000;
        c.reports_per_s = static_cast<uint16_t>(secs ? s.timing.reports / secs : s.timing.rate_hz);
        uint32_t median = 0, late = 0;
        s.timing.lateness(median, late);
        c.late_pct_x10 = static_cast<uint16_t>(late);
        bool usable = false;
        const uint32_t lost = counter_lost_x10(s, usable);
        c.lost_pct_x10 = usable ? static_cast<uint16_t>(lost) : 0xFFFF;
        c.max_gap_ms = static_cast<uint16_t>(s.timing.gap_max > 0xFFFF ? 0xFFFF : s.timing.gap_max);
        c.mode = s.link.mode_known ? s.link.mode : 0xFF;
        c.sniff_interval = s.link.sniff_interval;
    }
    for (const auto& u : s_usb) {
        if (!u.active || ss.wired) continue;
        ss.wired = 1;
        ss.wired_vid = u.vid;
        ss.wired_pid = u.pid;
        const uint32_t secs = (now_ms - u.connected_ms) / 1000;
        ss.wired_reports_per_s = static_cast<uint16_t>(secs ? u.timing.reports / secs : u.timing.rate_hz);
        ss.wired_max_gap_ms = static_cast<uint16_t>(u.timing.gap_max > 0xFFFF ? 0xFFFF : u.timing.gap_max);
    }
    const size_t n = s_event_count < kSessionEvents ? s_event_count : kSessionEvents;
    for (size_t i = 0; i < n; ++i) {
        const Event& e = s_events[(s_event_next + kEvents - n + i) % kEvents];
        ss.events[i].ms = e.ms;
        copy_text(ss.events[i].text, sizeof(ss.events[i].text), e.text);
    }
    return ss;
}
} // namespace

void set_stored_session(const uint8_t* data, size_t len)
{
    Lock l;
    const bool ok = data && len >= sizeof(Session) && data[0] == kSessionVersion;
    s_sessions_valid[1] = ok;
    if (ok) std::memcpy(&s_sessions[1], data, sizeof(Session));
}

size_t take_replaced_session(uint8_t* out, size_t out_len)
{
    Lock l;
    if (!s_replaced_pending || !out || out_len < sizeof(Session)) return 0;
    s_replaced_pending = false;
    std::memcpy(out, &s_replaced, sizeof(Session));
    return sizeof(Session);
}

void mark_session_stored()
{
    Lock l;
    s_p.session_stored = 1;
}


namespace {
void write_session(Writer& w, const Session& p)
{
    w.raw("{\"mode\":");
    w.str(p.mode);
    w.raw(",\"uptime_s\":%lu,\"usb_configured_s\":%lu,\"usb_reports_sent\":%lu",
          static_cast<unsigned long>(p.uptime_s), static_cast<unsigned long>(p.usb_configured_s),
          static_cast<unsigned long>(p.usb_reports_sent));
    if (p.usb_configured_s)
        w.raw(",\"usb_reports_sent_per_s\":%lu", static_cast<unsigned long>(p.usb_reports_sent / p.usb_configured_s));
    w.raw(",\"input_to_output_latency\":{\"samples\":%lu,\"avg_us\":%lu,\"max_us\":%lu},\"controllers\":[",
          static_cast<unsigned long>(p.latency_samples), static_cast<unsigned long>(p.latency_avg_us),
          static_cast<unsigned long>(p.latency_max_us));
    for (uint8_t i = 0; i < p.controllers && i < 2; ++i) {
        const SessionCtrl& c = p.ctrl[i];
        w.raw("%s{\"vid\":\"%04x\",\"pid\":\"%04x\",\"link\":\"%s\",\"reports_per_s\":%u", i ? "," : "",
              c.vid, c.pid, c.le ? "LE" : "Classic", c.reports_per_s);
        w.pct("late_reports_pct", c.late_pct_x10);
        if (c.lost_pct_x10 != 0xFFFF) w.pct("lost_reports_pct", c.lost_pct_x10);
        w.raw(",\"max_gap_ms\":%u", c.max_gap_ms);
        if (c.le && c.interval) w.ms_1_25("le_interval_ms", c.interval);
        if (c.mode == 0) w.raw(",\"link_mode\":\"active\"");
        if (c.mode == 2) {
            w.raw(",\"link_mode\":\"sniff\"");
            w.ms_0_625("sniff_interval_ms", c.sniff_interval);
        }
        w.raw("}");
    }
    w.raw("]");
    if (p.wired)
        w.raw(",\"wired\":{\"vid\":\"%04x\",\"pid\":\"%04x\",\"reports_per_s\":%u,\"max_gap_ms\":%u}",
              p.wired_vid, p.wired_pid, p.wired_reports_per_s, p.wired_max_gap_ms);
    w.raw(",\"last_events\":[");
    bool first_ev = true;
    for (size_t i = 0; i < kSessionEvents; ++i) {
        if (!p.events[i].text[0]) continue;
        char text[sizeof(p.events[i].text) + 1]{};
        std::memcpy(text, p.events[i].text, sizeof(p.events[i].text));
        w.raw("%s{\"ms\":%lu,\"text\":", first_ev ? "" : ",", static_cast<unsigned long>(p.events[i].ms));
        w.str(text);
        w.raw("}");
        first_ev = false;
    }
    w.raw("]}");
}

} // namespace

size_t report_json(char* out, size_t out_len, uint32_t now_ms)
{
    if (!out || out_len == 0) return 0;
    out[0] = '\0';
    Writer w{out, out_len, 0};
    Lock l;
    w.raw("{\"firmware\":");
    w.str(s_info.firmware_version);
    w.key_str("board", s_info.board);
    w.key_str("chip", s_info.chip);
    w.raw(",\"clock_mhz\":%lu", static_cast<unsigned long>(s_info.clock_mhz));
    w.key_str("build", s_info.build_type);
    w.key_str("output_mode", s_info.output_mode);
    w.raw(",\"max_gamepads\":%u,\"uptime_ms\":%lu", s_info.max_gamepads, static_cast<unsigned long>(now_ms));
    w.key_str("last_reset", s_info.reset_reason);
    w.raw(",\"bluetooth\":{\"bredr_inquiry_running\":%s,\"bredr_inquiries\":%lu",
          s_inquiry_seen && now_ms - s_last_inquiry_ms < kInquiryRecentMs ? "true" : "false",
          static_cast<unsigned long>(s_inquiries));
    w.raw(",\"le_scan_adv_reports_per_s\":%lu", static_cast<unsigned long>(s_adv_rate));
    if (s_searching_known)
        w.raw(",\"accepting_new_controllers\":%s", s_searching ? "true" : "false");
    w.raw("}");
    w.raw(",\"usb_output\":{\"configured\":%s,\"suspended\":%s,\"reports_sent\":%lu,\"reports_sent_per_s\":%lu}",
          s_usb_configured ? "true" : "false", s_usb_suspended ? "true" : "false",
          static_cast<unsigned long>(s_usb_sent), static_cast<unsigned long>(s_usb_sent_rate));

    w.raw(",\"controllers\":[");
    bool first = true;
    for (size_t i = 0; i < kSlots; ++i) {
        const Slot& s = s_slots[i];
        if (!s.active) continue;
        w.raw(first ? "{" : ",{");
        first = false;
        w.raw("\"slot\":%u", static_cast<unsigned>(i));
        w.key_str("name", s.name);
        w.raw(",\"vid\":\"%04x\",\"pid\":\"%04x\",\"type\":%u,\"link\":\"%s\",\"bt_address_prefix\":\"%02X:%02X:%02X\",\"connected_s\":%lu",
              s.vid, s.pid, s.controller_type, s.le ? "LE" : "Classic", s.oui[0], s.oui[1], s.oui[2],
              static_cast<unsigned long>((now_ms - s.connected_ms) / 1000));
        if (s.battery != 0xFFFF) w.raw(",\"battery_pct\":%u", static_cast<unsigned>(s.battery * 100 / 255));
        write_timing(w, s.timing);
        if (s.counter_seen) {
            bool usable = false;
            const uint32_t lost = counter_lost_x10(s, usable);
            if (usable) {
                w.pct("lost_reports_pct", lost);
                // How many values the counter usually advances between received reports (1 = none lost).
                uint32_t mode = 0;
                for (uint32_t i = 1; i < 5; ++i)
                    if (s.counter_step_hist[i] > s.counter_step_hist[mode]) mode = i;
                w.raw(",\"counter_usual_step\":\"%lu%s\"", static_cast<unsigned long>(mode + 1), mode == 4 ? "+" : "");
            } else {
                w.raw(",\"lost_reports_pct\":null");
            }
        }
        write_link(w, s.link, s.le);
        w.raw("}");
    }

    w.raw("],\"wired_controllers\":[");
    first = true;
    for (const auto& u : s_usb) {
        if (!u.active) continue;
        w.raw(first ? "{" : ",{");
        first = false;
        static const char* const kSpeeds[] = {"full", "low", "high"};
        w.raw("\"address\":%u,\"vid\":\"%04x\",\"pid\":\"%04x\",\"bcd_device\":\"%04x\",\"speed\":\"%s\"",
              u.address, u.vid, u.pid, u.bcd, u.speed < 3 ? kSpeeds[u.speed] : "?");
        w.key_str("connection", is_wireless_receiver(u.vid, u.pid) ? "2.4 GHz receiver" : "cable (or unknown receiver)");
        w.key_str("driver", u.driver);
        w.raw(",\"connected_s\":%lu", static_cast<unsigned long>((now_ms - u.connected_ms) / 1000));
        write_timing(w, u.timing);
        w.raw("}");
    }
    w.raw("]");

    if (s_sessions_valid[0]) {
        w.raw(",\"previous_session\":");
        write_session(w, s_sessions[0]);
    }
    // The flash copy, unless it is the same session as the one in RAM.
    if (s_sessions_valid[1] &&
        !(s_sessions_valid[0] && std::memcmp(&s_sessions[0], &s_sessions[1], sizeof(Session)) == 0)) {
        w.raw(",\"stored_session\":");
        write_session(w, s_sessions[1]);
    }

    if (s_last_crash_valid) {
        const CrashInfo& k = s_last_crash;
        w.raw(",\"last_crash\":{\"when\":\"%s\",\"kind\":\"%s\",\"core\":%u,\"uptime_ms\":%lu",
              s_last_crash_previous_boot ? "previous boot" : "stored (an earlier boot)",
              k.kind == 2 ? "panic" : "hard fault", k.core, static_cast<unsigned long>(k.uptime_ms));
        if (k.kind == 1)
            w.raw(",\"pc\":\"0x%08lx\",\"lr\":\"0x%08lx\",\"xpsr\":\"0x%08lx\"", static_cast<unsigned long>(k.pc),
                  static_cast<unsigned long>(k.lr), static_cast<unsigned long>(k.xpsr));
        if (k.has_fault_regs)
            w.raw(",\"cfsr\":\"0x%08lx\",\"hfsr\":\"0x%08lx\",\"mmfar\":\"0x%08lx\",\"bfar\":\"0x%08lx\"",
                  static_cast<unsigned long>(k.cfsr), static_cast<unsigned long>(k.hfsr),
                  static_cast<unsigned long>(k.mmfar), static_cast<unsigned long>(k.bfar));
        if (k.message[0]) {
            char msg[sizeof(k.message) + 1]{};
            std::memcpy(msg, k.message, sizeof(k.message));
            w.key_str("message", msg);
        }
        w.raw("}");
    }

    /* Events, oldest first; this boot's and the ones before a reboot that kept the RAM. The
     * oldest are left out when the report would not fit (it must stay valid JSON). */
    w.raw(",\"events\":[");
    size_t shown = 0, budget = w.cap > w.len + 64 ? w.cap - w.len - 64 : 0;
    while (shown < s_event_count) {
        const Event& e = s_events[(s_event_next + kEvents - 1 - shown) % kEvents];
        const size_t need = 48 + std::strlen(e.text) * 2;  // worst case escaping
        if (need > budget) break;
        budget -= need;
        ++shown;
    }
    for (size_t n = 0; n < shown; ++n) {
        const Event& e = s_events[(s_event_next + kEvents - shown + n) % kEvents];
        const int boot = static_cast<int>(static_cast<int8_t>(static_cast<uint8_t>(e.boot - static_cast<uint8_t>(s_p.boot))));
        w.raw("%s{\"ms\":%lu", n ? "," : "", static_cast<unsigned long>(e.ms));
        if (boot) w.raw(",\"boot\":%d", boot);
        w.raw(",\"text\":");
        w.str(e.text);
        w.raw("}");
    }
    w.raw("]");
    if (shown < s_event_count) w.raw(",\"events_left_out\":%lu", static_cast<unsigned long>(s_event_count - shown));
    w.raw("}");
    return w.len;
}

bool is_wireless_receiver(uint16_t vid, uint16_t pid)
{
    struct Id { uint16_t vid, pid; };
    static const Id kReceivers[] = {
        {0x045e, 0x0719}, {0x045e, 0x0291}, {0x045e, 0x02a9},  // Xbox 360 wireless receivers
        {0x045e, 0x02e6}, {0x045e, 0x02fe}, {0x045e, 0x091e},  // Xbox Wireless Adapter
        {0x054c, 0x0ba0},                                      // DualShock 4 USB wireless adapter
        {0x2dc8, 0x3106}, {0x2dc8, 0x3109},                    // 8BitDo USB wireless adapters
    };
    for (const auto& r : kReceivers)
        if (r.vid == vid && r.pid == pid) return true;
    return false;
}

void reset_for_tests()
{
    Lock l;
    s_info = BoardInfo{};
    std::memset(&s_p, 0, sizeof(s_p));
    s_last_crash = CrashInfo{};
    s_last_crash_valid = s_last_crash_previous_boot = false;
    for (auto& s : s_slots) s = Slot{};
    for (auto& p : s_pending) p = Link{};
    for (auto& u : s_usb) u = UsbDevice{};
    s_searching = s_searching_known = false;
    s_inquiry_seen = false;
    s_last_inquiry_ms = 0;
    s_inquiries = 0;
    s_adv_count = s_adv_rate = 0;
    s_second_start = s_window_start = 0;
    s_usb_configured = s_usb_suspended = false;
    s_usb_configured_since = s_usb_configured_ms = 0;
    s_usb_sent = s_usb_sent_rate = s_usb_sent_count = 0;
    for (int k = 0; k < 2; ++k) {
        s_sessions[k] = Session{};
        s_sessions_valid[k] = false;
    }
    s_ram_session_stored = false;
    s_replaced_pending = false;
    s_frozen = Session{};
    s_frozen_valid = false;
}

} // namespace diag
