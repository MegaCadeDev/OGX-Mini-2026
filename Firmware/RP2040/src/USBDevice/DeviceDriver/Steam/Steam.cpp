#include <algorithm>
#include <cstddef>
#include <cstring>

#include "pico/time.h"
#include "Board/BoardMac.h"
#include "Gamepad/MotionImu.h"
#include "USBDevice/DeviceDriver/Sony/SonyImu.h"
#include "USBDevice/DeviceDriver/Sony/SonyReports.h"
#include "USBDevice/DeviceDriver/Steam/Steam.h"
#include "USBDevice/DeviceDriver/Steam/SteamPassthrough.h"
#include "USBDevice/DeviceDriver/Steam/SteamTouchpad.h"
#include "USBDevice/DeviceDriver/Steam/SteamBtReport.h"
#include "Descriptors/PS5Usb.h"
#include "Descriptors/Steam.h"

namespace {

constexpr uint8_t kReportIdIn = 0x01;

void init_neutral_report(std::array<uint8_t, SteamPassthrough::USB_REPORT_SIZE>& rep)
{
	std::memset(rep.data(), 0, rep.size());
	rep[0] = kReportIdIn;
	rep[1] = rep[2] = rep[3] = rep[4] = PS5::JOYSTICK_MID;
}

/* Fields a synthesized DualSense report (any pad but a real DualSense) used to leave at zero;
 * see sony_reports::ds5_fill_synth. */
void add_synth_fields(std::array<uint8_t, SteamPassthrough::USB_REPORT_SIZE>& rep,
                      const Gamepad::PadIn& gp_in, uint8_t seq)
{
	sony_reports::Ds5SynthInput in{};
	in.seq = seq;
	in.has_motion = gp_in.has_motion();
	if (in.has_motion) {
		for (int i = 0; i < 3; ++i) {
			in.gyro[i] = gp_in.gyro[i];
			in.accel[i] = gp_in.accel[i];
		}
		MotionImu::remap_to_ds4_playing_frame(gp_in.motion_source, in.accel, in.gyro);
	}
	in.time_us = time_us_64();
	in.touch_raw = gp_in.touch_raw;
	in.touch_valid = gp_in.touchpad_valid != 0;
	in.touch_click = gp_in.touchpad_click != 0;
	in.battery = gp_in.battery;
	sony_reports::ds5_fill_synth(rep.data(), in);
}

/* DualSense feature reports hosts read at startup (all zeros before). Report ID at [0]. */
constexpr uint8_t kFeatureCalibration = 0x05;
constexpr uint8_t kFeaturePairingInfo = 0x09;
constexpr uint8_t kFeatureFirmwareInfo = 0x20;

void fill_feature(uint8_t report_id, uint8_t* report)
{
	switch (report_id) {
		case kFeatureCalibration:
			sony_imu::fill_calibration(report);
			break;
		case kFeaturePairingInfo:
			board_mac::get_lsb_first(&report[1]);
			break;
		case kFeatureFirmwareInfo:
			/* Build date/time, hardware version at [24], firmware version at [28]. Update
			 * version at [44] stays 0: hosts keep to the original rumble format. */
			std::memcpy(&report[1], "Jun 19 2020", 11);
			std::memcpy(&report[12], "04:45:31", 8);
			report[24] = 0x17; report[25] = 0x06;
			report[28] = 0x1E; report[29] = 0x00; report[30] = 0x00; report[31] = 0x01;
			break;
		default:
			break;
	}
}

} // namespace

void SteamDevice::initialize()
{
	class_driver_ = {
		.name = TUD_DRV_NAME("STEAM"),
		.init = hidd_init,
		.deinit = hidd_deinit,
		.reset = hidd_reset,
		.open = hidd_open,
		.control_xfer_cb = hidd_control_xfer_cb,
		.xfer_cb = hidd_xfer_cb,
		.sof = NULL
	};

	std::memcpy(device_descriptor_, PS5Usb::DEVICE_DESCRIPTORS, sizeof(device_descriptor_));
	init_neutral_report(report_in_);
	report_out_.report_id = PS5::OutReportID::RUMBLE;
}

void SteamDevice::process(const uint8_t idx, Gamepad& gamepad)
{
	(void)idx;

	const Gamepad::PadIn gp_in = gamepad.get_pad_in();

	if (!SteamPassthrough::input_has_touchpad) {
		PS5::InReport rep{};
		SteamBtReport::fill_report_from_pad(rep, gp_in, gamepad);
		std::memcpy(report_in_.data(), &rep, sizeof(rep));
		if (report_in_.size() > sizeof(rep)) {
			std::memset(report_in_.data() + sizeof(rep), 0, report_in_.size() - sizeof(rep));
		}
		/* Core1 passthrough uses raw host axes (no profile axis-restrict) for sticks/triggers. */
		if (SteamPassthrough::has_report) {
			auto* out = reinterpret_cast<PS5::InReport*>(report_in_.data());
			const auto* raw = reinterpret_cast<const PS5::InReport*>(SteamPassthrough::report);
			out->joystick_rx = raw->joystick_rx;
			out->joystick_ry = raw->joystick_ry;
			out->trigger_l = raw->trigger_l;
			out->trigger_r = raw->trigger_r;
		}
		add_synth_fields(report_in_, gp_in, seq_++);
	} else if (SteamPassthrough::has_report) {
		std::memcpy(report_in_.data(), SteamPassthrough::report, SteamPassthrough::USB_REPORT_SIZE);
	} else {
		init_neutral_report(report_in_);
	}

	/* Mouse only from a pad's touchpad (DualSense passthrough, or DS4 in the synthesized
	 * report) — no right-stick mouse fallback. */
	if (SteamPassthrough::input_has_touchpad || gp_in.touchpad_valid) {
		SteamTouchpad::send_mouse_from_report(report_in_.data());
	}

	if (tud_hid_n_ready(Steam::ITF_GAMEPAD)) {
		tud_hid_n_report(Steam::ITF_GAMEPAD, 0, report_in_.data(), static_cast<uint16_t>(report_in_.size()));
	}

	if (new_report_out_) {
		/* Rumble flags as hosts send them (SDL HIDAPI PS5, Linux hid-playstation, inputtino):
		 * motors are valid with compatible vibration in valid_flag0 (0x01) or its "improved
		 * rumble" form in valid_flag2 (0x04); every valid flag clear is SDL's stop. Other
		 * reports (e.g. lightbar-only) leave the running rumble alone. */
		const uint8_t flag0 = report_out_.control_flag[0];
		const uint8_t flag1 = report_out_.control_flag[1];
		const uint8_t flag2 = report_out_.led_control_flag;   // valid_flag2
		if ((flag0 & 0x01) || (flag2 & 0x04)) {
			Gamepad::PadOut gp_out;
			gp_out.rumble_l = report_out_.motor_left;
			gp_out.rumble_r = report_out_.motor_right;
			gamepad.set_pad_out(gp_out);
		} else if (flag0 == 0 && flag1 == 0 && flag2 == 0) {
			gamepad.set_pad_out(Gamepad::PadOut());
		}
		/* Lightbar colour (valid_flag1 bit 2), sent on to a DS4 / DualSense. */
		if (flag1 & 0x04) {
			gamepad.set_host_lightbar(report_out_.lightbar_red, report_out_.lightbar_green,
			                          report_out_.lightbar_blue);
		}
		new_report_out_ = false;
	}
}

uint16_t SteamDevice::get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
	if (report_type == HID_REPORT_TYPE_INPUT) {
		if (itf == Steam::ITF_MOUSE) {
			const uint16_t n = static_cast<uint16_t>(std::min<uint16_t>(reqlen, 4));
			std::memset(buffer, 0, n);
			const uint16_t copy = static_cast<uint16_t>(std::min<size_t>(n, sizeof(SteamTouchpad::last_mouse_report)));
			std::memcpy(buffer, SteamTouchpad::last_mouse_report, copy);
			return n;
		}
		if (itf == Steam::ITF_GAMEPAD && (report_id == 0 || report_id == kReportIdIn)) {
			/* With a report ID, TinyUSB already puts it in front of the buffer: skip ours. */
			const size_t skip = (report_id == 0) ? 0 : 1;
			const uint16_t n = static_cast<uint16_t>(std::min<size_t>(reqlen, report_in_.size() - skip));
			std::memcpy(buffer, report_in_.data() + skip, n);
			return n;
		}
	} else if (report_type == HID_REPORT_TYPE_FEATURE) {
		/* Build the full report (ID at [0]) and hand TinyUSB everything after the ID. */
		std::array<uint8_t, 64> report{};
		report[0] = report_id;
		fill_feature(report_id, report.data());
		return static_cast<uint16_t>(sony_reports::copy_for_get_report(
			report_id, report.data(), report.size(), buffer, reqlen));
	}
	return 0;
}

void SteamDevice::set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
	if (itf != Steam::ITF_GAMEPAD || report_type != HID_REPORT_TYPE_OUTPUT) {
		return;
	}

	uint8_t rid = report_id;
	const uint8_t *buf = buffer;
	uint16_t len = bufsize;
	if (rid == 0 && len > 0) {
		rid = buffer[0];
		len = static_cast<uint16_t>(len - 1u);
		buf = &buffer[1];
	}

	/* buf holds the report after its ID (47 bytes for report 0x02), while PS5::OutReport starts
	 * with the ID field: the old size check dropped every report. Copy the body after the ID
	 * field; it must reach at least the lightbar colour. */
	constexpr size_t kMinBody = offsetof(PS5::OutReport, lightbar_blue);
	if ((rid == PS5::OutReportID::RUMBLE || rid == PS5::OutReportID::CONTROL) && len >= kMinBody) {
		report_out_ = PS5::OutReport();
		uint8_t* out = reinterpret_cast<uint8_t*>(&report_out_);
		out[0] = rid;
		std::memcpy(out + 1, buf, std::min<size_t>(len, sizeof(PS5::OutReport) - 1));
		new_report_out_ = true;
	}
}

bool SteamDevice::vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
	(void)rhport;
	(void)stage;
	(void)request;
	return false;
}

const uint16_t* SteamDevice::get_descriptor_string_cb(uint8_t index, uint16_t langid)
{
	const char *value = reinterpret_cast<const char*>(PS5Usb::STRING_DESCRIPTORS[index]);
	return get_string_descriptor(value, index);
}

const uint8_t* SteamDevice::get_descriptor_device_cb()
{
	device_descriptor_[8] = static_cast<uint8_t>(SteamPassthrough::host_vid & 0xFF);
	device_descriptor_[9] = static_cast<uint8_t>((SteamPassthrough::host_vid >> 8) & 0xFF);
	device_descriptor_[10] = static_cast<uint8_t>(SteamPassthrough::host_pid & 0xFF);
	device_descriptor_[11] = static_cast<uint8_t>((SteamPassthrough::host_pid >> 8) & 0xFF);
	return device_descriptor_;
}

const uint8_t* SteamDevice::get_hid_descriptor_report_cb(uint8_t itf)
{
	if (itf == Steam::ITF_MOUSE) {
		return Steam::MOUSE_REPORT_DESCRIPTORS;
	}
	return PS5Usb::REPORT_DESCRIPTORS;
}

const uint8_t* SteamDevice::get_descriptor_configuration_cb(uint8_t index)
{
	(void)index;
	return Steam::CONFIGURATION_DESCRIPTORS;
}

const uint8_t* SteamDevice::get_descriptor_device_qualifier_cb()
{
	return nullptr;
}
