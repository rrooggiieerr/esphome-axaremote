#include "esphome/core/log.h"
#include "cover.h"

namespace esphome {
namespace axaremote {

static const char *const TAG = "axaremote.cover";

void AXARemoteCover::setup() {
	ESP_LOGCONFIG(TAG, "Setting up AXA Remote cover...");

	// Clear the serial buffer
	this->write_str("\r\n");
	esphome::delay(20);
	while(this->available() > 0) {
		uint8_t c;
		this->read_byte(&c);
	}

	this->send_cmd_(AXACommand::DEVICE, this->device);
	ESP_LOGCONFIG(TAG, "  Device: %s", this->device.c_str());
	this->send_cmd_(AXACommand::VERSION, this->version);
	ESP_LOGCONFIG(TAG, "  Firmware: %s", this->version.c_str());

	auto restore = this->restore_state_();
	if (restore.has_value()) {
		ESP_LOGI(TAG, "Restoring state");
		restore->apply(this);
		ESP_LOGV(TAG, "Current position: %.1f%%", this->position * 100);
		ESP_LOGI(TAG, "Current operation: %d", this->current_operation);
		if (this->position == cover::COVER_CLOSED && this->current_operation == cover::COVER_OPERATION_IDLE && this->send_cmd_(AXACommand::STATUS) == AXAResponseCode::Unlocked) {
			// After a power outage the AXA Remote always returns "Unlocked", even when the window is supposed to be closed.
			// This can only be solved by opening and closing the window.
//			this->send_cmd_(AXACommand::OPEN);
//			esphome::delay(this->unlock_duration_ / 2);
			this->send_cmd_(AXACommand::CLOSE);
		}

		if(this->position > 0.0f) {
			this->lock_position_ = LOCK_OPEN;
			this->lock_cleared_ = true;
		}
	} else {
		ESP_LOGI(TAG, "Nothing to restore");
		AXAResponseCode response = this->send_cmd_(AXACommand::STATUS);

		if(response == AXAResponseCode::StrongLocked || response == AXAResponseCode::WeakLocked) {
			this->position = cover::COVER_CLOSED;
			this->last_position_ = cover::COVER_CLOSED;
			this->lock_position_ = LOCK_CLOSED;
			this->lock_cleared_ = false;
		} else if(response == AXAResponseCode::Unlocked) {
			this->position = 0.5f;
			this->last_position_ = 0.5f;
			this->lock_position_ = LOCK_OPEN;
			this->lock_cleared_ = true;
		}
		this->current_operation = cover::COVER_OPERATION_IDLE;
	}
}

void AXARemoteCover::dump_config() {
	LOG_COVER("", "AXA Remote cover", this);
	ESP_LOGCONFIG(TAG, "  Device: %s", this->device.c_str());
	ESP_LOGCONFIG(TAG, "  Firmware: %s", this->version.c_str());
	ESP_LOGCONFIG(TAG, "  Unlock Duration: %.1fs", this->unlock_duration_ / 1e3f);
	ESP_LOGCONFIG(TAG, "  Open Duration: %.1fs", this->open_duration_ / 1e3f);
	ESP_LOGCONFIG(TAG, "  Close Duration: %.1fs", this->close_duration_ / 1e3f);
	ESP_LOGCONFIG(TAG, "  Lock Duration: %.1fs", this->lock_duration_ / 1e3f);
	ESP_LOGCONFIG(TAG, "  Auto calibrate: %s", this->auto_calibrate_ ? "True" : "False");
	ESP_LOGCONFIG(TAG, "  Operation: %d", this->current_operation);
	ESP_LOGCONFIG(TAG, "  Window position: %.1f%%", this->position * 100);
	ESP_LOGCONFIG(TAG, "  Lock position: %.1f%%", this->lock_position_ * 100);
}

cover::CoverTraits AXARemoteCover::get_traits() {
	auto traits = cover::CoverTraits();
	traits.set_supports_stop(true);
	traits.set_supports_position(true);
	traits.set_supports_toggle(true);
	return traits;
}

void AXARemoteCover::set_close_duration(uint32_t close_duration) {
	this->unlock_duration_ = close_duration * 0.55f;
	this->open_duration_ = close_duration  * 0.7f;
	this->close_duration_ = close_duration * 0.7f;
	this->lock_duration_ = close_duration * 0.3f;

	ESP_LOGD(TAG, "  Unlock Duration: %.1fs", this->unlock_duration_ / 1e3f);
	ESP_LOGD(TAG, "  Open Duration: %.1fs", this->open_duration_ / 1e3f);
	ESP_LOGD(TAG, "  Close Duration: %.1fs", this->close_duration_ / 1e3f);
	ESP_LOGD(TAG, "  Lock Duration: %.1fs", this->lock_duration_ / 1e3f);
}

void AXARemoteCover::loop() {
	const uint32_t now = millis();

	AXAResponseCode response = this->send_cmd_(AXACommand::STATUS);

	if ((response == AXAResponseCode::StrongLocked || response == AXAResponseCode::WeakLocked) && this->current_operation == cover::COVER_OPERATION_IDLE && this->position != cover::COVER_CLOSED) {
		// This happens when the window opener is closed with the remote.
		ESP_LOGD(TAG, "Closed with remote?");

		this->position = cover::COVER_CLOSED;
		this->last_position_ = cover::COVER_CLOSED;
		this->lock_position_ = LOCK_CLOSED;
		this->lock_cleared_ = false;
		this->current_operation = cover::COVER_OPERATION_IDLE;
		this->publish_state();

		return;
	} else if ((response == AXAResponseCode::StrongLocked || response == AXAResponseCode::WeakLocked) && this->current_operation == cover::COVER_OPERATION_OPENING && !this->lock_cleared_) {
		// When the window is being opened by the ESP from the closed position the first couple of
		// status responses will still be StrongLocked/WeakLocked. Ignore and continue.
	} else if ((response == AXAResponseCode::StrongLocked || response == AXAResponseCode::WeakLocked) && this->current_operation == cover::COVER_OPERATION_OPENING && this->lock_cleared_) {
		// This happens when the window is being opened, but then closed with the remote.
		ESP_LOGD(TAG, "Closed with remote while opening?");

		this->position = cover::COVER_CLOSED;
		this->last_position_ = cover::COVER_CLOSED;
		this->lock_position_ = LOCK_CLOSED;
		this->lock_cleared_ = false;
		this->current_operation = cover::COVER_OPERATION_IDLE;
		this->publish_state();

		return;
	} else if ((response == AXAResponseCode::StrongLocked || response == AXAResponseCode::WeakLocked) && this->current_operation == cover::COVER_OPERATION_CLOSING) {
		if(this->last_position_ == cover::COVER_OPEN) {
			// Only calculate the full close duration when the last position has been fully open.
			uint32_t close_duration = now - this->start_close_time_;
			ESP_LOGI(TAG, "Full Close Duration: %.1fs", close_duration / 1e3f);
			if (this->auto_calibrate_)
				this->set_close_duration(close_duration);
		}

		this->position = cover::COVER_CLOSED;
		this->last_position_ = cover::COVER_CLOSED;
		this->lock_position_ = LOCK_CLOSED;
		this->lock_cleared_ = false;
		this->current_operation = cover::COVER_OPERATION_IDLE;
		this->publish_state();

		return;
	} else if (response == AXAResponseCode::Unlocked && this->position == cover::COVER_CLOSED && this->current_operation == cover::COVER_OPERATION_IDLE) {
		// This happens when the window opener is opened with the remote.
		ESP_LOGD(TAG, "Opened with remote?");
		this->position = cover::COVER_CLOSED;
		this->last_position_ = cover::COVER_CLOSED;
		this->current_operation = cover::COVER_OPERATION_OPENING;
		this->lock_position_ = 0.2f;
		this->last_recompute_time_ = now;
		this->publish_state();
	} else if (this->current_operation == cover::COVER_OPERATION_IDLE)
		return;
	else if (response == AXAResponseCode::Unlocked) {
		if(!this->lock_cleared_) {
			ESP_LOGD(TAG, "Lock cleared on lock position %.1f%%", this->lock_position_ * 100);
		}
		this->lock_cleared_ = true;
	} else {
		ESP_LOGV(TAG, "Response: %d", response);
	}

	// Recompute position every loop cycle.
	this->recompute_position_();

	if (this->is_at_target_()) {
		if (this->target_position_ == cover::COVER_CLOSED) {
			// Don't trigger stop and don't idle, let the cover stop and idle by response StrongLocked/WeakLocked.
		} else if (this->target_position_ == cover::COVER_OPEN) {
			// Don't trigger stop command, let the cover stop by itself.
			this->current_operation = cover::COVER_OPERATION_IDLE;
			this->last_position_ = cover::COVER_OPEN;
		} else {
			// Trigger stop command.
			this->start_direction_(cover::COVER_OPERATION_IDLE);
			this->last_position_ = this->position;
		}
		this->publish_state();
	}

	// Send current position every 1%.
	if (now - this->last_publish_time_ > this->unlock_duration_/100) {
		this->publish_state(false);
		this->last_publish_time_ = now;
	}

	// Log current position every second.
	if (now - this->last_log_time_ > 1000) {
		ESP_LOGV(TAG, "Current operation: %d", this->current_operation);
		ESP_LOGV(TAG, "Current position: %.1f%%", this->position * 100);
		ESP_LOGV(TAG, "Last position: %.1f%%", this->last_position_ * 100);
		ESP_LOGV(TAG, "Target position: %.1f%%", this->target_position_ * 100);
		ESP_LOGV(TAG, "Lock position: %.1f%%", this->lock_position_ * 100);

		this->last_log_time_ = now;
	}
}

void AXARemoteCover::control(const cover::CoverCall &call) {
	if (call.get_stop()) {
		this->start_direction_(cover::COVER_OPERATION_IDLE);
		this->publish_state();
	}
	if (call.get_toggle().has_value()) {
		if (this->current_operation != cover::COVER_OPERATION_IDLE) {
			this->start_direction_(cover::COVER_OPERATION_IDLE);
			this->publish_state();
		} else if (this->position == cover::COVER_CLOSED || this->last_operation_ == cover::COVER_OPERATION_CLOSING) {
			this->target_position_ = cover::COVER_OPEN;
			this->start_direction_(cover::COVER_OPERATION_OPENING);
		} else {
			this->target_position_ = cover::COVER_CLOSED;
			this->start_direction_(cover::COVER_OPERATION_CLOSING);
		}
	}
	if (call.get_position().has_value()) {
		auto pos = *call.get_position();
		if (pos == this->position) {
			// Already at target.
			if (pos == cover::COVER_OPEN || pos == cover::COVER_CLOSED) {
				// We should send the command again.
				auto op = pos == cover::COVER_CLOSED ? cover::COVER_OPERATION_CLOSING : cover::COVER_OPERATION_OPENING;
				this->target_position_ = pos;
				this->start_direction_(op);
			}
		} else {
			auto op = pos < this->position ? cover::COVER_OPERATION_CLOSING : cover::COVER_OPERATION_OPENING;
			this->target_position_ = pos;
			this->start_direction_(op);
		}
	}
}

void AXARemoteCover::start_direction_(cover::CoverOperation dir) {
	if (dir == this->current_operation && dir != cover::COVER_OPERATION_IDLE)
		return;

	this->recompute_position_();

	this->current_operation = dir;

	const uint32_t now = millis();
	this->last_recompute_time_ = now;

	switch (dir) {
	case cover::COVER_OPERATION_IDLE:
		this->send_cmd_(AXACommand::STOP);
		this->last_position_ = this->position;
		break;
	case cover::COVER_OPERATION_OPENING:
		this->last_operation_ = dir;
		if(this->position > cover::COVER_CLOSED)
			this->last_position_ = this->position;
		this->send_cmd_(AXACommand::OPEN);
		break;
	case cover::COVER_OPERATION_CLOSING:
		this->last_operation_ = dir;
		if(this->position < cover::COVER_OPEN)
			this->last_position_ = this->position;
		this->start_close_time_ = now;
		this->send_cmd_(AXACommand::CLOSE);
		break;
	default:
		return;
	}
}

void AXARemoteCover::recompute_position_() {
	if (this->current_operation == cover::COVER_OPERATION_IDLE)
		return;

	const uint32_t now = millis();
	switch (this->current_operation) {
	case cover::COVER_OPERATION_OPENING:
		this->lock_position_ += (now - this->last_recompute_time_) / this->lock_duration_;
		if(this->lock_cleared_)
			this->position += (now - this->last_recompute_time_) / this->open_duration_;
		break;
	case cover::COVER_OPERATION_CLOSING:
		this->position -= (now - this->last_recompute_time_) / this->close_duration_;
		if (this->position <= cover::COVER_CLOSED)
			this->lock_position_ -= (now - this->last_recompute_time_) / this->lock_duration_;
		break;
	default:
		return;
	}

	this->position = clamp(this->position, LOCK_CLOSED, LOCK_OPEN);
	this->lock_position_ = clamp(this->lock_position_, cover::COVER_CLOSED, cover::COVER_OPEN);

	this->last_recompute_time_ = now;
}

bool AXARemoteCover::is_at_target_() const {
	switch (this->current_operation) {
	case cover::COVER_OPERATION_OPENING:
		return this->position >= this->target_position_;
	case cover::COVER_OPERATION_CLOSING:
		return this->position <= this->target_position_;
	case cover::COVER_OPERATION_IDLE:
	default:
		return true;
	}
}

AXAResponseCode AXARemoteCover::send_cmd_(std::string &cmd, std::string &response) {
	int retries = 0;
	while (true) {
		// Flush UART before sending command.
		this->flush();

		// Clear the serial buffer
		while(this->available()) {
			uint8_t c;
			this->read_byte(&c);
		}

		// Send the command.
		if (cmd != AXACommand::STATUS) {
			ESP_LOGD(TAG, "Command: %s", cmd.c_str());
		}
		this->write_str(cmd.c_str());
		this->write_str("\r\n");

		// Flush UART.
		this->flush();

		// Read the response.
		bool echo_received = false;
		int response_code_ = 0;
		std::string response_;
		const uint32_t now = millis();
		while(true) {
			if(this->available() > 0) {
				uint8_t c;
				this->read_byte(&c);
				if (response_.length() == 0 && c >= '0' && c <= '9')
					response_code_ = (response_code_ * 10) + (c - '0');
				else if (c == ' ' && response_.length() == 0) {
					// Do nothing.
				} else if (c != '\r' && c != '\n')
					response_ += c;
				if (c == '\n' || !this->available()) {
					if (response_ == cmd) {
						// Command echo.
						if (cmd != AXACommand::STATUS)
							ESP_LOGD(TAG, "Command echo received: %s", response_.c_str());
						echo_received = true;
					} else if (response_.length() > 0) {
						if (!echo_received && cmd != AXACommand::STATUS)
							ESP_LOGW(TAG, "No command echo received");

						if (response_code_ >= int(AXAResponseCode::OK) && response_code_ <= int(AXAResponseCode::Error)) {
							// The actual response.
							if (cmd != AXACommand::STATUS)
								ESP_LOGD(TAG, "Response: %d %s", response_code_, response_.c_str());
							response += response_;
							return AXAResponseCode(response_code_);
						} else {
							// Garbage.
							ESP_LOGW(TAG, "Garbage received: %s", response_.c_str());
						}
					}
					response_.erase();
				}
			}
			if (millis() - now > 25) {
				ESP_LOGE(TAG, "Timeout while waiting for response");
				break;
			}
		}

		if (retries == 5) {
			ESP_LOGE(TAG, "Timeout while waiting for response");
			break;
		}
		retries++;
		esphome::delay(10);
	}

	return AXAResponseCode::Invalid;
}

AXAResponseCode AXARemoteCover::send_cmd_(std::string &cmd) {
	std::string s;
	return this->send_cmd_(cmd, s);
}

}  // namespace axaremote
}  // namespace esphome
