#pragma once

#include "esphome/core/defines.h"
#ifdef USE_DISPLAY
#include "esphome/components/display/display_buffer.h"
#endif

#include "esphome/core/automation.h"
#include "esphome/core/hal.h"

#include <vector>
#include <map>

namespace esphome {
namespace touchscreen {

struct TouchPoint {
  uint8_t id;
  int16_t x_raw{0}, y_raw{0}, z_raw{0};
  uint16_t x;
  uint16_t y;
  uint8_t state;
};

using TouchPoints_t = std::vector<TouchPoint>;

enum TouchMode {
  SINGLE_TOUCH,
  MULTI_TOUCH,
  MOVING_TOUCH,
};

struct TouchscreenInterupt {
  volatile bool touched{true};
  bool init{false};
  static void gpio_intr(TouchscreenInterupt *store);
};

class TouchListener {
 public:
  virtual void touch(TouchPoint tp) {}
  virtual void touch(const TouchPoints_t &tpoints) {}
  /* return true to update first touch position */
  virtual void update(const TouchPoints_t &tpoints) {}
  virtual void release() {}
};
// std::vector<Style> styles_{};

enum TouchRotation {
  ROTATE_0_DEGREES = 0,
  ROTATE_90_DEGREES = 90,
  ROTATE_180_DEGREES = 180,
  ROTATE_270_DEGREES = 270,
};

class Touchscreen : public PollingComponent {
 public:
#ifdef USE_DISPLAY
  void set_display(display::Display *display) { this->display_ = display; }
  display::Display *get_display() const { return this->display_; }
#endif
  void set_display_dimension(uint16_t width, u_int16_t height) {
    this->display_width_ = width;
    this->display_height_ = height;
  }
  void set_rotation(TouchRotation rotation) { this->rotation_ = rotation; }

  void set_swap_x_y(bool swap) { this->swap_x_y_ = swap; }
  void set_calibration(int16_t x_min, int16_t x_max, int16_t y_min, int16_t y_max) {
    this->x_raw_min_ = std::min(x_min, x_max);
    this->x_raw_max_ = std::max(x_min, x_max);
    this->y_raw_min_ = std::min(y_min, y_max);
    this->y_raw_max_ = std::max(y_min, y_max);
    this->invert_x_ = (x_min > x_max);
    this->invert_y_ = (y_min > y_max);
  }

  Trigger<TouchPoint, const TouchPoints_t &> *get_touch_trigger() { return &this->touch_trigger_; }
  Trigger<const TouchPoints_t &> *get_update_trigger() { return &this->update_trigger_; }
  Trigger<> *get_release_trigger() { return &this->release_trigger_; }

  void register_listener(TouchListener *listener) { this->touch_listeners_.push_back(listener); }

  void update() override;
  void loop() override;

 protected:
  /// Call this function to send touch points to the `on_touch` listener and the binary_sensors.
  void attach_interrupt_(InternalGPIOPin *irq_pin, esphome::gpio::InterruptType type);

  virtual void handle_touch(TouchPoints_t &touches) = 0;
  void send_touch_(TouchPoints_t touches);

  static int16_t normalize(int16_t val, int16_t min_val, int16_t max_val, bool inverted = false);

  uint16_t get_width_() {
#ifdef USE_DISPLAY
    if (this->display_ != nullptr) {
      return this->display_->get_width();
    }
#endif
    return display_width_;
  }

  uint16_t get_height_() {
#ifdef USE_DISPLAY
    if (this->display_ != nullptr) {
      return display_->get_height();
    }
#endif
    return display_height_;
  }

  TouchRotation get_rotation_() {
#ifdef USE_DISPLAY
    if (this->display_ != nullptr) {
      return static_cast<TouchRotation>(this->display_->get_rotation());
    }
#endif
    return this->rotation_;
  }
#ifdef USE_DISPLAY
  display::Display *display_{nullptr};
#endif
  uint16_t display_width_{240};
  uint16_t display_height_{320};
  TouchRotation rotation_{ROTATE_0_DEGREES};

  int16_t x_raw_min_{0}, x_raw_max_{0}, y_raw_min_{0}, y_raw_max_{0};
  bool invert_x_{false}, invert_y_{false}, swap_x_y_{false};

  Trigger<TouchPoint, const TouchPoints_t &> touch_trigger_;
  Trigger<const TouchPoints_t &> update_trigger_;
  Trigger<> release_trigger_;
  std::vector<TouchListener *> touch_listeners_;

  std::map<uint8_t, TouchPoint> first_touch_;
  TouchscreenInterupt store_;
};

}  // namespace touchscreen
}  // namespace esphome
