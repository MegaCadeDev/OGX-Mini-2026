#include <cstring>

#include "pico/time.h"

#include "Board/ogxm_log.h"
#include "USBDevice/DeviceDriver/Switch/Switch.h"
#include "Gamepad/Gamepad.h"
#include "Gamepad/Range.h"

namespace
{
	constexpr uint8_t VIB_OPTS[4] = { 0x0A, 0x0C, 0x0B, 0x09 };
}

void SwitchDevice::initialize()
{
	class_driver_ = {
		.name = TUD_DRV_NAME("SWITCH"),
		.init = hidd_init,
		.deinit = hidd_deinit,
		.reset = hidd_reset,
		.open = hidd_open,
		.control_xfer_cb = hidd_control_xfer_cb,
		.xfer_cb = hidd_xfer_cb,
		.sof = NULL
	};
	switch_report_.batteryConnection = 0x81;
	std::memset(report_.data(), 0, report_.size());
	std::memset(pending_output_.data(), 0, pending_output_.size());
	std::memset(report_81_.data(), 0, report_81_.size());
	has_pending_81_ = false;
	OGXM_LOG("Switch: device init (Pro Controller emulation)\n");
}

void SwitchDevice::set_timer()
{
	if (timestamp_ms_ == 0)
	{
		timestamp_ms_ = to_ms_since_boot(get_absolute_time());
		report_[0] = 0x00;
		return;
	}
	uint32_t now = to_ms_since_boot(get_absolute_time());
	uint32_t delta = now - timestamp_ms_;
	uint32_t ticks = static_cast<uint32_t>(delta * 4u);
	timer_ = (timer_ + static_cast<uint8_t>(ticks)) & 0xFF;
	report_[0] = timer_;
	timestamp_ms_ = now;
}

void SwitchDevice::gamepad_to_switch_report(const Gamepad::PadIn& gp_in, SwitchPro::SwitchReport& out, const Gamepad& gamepad)
{
	out.batteryConnection = 0x81;
	out.buttons[0] = 0;
	out.buttons[1] = 0;
	out.buttons[2] = 0;

	// Face: match project convention. SwitchPro host (SwitchPro.cpp) maps Pro→gamepad as:
	// Pro Y→MAP_X, Pro B→MAP_A, Pro A→MAP_B, Pro X→MAP_Y. So device must output inverse:
	// internal A→Pro B, B→Pro A, X→Pro Y, Y→Pro X. (PS: Cross=A→Pro B, Circle=B→Pro A, Square=X→Pro Y, Triangle=Y→Pro X.)
	if (gp_in.buttons & Gamepad::BUTTON_A) out.buttons[0] |= SwitchPro::Btn::B;
	if (gp_in.buttons & Gamepad::BUTTON_B) out.buttons[0] |= SwitchPro::Btn::A;
	if (gp_in.buttons & Gamepad::BUTTON_X) out.buttons[0] |= SwitchPro::Btn::Y;
	if (gp_in.buttons & Gamepad::BUTTON_Y) out.buttons[0] |= SwitchPro::Btn::X;
	if (gp_in.buttons & gamepad.MAP_BUTTON_RB) out.buttons[0] |= SwitchPro::Btn::R;
	if (gp_in.trigger_r)                       out.buttons[0] |= SwitchPro::Btn::ZR;

	if (gp_in.buttons & gamepad.MAP_BUTTON_BACK)  out.buttons[1] |= SwitchPro::Btn::MINUS;
	if (gp_in.buttons & gamepad.MAP_BUTTON_START) out.buttons[1] |= SwitchPro::Btn::PLUS;
	if (gp_in.buttons & gamepad.MAP_BUTTON_L3)    out.buttons[1] |= SwitchPro::Btn::L3;
	if (gp_in.buttons & gamepad.MAP_BUTTON_R3)    out.buttons[1] |= SwitchPro::Btn::R3;
	if (gp_in.buttons & gamepad.MAP_BUTTON_SYS)   out.buttons[1] |= SwitchPro::Btn::HOME;
	if (gp_in.buttons & gamepad.MAP_BUTTON_MISC)  out.buttons[1] |= SwitchPro::Btn::CAPTURE;

	uint8_t hat = SwitchPro::HAT_NEUTRAL;
	if (gp_in.dpad & gamepad.MAP_DPAD_UP)         hat |= SwitchPro::HAT_UP;
	if (gp_in.dpad & gamepad.MAP_DPAD_DOWN)       hat |= SwitchPro::HAT_DOWN;
	if (gp_in.dpad & gamepad.MAP_DPAD_LEFT)       hat |= SwitchPro::HAT_LEFT;
	if (gp_in.dpad & gamepad.MAP_DPAD_RIGHT)      hat |= SwitchPro::HAT_RIGHT;
	out.buttons[2] = hat;
	if (gp_in.buttons & gamepad.MAP_BUTTON_LB) out.buttons[2] |= SwitchPro::Btn::L;
	if (gp_in.trigger_l)                      out.buttons[2] |= SwitchPro::Btn::ZL;

	// Sticks: int16 -> 12-bit (0x000–0xFFF), center 0x7FF. Small deadzone.
	constexpr int16_t DZ = 512;
	auto to12 = [](int16_t val) -> uint16_t {
		if (val > -DZ && val < DZ)
			return SwitchPro::STICK_MID;
		int32_t v = static_cast<int32_t>(val) + 32768;
		int32_t u = (v * 4095 + 32767) / 65535;
		if (u < 0) u = 0;
		if (u > 4095) u = 4095;
		return static_cast<uint16_t>(u);
	};
	uint16_t lx = to12(gp_in.joystick_lx);
	uint16_t ly = to12(gp_in.joystick_ly);
	uint16_t rx = to12(gp_in.joystick_rx);
	uint16_t ry = to12(gp_in.joystick_ry);
	// Invert Y axes so up/down match Switch expectation (both sticks were reversed)
	ly = static_cast<uint16_t>(0xFFF - ly);
	ry = static_cast<uint16_t>(0xFFF - ry);

	out.l[0] = static_cast<uint8_t>(lx & 0xFF);
	out.l[1] = static_cast<uint8_t>(((ly & 0x0F) << 4) | (lx >> 8));
	out.l[2] = static_cast<uint8_t>(ly >> 4);
	out.r[0] = static_cast<uint8_t>(rx & 0xFF);
	out.r[1] = static_cast<uint8_t>(((ry & 0x0F) << 4) | (rx >> 8));
	out.r[2] = static_cast<uint8_t>(ry >> 4);
}

void SwitchDevice::build_standard_report(const SwitchPro::SwitchReport& sw)
{
	std::memset(report_.data(), 0, report_.size());
	set_timer();
	std::memcpy(report_.data() + 1, &sw, sizeof(SwitchPro::SwitchReport));
	if (vibration_enabled_)
	{
		vibration_idx_ = (vibration_idx_ + 1) % 4;
		vibration_report_ = VIB_OPTS[vibration_idx_];
	}
	report_[11] = vibration_report_;
}

void SwitchDevice::build_subcommand_reply(const SwitchPro::SwitchReport& sw)
{
	std::memset(report_.data(), 0, report_.size());
	set_timer();
	std::memcpy(report_.data() + 1, &sw, sizeof(SwitchPro::SwitchReport));
	report_[11] = vibration_report_;
	// Reply data starts at byte 12 in payload (timer=0, SwitchReport=1-10, vib=11, reply=12+)
	uint8_t* r = report_.data() + 12;

	// Subcommand at byte 10 of host output report (retro-pico-switch layout)
	uint8_t sub = pending_output_[10];
	OGXM_LOG("Switch: replying to subcmd=0x%02x\n", (unsigned)sub);
	switch (sub)
	{
		case SwitchPro::SUBCMD_BT_PAIR:
			r[0] = 0x81; r[1] = 0x01; r[2] = 0x03;
			break;
		case SwitchPro::SUBCMD_DEVICE_INFO:
			r[0] = 0x82; r[1] = 0x02; r[2] = 0x03; r[3] = 0x48; r[4] = 0x03; r[5] = 0x02;
			std::memcpy(r + 6, addr_.data(), 6);
			r[12] = 0x01; r[13] = 0x01;
			break;
		case SwitchPro::SUBCMD_SHIPMENT:
			r[0] = 0x80; r[1] = 0x08;
			break;
		case SwitchPro::SUBCMD_SET_MODE:
			r[0] = 0x80; r[1] = 0x03;
			break;
		case SwitchPro::SUBCMD_TRIGGER:
			r[0] = 0x83; r[1] = 0x04;
			break;
		case SwitchPro::SUBCMD_IMU:
			r[0] = 0x80; r[1] = 0x40;
			break;
		case SwitchPro::SUBCMD_IMU_SENS:
			r[0] = 0x80; r[1] = 0x41;
			break;
		case SwitchPro::SUBCMD_VIBRATION:
			r[0] = 0x80; r[1] = 0x48;
			vibration_enabled_ = true;
			vibration_idx_ = 0;
			vibration_report_ = VIB_OPTS[0];
			break;
		case SwitchPro::SUBCMD_SET_PLAYER:
			r[0] = 0x80; r[1] = 0x30;
			break;
		case SwitchPro::SUBCMD_NFC_IR_STATE:
			r[0] = 0x80; r[1] = 0x22;
			break;
		case SwitchPro::SUBCMD_NFC_IR_CFG:
			r[0] = 0xA0; r[1] = 0x21;
			r[2] = 0x01; r[3] = 0x00; r[4] = 0xFF; r[5] = 0x00; r[6] = 0x08; r[7] = 0x00; r[8] = 0x1B; r[9] = 0x01;
			r[37] = 0xC8;
			break;
		case SwitchPro::SUBCMD_SPI_READ:
		{
			r[0] = 0x90; r[1] = 0x10;
			uint8_t addr_lo = pending_output_[11];
			uint8_t addr_hi = pending_output_[12];
			uint8_t read_len = pending_output_[15];
			r[2] = addr_lo; r[3] = addr_hi; r[6] = read_len;
			if (addr_hi == 0x60 && addr_lo == 0x00)
				std::memset(r + 7, 0xFF, 16);
			else if (addr_hi == 0x60 && addr_lo == 0x50)
			{
				std::memset(r + 7, 0x32, 3);
				std::memset(r + 10, 0xFF, 3);
				std::memset(r + 13, 0xFF, 7);
			}
			else if (addr_hi == 0x60 && addr_lo == 0x80)
			{
				r[7] = 0x50; r[8] = 0xFD; r[9] = 0x00; r[10] = 0x00; r[11] = 0xC6; r[12] = 0x0F;
				const uint8_t params[18] = { 0x0F, 0x30, 0x61, 0x96, 0x30, 0xF3, 0xD4, 0x14, 0x54, 0x41, 0x15, 0x54, 0xC7, 0x79, 0x9C, 0x33, 0x36, 0x63 };
				std::memcpy(r + 13, params, 18);
			}
			else if (addr_hi == 0x60 && addr_lo == 0x98)
			{
				const uint8_t params[18] = { 0x0F, 0x30, 0x61, 0x96, 0x30, 0xF3, 0xD4, 0x14, 0x54, 0x41, 0x15, 0x54, 0xC7, 0x79, 0x9C, 0x33, 0x36, 0x63 };
				std::memcpy(r + 7, params, 18);
			}
			else if (addr_hi == 0x80 && addr_lo == 0x10)
				std::memset(r + 7, 0xFF, 3);
			else if (addr_hi == 0x60 && addr_lo == 0x3D)
			{
				const uint8_t l_cal[9] = { 0xD4, 0x75, 0x61, 0xE5, 0x87, 0x7C, 0xEC, 0x55, 0x61 };
				const uint8_t r_cal[9] = { 0x5D, 0xD8, 0x7F, 0x18, 0xE6, 0x61, 0x86, 0x65, 0x5D };
				std::memcpy(r + 7, l_cal, 9);
				std::memcpy(r + 16, r_cal, 9);
				r[25] = 0xFF;
				std::memset(r + 26, 0x32, 3);
				std::memset(r + 29, 0xFF, 3);
			}
			else if (addr_hi == 0x60 && addr_lo == 0x20)
			{
				const uint8_t sa[24] = { 0xCC, 0x00, 0x40, 0x00, 0x91, 0x01, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0xE7, 0xFF, 0x0E, 0x00, 0xDC, 0xFF, 0x3B, 0x34, 0x3B, 0x34, 0x3B, 0x34 };
				std::memcpy(r + 7, sa, 24);
			}
			else
				std::memset(r + 7, 0xFF, read_len);
			break;
		}
		default:
			r[0] = 0x80; r[1] = sub;
			break;
	}
}

void SwitchDevice::process(const uint8_t idx, Gamepad& gamepad)
{
	(void)idx;
	Gamepad::PadIn gp_in = gamepad.get_pad_in();
	gamepad_to_switch_report(gp_in, switch_report_, gamepad);

	if (tud_suspended())
		tud_remote_wakeup();

	// USB init: host reads 0x81 from interrupt IN after sending 0x80 0x01/0x02/ etc. Push it first.
	if (has_pending_81_ && tud_hid_n_ready(0))
	{
		tud_hid_n_report(0, SwitchPro::REPORT_ID_USB_INIT, report_81_.data() + 1, 63);
		has_pending_81_ = false;
		return;
	}

	bool is_subcommand = has_pending_output_;
	if (has_pending_output_)
		build_subcommand_reply(switch_report_);
	else
		build_standard_report(switch_report_);

	if (tud_hid_n_ready(0))
	{
		tud_hid_n_report(0, is_subcommand ? SwitchPro::REPORT_ID_SUBCMD : SwitchPro::REPORT_ID_STANDARD, report_.data(), 63);
		if (is_subcommand)
		{
			has_pending_output_ = false;
			std::memset(pending_output_.data(), 0, pending_output_.size());
		}
	}
	else if (is_subcommand)
		OGXM_LOG("Switch: HID not ready, subcommand reply dropped\n");
}

uint16_t SwitchDevice::get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
	(void)itf;
	// Log init and subcommand report requests (standard 0x30 polling is very frequent)
	if (report_id == SwitchPro::REPORT_ID_USB_INIT)
		OGXM_LOG("Switch: get_report id=0x81 (USB init) reqlen=%u\n", (unsigned)reqlen);
	if (report_id == SwitchPro::REPORT_ID_SUBCMD)
		OGXM_LOG("Switch: get_report id=0x21 (subcmd reply) reqlen=%u\n", (unsigned)reqlen);
	if (report_type != HID_REPORT_TYPE_INPUT)
	{
		OGXM_LOG("Switch: get_report rejected (bad type)\n");
		return 0;
	}
	// Report ID 0x81: USB init handshake reply (MAC, handshake, timeout ack)
	if (report_id == SwitchPro::REPORT_ID_USB_INIT)
	{
		if (!has_pending_81_ || reqlen < 2)
			return 0;
		uint16_t copy_len = reqlen < SwitchPro::USB_INIT_REPORT_SIZE ? reqlen : SwitchPro::USB_INIT_REPORT_SIZE;
		std::memcpy(buffer, report_81_.data(), copy_len);
		has_pending_81_ = false;
		return copy_len;
	}
	if (report_id != SwitchPro::REPORT_ID_STANDARD && report_id != SwitchPro::REPORT_ID_SUBCMD)
	{
		OGXM_LOG("Switch: get_report rejected (bad id)\n");
		return 0;
	}
	if (reqlen < SwitchPro::REPORT_SIZE)
	{
		OGXM_LOG("Switch: get_report rejected (reqlen %u < %d)\n", (unsigned)reqlen, SwitchPro::REPORT_SIZE);
		return 0;
	}
	buffer[0] = report_id;
	std::memcpy(buffer + 1, report_.data(), 63);
	return SwitchPro::REPORT_SIZE;
}

void SwitchDevice::set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
	(void)itf;
	(void)report_type;
	OGXM_LOG("Switch: set_report id=0x%02x type=%u len=%u\n", (unsigned)report_id, (unsigned)report_type, (unsigned)bufsize);
	if (bufsize > 12)
		OGXM_LOG("Switch: cmd=0x%02x seq=0x%02x subcmd=0x%02x\n",
			(unsigned)buffer[0], (unsigned)buffer[1], (unsigned)buffer[10]);
	OGXM_LOG_HEX(buffer, bufsize > 64 ? 64 : bufsize);
	if (bufsize > SwitchPro::REPORT_SIZE)
		bufsize = SwitchPro::REPORT_SIZE;
	std::memcpy(pending_output_.data(), buffer, bufsize);
	// USB init: 2-byte reports [0x80, subtype] require reply on report ID 0x81 (Chromium / Switch console).
	if (bufsize >= 2 && buffer[0] == 0x80)
	{
		uint8_t subtype = buffer[1];
		std::memset(report_81_.data(), 0, report_81_.size());
		report_81_[0] = SwitchPro::REPORT_ID_USB_INIT;
		report_81_[1] = subtype;
		if (subtype == 0x01) // Request MAC: subtype, padding, device_type (0x03 = Pro), mac[6]
		{
			report_81_[2] = 0x00;
			report_81_[3] = 0x03; // kUsbDeviceTypeProController
			std::memcpy(report_81_.data() + 4, addr_.data(), 6);
		}
		has_pending_81_ = true;
	}
	// Only queue a subcommand reply when the report has a subcommand at byte 10 (len >= 11).
	else if (bufsize >= 11)
		has_pending_output_ = true;
	// Rumble: report_id 0x01/0x10/0x11, bytes 2-5 and 6-9 contain rumble data; we don't drive physical rumble here
}

bool SwitchDevice::vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request)
{
	(void)rhport;
	OGXM_LOG("Switch: vendor_control stage=%u bmReqType=0x%02x bReq=0x%02x wVal=0x%04x wIdx=0x%04x wLen=%u\n",
		(unsigned)stage, (unsigned)request->bmRequestType, (unsigned)request->bRequest,
		(unsigned)request->wValue, (unsigned)request->wIndex, (unsigned)request->wLength);
	return false;
}

const uint16_t* SwitchDevice::get_descriptor_string_cb(uint8_t index, uint16_t langid)
{
	(void)langid;
	OGXM_LOG("Switch: get_descriptor_string index=%u\n", (unsigned)index);
	if (index == 0)
	{
		static uint16_t lang[2] = { 0x0304, 0x0409 };
		return lang;
	}
	const char* str = nullptr;
	if (index == 1) str = SwitchPro::STRING_MANUFACTURER;
	else if (index == 2) str = SwitchPro::STRING_PRODUCT;
	else if (index == 3) str = SwitchPro::STRING_VERSION;
	if (!str)
		return nullptr;
	return get_string_descriptor(str, index);
}

const uint8_t* SwitchDevice::get_descriptor_device_cb()
{
	OGXM_LOG("Switch: get_descriptor device\n");
	return SwitchPro::DEVICE_DESCRIPTOR;
}

const uint8_t* SwitchDevice::get_hid_descriptor_report_cb(uint8_t itf)
{
	(void)itf;
	OGXM_LOG("Switch: get_descriptor HID report\n");
	return SwitchPro::REPORT_DESCRIPTOR;
}

const uint8_t* SwitchDevice::get_descriptor_configuration_cb(uint8_t index)
{
	(void)index;
	OGXM_LOG("Switch: get_descriptor configuration\n");
	return SwitchPro::CONFIGURATION_DESCRIPTOR;
}

const uint8_t* SwitchDevice::get_descriptor_device_qualifier_cb()
{
	return nullptr;
}
