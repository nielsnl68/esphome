#pragma once
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display.h"

namespace esphome {
namespace display {

const size_t TRANSFER_BUFFER_SIZE = 126;  // ensure this is divisible by 6

class DisplayDriver;

class IOBus {
 public:
  virtual void setup(DisplayDriver *driver){};

  virtual void begin_commands(){};
  virtual void end_commands(){};
  inline void send_command(uint8_t command) { this->send_command(command, nullptr, 0); }
  void send_command(uint8_t command, const uint8_t *data, size_t len);
  inline void send_data(const uint8_t data) { this->send_data(&data, 1); }
  void send_data(const uint8_t *data, size_t len = 1);

  virtual void begin_pixels() { this->begin_commands(); }
  virtual void end_pixels() { this->end_commands(); }
  void send_pixels(Rect window, const uint8_t *data) { this->send_pixels(window, data, 1); }
  virtual void send_pixels(Rect window, const uint8_t *data, size_t len) { this->send_data(data, len); }

  virtual void dump_config(){};

 protected:
  virtual void start_command(){};
  virtual void end_command(){};
  virtual void start_data(){};
  virtual void end_data(){};

  virtual void command(uint8_t value);
  virtual void data(const uint8_t *value, size_t len);
  virtual void send_byte(uint8_t data) = 0;
  virtual void send_array(const uint8_t *data, size_t len) = 0;

  bool always_begin_transaction_{false};
};

class SPIBus : public IOBus,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                     spi::DATA_RATE_40MHZ> {
 public:
  void setup(DisplayDriver *driver) override;
  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }

  void begin_commands() override;
  void end_commands() override;

  void dump_config() override;

 protected:
  void start_command() override;
  void end_command() override;
  void start_data() override;

  void send_byte(uint8_t data) override;
  void send_array(const uint8_t *data, size_t len) override;
  GPIOPin *dc_pin_{nullptr};
  int enabled_{0};
};

class SPI16DBus : public SPIBus {
  void dump_config() override;

 protected:
  void data(const uint8_t *value, size_t len) override;
};

#ifdef USE_ESP32_VARIANT_ESP32S3
class RGBBus : public IOBus {
  void setup(DisplayDriver *driver) override;
  void dump_config() override;

  void begin_pixels() override {}
  void end_pixels() override {}
  void send_pixels(Rect window, const uint8_t *data, size_t len);

  void add_data_pin(InternalGPIOPin *data_pin, size_t index) { this->data_pins_[index] = data_pin; };
  void set_de_pin(InternalGPIOPin *de_pin) { this->de_pin_ = de_pin; }

  void set_pclk_pin(InternalGPIOPin *pclk_pin) { this->pclk_pin_ = pclk_pin; }
  void set_pclk_frequency(uint32_t pclk_frequency) { this->pclk_frequency_ = pclk_frequency; }
  void set_pclk_inverted(bool inverted) { this->pclk_inverted_ = inverted; }

  void set_vsync_pin(InternalGPIOPin *vsync_pin) { this->vsync_pin_ = vsync_pin; }
  void set_vsync_pulse_width(uint16_t vsync_pulse_width) { this->vsync_pulse_width_ = vsync_pulse_width; }
  void set_vsync_back_porch(uint16_t vsync_back_porch) { this->vsync_back_porch_ = vsync_back_porch; }
  void set_vsync_front_porch(uint16_t vsync_front_porch) { this->vsync_front_porch_ = vsync_front_porch; }

  void set_hsync_pin(InternalGPIOPin *hsync_pin) { this->hsync_pin_ = hsync_pin; }
  void set_hsync_back_porch(uint16_t hsync_back_porch) { this->hsync_back_porch_ = hsync_back_porch; }
  void set_hsync_front_porch(uint16_t hsync_front_porch) { this->hsync_front_porch_ = hsync_front_porch; }
  void set_hsync_pulse_width(uint16_t hsync_pulse_width) { this->hsync_pulse_width_ = hsync_pulse_width; }

 protected:
  void data(const uint8_t *value, size_t len) override{};
  void send_byte(uint8_t data) override{};
  void send_array(const uint8_t *data, size_t len) override{};

  InternalGPIOPin *data_pins_[16] = {};
  InternalGPIOPin *de_pin_{nullptr};

  InternalGPIOPin *pclk_pin_{nullptr};
  uint32_t pclk_frequency_ = 16 * 1000 * 1000;
  bool pclk_inverted_{true};

  InternalGPIOPin *hsync_pin_{nullptr};
  uint16_t hsync_front_porch_ = 8;
  uint16_t hsync_pulse_width_ = 4;
  uint16_t hsync_back_porch_ = 8;

  InternalGPIOPin *vsync_pin_{nullptr};
  uint16_t vsync_front_porch_ = 8;
  uint16_t vsync_pulse_width_ = 4;
  uint16_t vsync_back_porch_ = 8;

  esp_lcd_panel_handle_t handle_{};
};
#endif

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
  void draw_pixels_at(const uint8_t *data, int x_display, int y_display, int x_in_data, int y_in_data, int width,
                      int height, ColorBitness bitness, int x_pad) override;

  bool is_buffered() { return (this->buffer_ != nullptr); }
  float get_setup_priority() const override { return setup_priority::HARDWARE; };

 protected:
  virtual void preset_init_values(){};
  virtual void finalize_init_values(){};

  virtual void buffer_pixel_at(int x, int y, Color color);
  virtual bool set_addr_window(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2) { return false; }
  virtual bool send_buffer(const uint8_t *data, int x_display, int y_display, int x_in_data, int y_in_data, int width,
                           int height, ColorBitness bitness, int x_pad);

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
