#pragma once
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display.h"
#include "esphome/components/display/display_color_utils.h"
#include "display_defines.h"
#include "display_iobusses.h"

namespace esphome {
namespace display {

const size_t TRANSFER_BUFFER_SIZE = 126;  // ensure this is divisible by 6

class DisplayDriver : public Display {
 public:
  DisplayDriver() = default;
  DisplayDriver(uint8_t const *init_sequence, int16_t width, int16_t height, bool invert_colors);

  void set_interface(IOBus *interface) { this->bus_ = interface; }
  void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }
  void set_palette(const uint8_t *palette) { this->palette_ = palette; }

  ColorBitness get_color_depth() { return this->display_bitness_; }
  void set_18bit_mode(bool mode) {
    if (mode) {
      this->display_bitness_.raw_16 = ColorBitness::COLOR_BITNESS_666;
    }
  }

  bool get_18bit_mode() { return this->display_bitness_.bits_per_pixel == ColorBitness::COLOR_BITS_666; }
  void set_color_order(ColorOrder color_order) { this->display_bitness_.color_order = color_order; }
  ColorOrder get_color_order() { return (ColorOrder) this->display_bitness_.color_order; }

  void set_swap_xy(bool swap_xy) { this->swap_xy_ = swap_xy; }
  void set_mirror_x(bool mirror_x) { this->mirror_x_ = mirror_x; }
  void set_mirror_y(bool mirror_y) { this->mirror_y_ = mirror_y; }
  void set_model(std::string model) { this->model_ = model; }

  void set_invert_colors(bool invert);

  void fill(Color color) override;

  void dump_config() override;
  void setup() override;

  void draw_pixel_at(int x, int y, Color color) override;
  void draw_pixels_at(const uint8_t *data, int x_display, int y_display, int x_in_data, int y_in_data, int width, int height,
                      ColorBitness bitness, int x_pad) override;

  bool is_buffered() { return (this->buffer_ != nullptr); }
  float get_setup_priority() const override { return setup_priority::HARDWARE; };

 protected:
  virtual void preset_init_values();
  virtual void finalize_init_values();

  virtual void buffer_pixel_at(int x, int y, Color color);
  virtual bool set_addr_window(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2);
  virtual bool send_buffer(const uint8_t *data, int x_display, int y_display, int x_in_data, int y_in_data,
                              int width, int height, ColorBitness bitness, int x_pad);

  virtual void setup_pins();
  virtual void setup_lcd();
  virtual bool setup_buffer();

  void display_buffer() override;

  void reset_();

  IOBus *bus_{nullptr};
  const uint8_t *init_sequence_{};
  Rect view_port_{};
  const uint8_t *palette_;
  std::string model_{""};

  ColorBitness buffer_bitness_{};
  ColorBitness display_bitness_{ColorBitness(ColorBitness::COLOR_BITNESS_565)};

  uint32_t get_buffer_size_();

  GPIOPin *reset_pin_{nullptr};

  bool pre_invertcolors_ = false;
  bool swap_xy_{};
  bool mirror_x_{};
  bool mirror_y_{};

  uint8_t *buffer_{nullptr};
  uint8_t *batch_buffer_{nullptr};
};

}  // namespace display
}  // namespace esphome
