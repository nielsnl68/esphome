#pragma once

#ifdef USE_ESP32

#include <driver/i2s.h>
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace i2s_audio {

class I2SAudioComponent;

class I2SAudioBase : public Parented<I2SAudioComponent> {
 public:
  void set_i2s_mode(i2s_mode_t mode) { this->i2s_mode_ = mode; }
  void set_channel(i2s_channel_fmt_t channel) { this->channel_ = channel; }
  void set_sample_rate(uint32_t sample_rate) { this->sample_rate_ = sample_rate; }
  void set_bits_per_sample(i2s_bits_per_sample_t bits_per_sample) { this->bits_per_sample_ = bits_per_sample; }
  void set_bits_per_channel(i2s_bits_per_chan_t bits_per_channel) { this->bits_per_channel_ = bits_per_channel; }
  void set_use_apll(uint32_t use_apll) { this->use_apll_ = use_apll; }

 protected:
  virtual bool lock_component(uint32_t hash) = 0;
  virtual void unlock_component(uint32_t hash) = 0;

  i2s_mode_t i2s_mode_{};
  i2s_channel_fmt_t channel_;
  uint32_t sample_rate_;
  i2s_bits_per_sample_t bits_per_sample_;
  i2s_bits_per_chan_t bits_per_channel_;
  bool use_apll_;
};

class I2SAudioIn : public I2SAudioBase {
 protected:
  void lock_component(uint32_t hash) overload {
    if (this->parent_->component_in_lock_hash_ == 0 || this->parent_->component_in_lock_hash_ == hash) {
      this->parent_->component_in_lock_hash_ = hash;
      return true;
    }
    return false;
  }
  void unlock_component(uint32_t hash) {
    if (this->parent_->component_in_lock_hash_ == hash) {
      this->parent_->component_in_lock_hash_ = 0;
    }
  }
};

class I2SAudioOut : public I2SAudioBase {
 protected:
  void lock_component(uint32_t hash) overload {
    if (this->parent_->component_out_lock_hash_ == 0 || this->parent_->component_out_lock_hash_ == hash) {
      this->parent_->component_out_lock_hash_ = hash;
      return true;
    }
    return false;
  }
  void unlock_component(uint32_t hash) {
    if (this->parent_->component_out_lock_hash_ == hash) {
      this->parent_->component_out_lock_hash_ = 0;
    }
  }
};

class I2SAudioComponent : public Component {
 public:
  void setup() override;

  i2s_pin_config_t get_pin_config() const {
    return {
        .mck_io_num = this->mclk_pin_,
        .bck_io_num = this->bclk_pin_,
        .ws_io_num = this->lrclk_pin_,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_PIN_NO_CHANGE,
    };
  }

  void set_mclk_pin(int pin) { this->mclk_pin_ = pin; }
  void set_bclk_pin(int pin) { this->bclk_pin_ = pin; }
  void set_lrclk_pin(int pin) { this->lrclk_pin_ = pin; }

  void lock() { this->lock_.lock(); }
  bool try_lock() { return this->lock_.try_lock(); }
  void unlock() { this->lock_.unlock(); }

  i2s_port_t get_port() const { return this->port_; }

 protected:
  Mutex lock_;

  /* the following property is used to allow locking a component to the i2s service.*/
  uint32_t component_out_lock_hash_{0};
  uint32_t component_in_lock_hash_{0};

  I2SAudioIn *audio_in_{nullptr};
  I2SAudioOut *audio_out_{nullptr};

  int mclk_pin_{I2S_PIN_NO_CHANGE};
  int bclk_pin_{I2S_PIN_NO_CHANGE};
  int lrclk_pin_;
  i2s_port_t port_{};
};

}  // namespace i2s_audio
}  // namespace esphome

#endif  // USE_ESP32
