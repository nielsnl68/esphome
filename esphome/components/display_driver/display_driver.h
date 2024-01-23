#pragma once
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display.h"
#include "esphome/components/display/display_color_utils.h"
#include "display_defines.h"

namespace esphome {
namespace display {

const size_t TRANSFER_BUFFER_SIZE = 126;  // ensure this is divisible by 6

#ifndef DISPLAY_DATA_RATE
#define Display_DATA_RATE spi::DATA_RATE_40MHZ
#endif  // Display_DATA_RATE

class displayInterface {
 public:
  virtual void setup(){};
  void set_always_start(bool always_enable) { this->always_start_ = always_enable; }
  inline void send_command(uint8_t command, bool enable = false) {
    this->send_command( command, nullptr, 0, enable);
  }
  void send_command(uint8_t command, const uint8_t *data, uint8_t len, bool enable = false);
  inline void send_data(const uint8_t data, bool enable = false) {
     send_data(&data, 1, enable);
  }
  void send_data(const uint8_t *data, uint8_t len = 1, bool enable = false);
  virtual void start(){};
  virtual void end(){};

 protected:
  virtual void start_command() = 0;
  virtual void end_command(){};
  virtual void start_data() = 0;
  virtual void end_data(){};

  virtual void command(uint8_t value);
  virtual void data(const uint8_t *value, uint16_t len);
  virtual void send_byte(uint8_t data)=0;
  virtual void send_array(const uint8_t *data, int16_t len)=0;

  bool always_start_{false};
};

class SPI_Interface : public displayInterface,
                      public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                            Display_DATA_RATE> {
 public:
  void setup();
  void set_dc_pin(GPIOPin *dc_pin) { dc_pin_ = dc_pin; }
  void start() override;
  void end() override;

 protected:
  void start_command() override;
  void end_command() override;
  void start_data() override;
  void send_byte(uint8_t data) override;
  void send_array(const uint8_t *data, int16_t len) override;

  GPIOPin *dc_pin_{nullptr};
  int enabled_{0};
};

class SPI16D_Interface : public SPI_Interface {
 protected:
  void data(const uint8_t *value, uint16_t len) override;
};

class DisplayDriver : public Display {
 public:
  DisplayDriver() = default;
  DisplayDriver(uint8_t const *init_sequence, int16_t width, int16_t height, bool invert_colors);

  void set_interface(displayInterface *interface) { this->bus_ = interface; }
  void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }
  void set_palette(const uint8_t *palette) { this->palette_ = palette; }

  ColorBitness get_color_depth() { return this->display_bitness_; }
  void set_18bit_mode(bool mode) {
    this->display_bitness_ = mode ? ColorBitness::COLOR_BITNESS_666 : ColorBitness::COLOR_BITNESS_565;
  }

  bool get_18bit_mode() { return this->display_bitness_ == ColorBitness::COLOR_BITNESS_666; }
  void set_color_order(ColorOrder color_order) { this->color_order_ = color_order; }
  ColorOrder gset_color_order() { return this->color_order_; }

  void set_swap_xy(bool swap_xy) { this->swap_xy_ = swap_xy; }
  void set_mirror_x(bool mirror_x) { this->mirror_x_ = mirror_x; }
  void set_mirror_y(bool mirror_y) { this->mirror_y_ = mirror_y; }
  void set_model(std::string model) { this->model_ = model; }

  void set_invert_colors(bool invert);

  void fill(Color color) override;

  void dump_config() override;
  void setup() override;

  void draw_pixel_at(int x, int y, Color color) override;
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, ColorOrder order,
                      ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override;

 protected:
  virtual void buffer_pixel_at(int x, int y, Color color);

  virtual void setup_pins();
  virtual void setup_lcd();
  virtual void setup_buffer();

  void display_buffer() override;

  void reset_();
  void allocate_buffer_(uint32_t buffer_length);
  virtual bool set_addr_window(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2);

  displayInterface* bus_{nullptr};
  uint8_t const *init_sequence_{};

  uint16_t x_low_{0};
  uint16_t y_low_{0};
  uint16_t x_high_{0};
  uint16_t y_high_{0};
  const uint8_t *palette_;
  std::string model_{""};

  ColorBitness buffer_bitness_{ColorBitness::COLOR_BITNESS_565};
  ColorBitness display_bitness_{ColorBitness::COLOR_BITNESS_565};

  uint32_t get_buffer_length_();

  GPIOPin *reset_pin_{nullptr};

  bool pre_invertcolors_ = false;
  ColorOrder color_order_{};
  bool swap_xy_{};
  bool mirror_x_{};
  bool mirror_y_{};

  uint8_t *buffer_{nullptr};
};

}  // namespace display
}  // namespace esphome
