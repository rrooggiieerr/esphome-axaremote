#include "esphome/core/log.h"
#include "cover.h"

#include <cinttypes>

#ifdef USE_ESP32
#include <esp_system.h>
#endif

namespace esphome {
namespace axaremote {

static const char *const TAG = "axaremote.cover";
static const char *const RETRY_TIMEOUT = "retry";

// Whether the ESP started because power was (re)applied. The AXA Remote shares the power supply, so it
// lost power as well.
static bool is_power_on_reset() {
#ifdef USE_ESP32
  const esp_reset_reason_t reason = esp_reset_reason();
  return reason == ESP_RST_POWERON || reason == ESP_RST_BROWNOUT;
#else
  return false;
#endif
}

void AXARemoteCover::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AXA Remote cover...");

  this->send_cmd_(AXACommand::DEVICE, this->device, 2);
  if (!this->device.empty())
    ESP_LOGCONFIG(TAG, "  Device: %s", this->device.c_str());
  this->send_cmd_(AXACommand::VERSION, this->version, 2);
  if (!this->version.empty())
    ESP_LOGCONFIG(TAG, "  Firmware: %s", this->version.c_str());

  if (this->device.empty() && this->version.empty()) {
    ESP_LOGCONFIG(TAG, "No AXA Remote connected");
    if (this->detect_) {
      this->connected_ = false;
      this->set_internal(true);
      return;
    }
  }

  this->connected_ = true;

  // Restore the settings from flash.
  this->settings_pref_ = this->make_entity_preference<AXARemoteSettings>(SETTINGS_PREF_VERSION);
  AXARemoteSettings settings{};
  if (this->settings_pref_.load(&settings)) {
    this->settings_ = settings;

    // Only use the calibrated close duration when auto calibration is enabled.
    if (this->auto_calibrate_ && this->settings_.close_duration != 0 &&
        this->set_close_duration(this->settings_.close_duration))
      ESP_LOGI(TAG, "Restored close duration: %.1fs", this->settings_.close_duration / 1e3f);
  }

  // Write the position to flash as soon as a movement ends.
  this->add_on_state_callback([this]() {
    if (this->current_operation == cover::COVER_OPERATION_IDLE &&
        this->published_operation_ != cover::COVER_OPERATION_IDLE)
      this->defer([]() { global_preferences->sync(); });
    this->published_operation_ = this->current_operation;
  });

  // After a power outage the AXA Remote reports the lock state as unlocked, also when the window is closed and
  // locked, until the window has been opened and closed again.
  if (is_power_on_reset()) {
    ESP_LOGW(TAG, "Power outage detected");
    this->power_outage_detected_ = true;
  }

  auto restore = this->restore_state_();
  if (restore.has_value()) {
    ESP_LOGI(TAG, "Restoring state");
    restore->apply(this);
    ESP_LOGV(TAG, "Current position: %.1f%%", this->position * 100);

    this->last_position_ = this->position;

    AXAResponseCode response_code = this->send_cmd_(AXACommand::STATUS, 2);

    if (this->position == cover::COVER_OPEN) {
      // After a power outage the AXA Remote will only close if first the open command is given. If the window is fully
      // open we can safely give an other open command without influencing the actual window position.
      esphome::delay(10);
      this->send_cmd_(AXACommand::OPEN, 2);
    }

#ifndef USE_ESP32
    // Without a reset reason, a closed window that reports unlocked is the best indication of a power outage.
    if (this->position == cover::COVER_CLOSED && response_code == AXAResponseCode::Unlocked) {
      ESP_LOGW(TAG, "Power outage detected");
      this->power_outage_detected_ = true;
    }
#endif

    if (this->position > 0.0f) {
      this->lock_position_ = LOCK_OPEN;
      this->lock_cleared_ = true;
    }
  } else {
    ESP_LOGI(TAG, "Nothing to restore");
    AXAResponseCode response_code = this->send_cmd_(AXACommand::STATUS, 2);

    if (response_code == AXAResponseCode::StrongLocked || response_code == AXAResponseCode::WeakLocked) {
      this->position = cover::COVER_CLOSED;
      this->last_position_ = cover::COVER_CLOSED;
      this->lock_position_ = LOCK_CLOSED;
      this->lock_cleared_ = false;
    } else if (response_code == AXAResponseCode::Unlocked) {
      this->position = 0.5f;
      this->last_position_ = 0.5f;
      this->lock_position_ = LOCK_OPEN;
      this->lock_cleared_ = true;
    }
    this->current_operation = cover::COVER_OPERATION_IDLE;
  }
}

void AXARemoteCover::dump_config() {
  if (!this->connected_) {
    ESP_LOGCONFIG(TAG, "No AXA Remote connected");
    return;
  }

  LOG_COVER("", "AXA Remote cover", this);
  ESP_LOGCONFIG(TAG, "  Device: %s", this->device.c_str());
  ESP_LOGCONFIG(TAG, "  Firmware: %s", this->version.c_str());
  ESP_LOGCONFIG(TAG, "  Full Close Duration: %.1fs", this->full_close_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Unlock Duration: %.1fs", this->unlock_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Open Duration: %.1fs", this->open_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Close Duration: %.1fs", this->close_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Lock Duration: %.1fs", this->lock_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Auto calibrate: %s", this->auto_calibrate_ ? "True" : "False");
  ESP_LOGCONFIG(TAG, "  Operation: %d", this->current_operation);
  ESP_LOGCONFIG(TAG, "  Window position: %.1f%%", this->position * 100);
  ESP_LOGCONFIG(TAG, "  Lock position: %.1f%%", this->lock_position_ * 100);
  if (this->power_outage_detected_)
    ESP_LOGW(TAG, "Power outage detected");
}

cover::CoverTraits AXARemoteCover::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_supports_stop(true);
  traits.set_supports_position(true);
  traits.set_supports_toggle(true);
  return traits;
}

void AXARemoteCover::save_settings_() {
  if (this->settings_pref_.save(&this->settings_)) {
    ESP_LOGD(TAG, "Stored settings");
  } else {
    ESP_LOGW(TAG, "Failed to store settings");
  }
}

bool AXARemoteCover::set_close_duration(uint32_t close_duration) {
  if (close_duration < MIN_CLOSE_DURATION || close_duration > MAX_CLOSE_DURATION) {
    ESP_LOGW(TAG, "Close duration %.1fs out of range, ignoring", close_duration / 1e3f);
    return false;
  }

  this->full_close_duration_ = close_duration;
  this->unlock_duration_ = close_duration * 0.55f;
  this->open_duration_ = close_duration * 0.7f;
  this->close_duration_ = close_duration * 0.7f;
  this->lock_duration_ = close_duration * 0.3f;

  ESP_LOGD(TAG, "  Unlock Duration: %.1fs", this->unlock_duration_ / 1e3f);
  ESP_LOGD(TAG, "  Open Duration: %.1fs", this->open_duration_ / 1e3f);
  ESP_LOGD(TAG, "  Close Duration: %.1fs", this->close_duration_ / 1e3f);
  ESP_LOGD(TAG, "  Lock Duration: %.1fs", this->lock_duration_ / 1e3f);

  return true;
}

void AXARemoteCover::loop() {
  if (!this->connected_)
    return;

  const uint32_t now = millis();

  if (now - this->last_cmd_time_ >= this->polling_interval_) {
    AXAResponseCode response_code = this->send_cmd_(AXACommand::STATUS);

    if (this->power_outage_detected_ &&
        (response_code == AXAResponseCode::StrongLocked || response_code == AXAResponseCode::WeakLocked)) {
      ESP_LOGI(TAG, "Lock state reported correctly again after power outage");
      this->power_outage_detected_ = false;
    }

    if ((response_code == AXAResponseCode::StrongLocked || response_code == AXAResponseCode::WeakLocked) &&
        this->current_operation == cover::COVER_OPERATION_IDLE && this->position != cover::COVER_CLOSED) {
      // This happens when the window opener is closed with the remote.
      ESP_LOGD(TAG, "Closed with remote?");

      this->position = cover::COVER_CLOSED;
      this->last_position_ = cover::COVER_CLOSED;
      this->lock_position_ = LOCK_CLOSED;
      this->lock_cleared_ = false;
      this->current_operation = cover::COVER_OPERATION_IDLE;
      this->publish_state();
    } else if ((response_code == AXAResponseCode::StrongLocked || response_code == AXAResponseCode::WeakLocked) &&
               this->current_operation == cover::COVER_OPERATION_OPENING && !this->lock_cleared_) {
      // When the window is being opened by the ESP from the closed position the first couple of
      // status responses will still be StrongLocked/WeakLocked. Ignore and continue.
    } else if ((response_code == AXAResponseCode::StrongLocked || response_code == AXAResponseCode::WeakLocked) &&
               this->current_operation == cover::COVER_OPERATION_OPENING && this->lock_cleared_) {
      // This happens when the window is being opened, but then closed with the remote.
      ESP_LOGD(TAG, "Closed with remote while opening?");

      this->position = cover::COVER_CLOSED;
      this->last_position_ = cover::COVER_CLOSED;
      this->lock_position_ = LOCK_CLOSED;
      this->lock_cleared_ = false;
      this->current_operation = cover::COVER_OPERATION_IDLE;
      this->publish_state();
    } else if ((response_code == AXAResponseCode::StrongLocked || response_code == AXAResponseCode::WeakLocked) &&
               this->current_operation == cover::COVER_OPERATION_CLOSING) {
      if (this->last_position_ == cover::COVER_OPEN) {
        // Only calculate the full close duration when the last position has been fully open.
        uint32_t close_duration = now - this->start_close_time_;
        ESP_LOGI(TAG, "Full Close Duration: %.1fs", close_duration / 1e3f);
        if (this->auto_calibrate_ && this->set_close_duration(close_duration)) {
          this->settings_.close_duration = close_duration;
          this->save_settings_();
        }
      }

      this->position = cover::COVER_CLOSED;
      this->last_position_ = cover::COVER_CLOSED;
      this->lock_position_ = LOCK_CLOSED;
      this->lock_cleared_ = false;
      this->current_operation = cover::COVER_OPERATION_IDLE;
      this->publish_state();
    } else if (response_code == AXAResponseCode::Unlocked && this->position == cover::COVER_CLOSED &&
               this->current_operation == cover::COVER_OPERATION_IDLE && !this->power_outage_detected_) {
      // This happens when the window opener is opened with the remote.
      ESP_LOGD(TAG, "Opened with remote?");
      this->position = cover::COVER_CLOSED;
      this->last_position_ = cover::COVER_CLOSED;
      this->current_operation = cover::COVER_OPERATION_OPENING;
      this->lock_position_ = 0.2f;
      this->last_recompute_time_ = now;
      this->publish_state();
    } else if (response_code == AXAResponseCode::Unlocked) {
      if (!this->lock_cleared_) {
        ESP_LOGD(TAG, "Lock cleared on position %.1f%%", this->lock_position_ * 100);
      }
      this->lock_cleared_ = true;
    } else {
      ESP_LOGV(TAG, "Response code: %d", response_code);
    }
  }

  if (this->current_operation != cover::COVER_OPERATION_IDLE) {
    // Recompute position every loop cycle during movement.
    this->recompute_position_();

    if (this->is_at_target_() && this->last_position_ != this->position) {
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
    } else if (now - this->last_publish_time_ > this->open_duration_ / 100 && this->position > cover::COVER_CLOSED &&
               this->position < cover::COVER_OPEN) {
      // Send current position every 1%.
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
  // A new direction replaces a pending retry.
  this->cancel_timeout(RETRY_TIMEOUT);

  if (dir == this->current_operation && dir != cover::COVER_OPERATION_IDLE)
    return;

  this->recompute_position_();

  const uint32_t now = millis();
  this->last_recompute_time_ = now;

  std::string *cmd;
  switch (dir) {
    case cover::COVER_OPERATION_IDLE:
      cmd = &AXACommand::STOP;
      break;
    case cover::COVER_OPERATION_OPENING:
      cmd = &AXACommand::OPEN;
      break;
    case cover::COVER_OPERATION_CLOSING:
      cmd = &AXACommand::CLOSE;
      break;
    default:
      ESP_LOGE(TAG, "Invalid direction: %d", dir);
      return;
  }

  AXAResponseCode response_code;
  response_code = this->send_cmd_(*cmd);

  if (response_code == AXAResponseCode::OK) {
    this->current_operation = dir;

    switch (dir) {
      case cover::COVER_OPERATION_IDLE:
        this->last_position_ = this->position;
        break;
      case cover::COVER_OPERATION_OPENING:
        this->last_operation_ = dir;
        if (this->position > cover::COVER_CLOSED)
          this->last_position_ = this->position;
        break;
      case cover::COVER_OPERATION_CLOSING:
        this->last_operation_ = dir;
        if (this->position < cover::COVER_OPEN)
          this->last_position_ = this->position;
        this->start_close_time_ = now;
        break;
    }

    // Publish the started movement, so Home Assistant shows it straight away.
    if (dir != cover::COVER_OPERATION_IDLE)
      this->publish_state();
  } else {
    // Retry until the AXA Remote accepts the command.
    ESP_LOGD(TAG, "Retrying operation in %" PRIu32 " ms", CMD_RETRY_INTERVAL);
    this->set_timeout(RETRY_TIMEOUT, CMD_RETRY_INTERVAL, [this, dir]() { this->start_direction_(dir); });
  }
}

void AXARemoteCover::recompute_position_() {
  if (this->current_operation == cover::COVER_OPERATION_IDLE)
    return;

  const uint32_t now = millis();
  switch (this->current_operation) {
    case cover::COVER_OPERATION_OPENING:
      this->lock_position_ += (now - this->last_recompute_time_) / this->lock_duration_;
      if (this->lock_cleared_)
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

  this->position = clamp(this->position, cover::COVER_CLOSED, cover::COVER_OPEN);
  this->lock_position_ = clamp(this->lock_position_, LOCK_CLOSED, LOCK_OPEN);

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
  // Flush UART before sending command.
  this->flush();

  // Read and clear the serial buffer
  std::string garbage;
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
    if (c != '\r') {
      garbage += c;
    }
  }
  if (garbage.length() > 0)
    ESP_LOGD(TAG, "Garbage received: %s", garbage.c_str());

  // Send the command.
  if (cmd == AXACommand::STATUS) {
    ESP_LOGV(TAG, "Command: %s", cmd.c_str());
  } else {
    ESP_LOGD(TAG, "Command: %s", cmd.c_str());
  }
  this->write_str(cmd.c_str());
  this->write_str("\r\n");

  // Flush UART.
  this->flush();

  const uint32_t now = millis();
  this->last_cmd_time_ = now;

  // Read the response.
  bool echo_received = false;
  int response_code_ = 0;
  std::string response_;
  while (true) {
    if (this->available() > 0) {
      uint8_t c;
      this->read_byte(&c);
      if (response_.length() == 0 && c >= '0' && c <= '9')
        response_code_ = (response_code_ * 10) + (c - '0');
      else if (c == ' ' && response_.length() == 0) {
        // Do nothing.
      } else if (c != '\r' && c != '\n')
        response_ += c;
      if (c == '\n') {
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
    if (millis() - now > 100) {
      // While detecting an optional AXA Remote a missing response is expected.
      if (this->detect_ && !this->connected_) {
        ESP_LOGD(TAG, "Timeout while waiting for response");
      } else {
        ESP_LOGE(TAG, "Timeout while waiting for response");
      }
      break;
    }
  }

  return AXAResponseCode::Invalid;
}

AXAResponseCode AXARemoteCover::send_cmd_(std::string &cmd, std::string &response, int max_tries) {
  AXAResponseCode response_code;
  int tries = 0;

  while (true) {
    if (tries > 0) {
      if (this->detect_ && !this->connected_) {
        ESP_LOGD(TAG, "Retrying to send command %s", cmd.c_str());
      } else {
        ESP_LOGI(TAG, "Retrying to send command %s", cmd.c_str());
      }
    }

    response_code = this->send_cmd_(cmd, response);
    if (response_code != AXAResponseCode::Invalid && response_code != AXAResponseCode::Error)
      break;
    response.erase();

    tries++;
    if (tries == max_tries) {
      if (this->detect_ && !this->connected_) {
        ESP_LOGD(TAG, "Failed to send command %s", cmd.c_str());
      } else {
        ESP_LOGE(TAG, "Failed to send command %s", cmd.c_str());
      }
      break;
    }
    esphome::delay(CMD_RETRY_INTERVAL);
  }

  return response_code;
}

AXAResponseCode AXARemoteCover::send_cmd_(std::string &cmd) {
  std::string s;
  return this->send_cmd_(cmd, s);
}

AXAResponseCode AXARemoteCover::send_cmd_(std::string &cmd, int max_tries) {
  std::string s;
  return this->send_cmd_(cmd, s, max_tries);
}

}  // namespace axaremote
}  // namespace esphome
