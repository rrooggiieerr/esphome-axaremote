#pragma once

#include "esphome/core/component.h"
//#include "esphome/core/helpers.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace axaremote {

// AXA Remote commands
class AXACommand{
public:
	static inline std::string OPEN = "OPEN";
	static inline std::string CLOSE = "CLOSE";
	static inline std::string STOP = "STOP";
	static inline std::string STATUS = "STATUS";
	static inline std::string DEVICE = "DEVICE";
	static inline std::string VERSION = "VERSION";
	static inline std::string HELP = "HELP";
};

// AXA Remote return codes
enum class AXAResponseCode {
	OK = 200,
	Unlocked = 210,
	StrongLocked = 211,
	WeakLocked = 212,
	Device = 260,
	Firmware = 261,
	Version = 261,
	Error = 502,
	Invalid = -1,
};

const float LOCK_OPEN = 1.0f;
const float LOCK_CLOSED = 0.0f;

class AXARemoteCover: public Component, public cover::Cover, public uart::UARTDevice {
public:
	float get_setup_priority() const override { return esphome::setup_priority::DATA; }
	void setup() override;
	void dump_config() override;
	cover::CoverTraits get_traits() override;
	void set_polling_interval(uint32_t polling_interval) { this->polling_interval_ = polling_interval; }
	void set_close_duration(uint32_t close_duration);
	void set_auto_calibrate(bool auto_calibrate) { this->auto_calibrate_ = auto_calibrate; }
	void loop();

protected:
	std::string device;
	std::string version;

	float unlock_duration_;
	float open_duration_;
	float close_duration_;
	float lock_duration_;
	uint32_t polling_interval_;
	bool auto_calibrate_ = false;
	bool power_outage_detected_ = false;

	uint32_t last_cmd_{0};
	uint32_t last_recompute_time_{0};
	uint32_t start_close_time_{0};
	uint32_t last_publish_time_{0};
	uint32_t last_log_time_{0};
	float target_position_{0};
	float last_position_{0};
	float lock_position_{0};
	bool lock_cleared_ = false;
	cover::CoverOperation last_operation_{cover::COVER_OPERATION_OPENING};
	cover::CoverOperation *retry_operation_;

	void control(const cover::CoverCall &call) override;
	bool is_at_target_() const;

	void start_direction_(cover::CoverOperation dir);

	void recompute_position_();

	AXAResponseCode send_cmd_(std::string &cmd, std::string &response);
	AXAResponseCode send_cmd_(std::string &cmd, std::string &response, int max_retries);
	AXAResponseCode send_cmd_(std::string &cmd);
	AXAResponseCode send_cmd_(std::string &cmd, int max_retries);
	AXAResponseCode execute_cmd_(std::string &cmd);
};

}  // namespace axaremote
}  // namespace esphome
