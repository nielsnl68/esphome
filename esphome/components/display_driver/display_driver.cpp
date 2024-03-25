#include "display_driver.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace display {

static const char *const TAG = "DisplayDriver";

static const uint16_t SPI_SETUP_US = 100;         // estimated fixed overhead in microseconds for an SPI write
static const uint16_t SPI_MAX_BLOCK_SIZE = 4092;  // Max size of continuous SPI transfer

DisplayDriver::DisplayDriver(uint8_t const *init_sequence, int16_t width, int16_t height, bool invert_colors)
    : init_sequence_{init_sequence}, pre_invertcolors_{invert_colors} {
  this->set_dimensions(width, height);
  this->preset_init_values();
}

void DisplayDriver::setup() {
  ESP_LOGD(TAG, "Setting up the display_driver");
  if (this->bus_ == nullptr) {
    ESP_LOGE(TAG, "IObus not set.");
    this->mark_failed();
    return;
  }
  this->setup_pins();
  this->bus_->setup(this);

  this->setup_lcd();
  this->view_port_ = Rect();
}

void DisplayDriver::setup_pins() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();  // OUTPUT
    this->reset_pin_->digital_write(true);
    this->reset_();
  }
}

void DisplayDriver::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(20);
    this->reset_pin_->digital_write(true);
    delay(20);
  }
}
void DisplayDriver::setup_lcd() {
  uint8_t cmd, x, num_args;
  const uint8_t *addr = this->init_sequence_;

  this->bus_->begin_commands();

  while ((cmd = *addr++) > 0) {
    x = *addr++;
    num_args = x & 0x7F;
    addr += num_args;
    if (x & 0x80) {
      delay(150);  // NOLINT
    }
  }
  this->finalize_init_values();
  this->bus_->end_commands();

  ESP_LOGD(TAG, "done Init ");
}

// should return the total size: return this->width_ * this->height_ * 2 // 16bit color
// values per bit is huge
size_t DisplayDriver::get_buffer_size_() {
  size_t d_width = this->width_;
  uint8_t devider = this->buffer_bitness_.pixel_devider();

  d_width = ((d_width + (devider - (d_width % devider))) / devider);

  size_t size = d_width * this->height_ * this->buffer_bitness_.bytes_per_pixel;
  return size;
}

bool DisplayDriver::setup_buffer() {
  if (this->is_failed()) {
    return false;
  }
  if (this->buffer_ != nullptr) {
    return true;
  }

  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  this->buffer_bitness_.raw = this->display_bitness_.raw;
  switch (this->buffer_bitness_.bytes_per_pixel) {
    case 1:  // 2 pixels per byte
      this->buffer_ = allocator.allocate(this->get_buffer_size_());
      if (this->buffer_ != nullptr) {
        break;
        ;
      }
      ESP_LOGW(TAG, "Not enough (ps)ram memory for 2 Pixels per byte.");
      this->buffer_bitness_ = ColorBitness::CB565;
    case 2:  // 4 pixels per byte
      this->buffer_ = allocator.allocate(this->get_buffer_size_());
      if (this->buffer_ != nullptr) {
        break;
        ;
      }
      ESP_LOGW(TAG, "Not enough (ps)ram memory for 4 Pixels per byte.");
      this->buffer_bitness_ = ColorBitness::CB565;
    case 3:  // 8 pixels per byte
      this->buffer_ = allocator.allocate(this->get_buffer_size_());
      if (this->buffer_ == nullptr) {
        ESP_LOGE(TAG, "Not enough (ps)ram memory for single bit colors.");
        this->mark_failed();
        return false;
      }
      break;
    default:
      switch (this->buffer_bitness_.bytes_per_pixel) {
        case 3:
          this->buffer_ = allocator.allocate(this->get_buffer_size_());
          if (this->buffer_ != nullptr) {
            break;
            ;
          }
          ESP_LOGW(TAG, "Not enough (ps)ram memory for 3byte colors.");
          this->buffer_bitness_ = ColorBitness::CB565;
          this->buffer_bitness_.byte_aligned = false;
        case 2:
          this->buffer_ = allocator.allocate(this->get_buffer_size_());
          if (this->buffer_ != nullptr) {
            break;
          }
          ESP_LOGW(TAG, "Not enough (ps)ram memory for 2byte colors.");
          if (this->palette_ = nullptr) {
            this->buffer_bitness_ = ColorBitness::CB332;
          } else {
            this->buffer_bitness_ = ColorBitness::CB8I;
          }
        default:
          this->buffer_ = allocator.allocate(this->get_buffer_size_());
          if (this->buffer_ == nullptr) {
            ESP_LOGE(TAG, "Not enough (ps)ram memory for single byte colors.");
            this->mark_failed();
            return false;
          }
          break;
      }
  }
  memset(this->buffer_, 0, this->get_buffer_size_());
  return true;
}

void DisplayDriver::dump_config() {
  LOG_DISPLAY("", "Display Driver:", this);
  ESP_LOGCONFIG(TAG, "  Offset X: %u", this->offset_x_);
  ESP_LOGCONFIG(TAG, "  Offset Y: %u", this->offset_y_);
  // this->padding_.info("display window:");
  if (this->buffer_ != nullptr) {
    this->buffer_bitness_.info("  Buffer Color Dept:", TAG);
  }
  this->display_bitness_.info("  Display Color Dept:", TAG);
  // ESP_LOGCONFIG(TAG, "  Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));

  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  this->bus_->dump_config();

  ESP_LOGCONFIG(TAG, "  Swap_xy: %s", YESNO(this->swap_xy_));
  ESP_LOGCONFIG(TAG, "  Mirror_x: %s", YESNO(this->mirror_x_));
  ESP_LOGCONFIG(TAG, "  Mirror_y: %s", YESNO(this->mirror_y_));

  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "  => Failed on setup: YES!");
  }
  LOG_UPDATE_INTERVAL(this);
}

void DisplayDriver::fill(Color color) {
  ColorBitness old_bitmess;
  old_bitmess.raw = this->buffer_bitness_.raw;
  if (this->buffer_ == nullptr) {
    ESP_LOGD(TAG, "Fill Direct");
    this->buffer_bitness_.raw = this->display_bitness_.raw;

    this->set_addr_window(0, 0, this->width_, this->height_);
    this->bus_->begin_pixels();
  } else {
    ESP_LOGD(TAG, "Fill Buffer");
  }
  auto now = millis();
  uint8_t bytes_per_pixel = this->buffer_bitness_.bytes_per_pixel;
  size_t pixel = 0U, last_pixel = this->get_buffer_size_();
  uint32_t new_color = ColorUtil::from_color(color, this->buffer_bitness_, this->palette_);
  const uint8_t *addr = this->buffer_;

  while (true) {
    if (this->buffer_ == nullptr) {
      this->bus_->send_pixels(Rect(), (uint8_t *) &new_color, bytes_per_pixel);
    } else {
      memcpy((void *) addr, (const void *) &new_color, bytes_per_pixel);
      addr += bytes_per_pixel;
    }
    pixel = pixel + (size_t) bytes_per_pixel;
    if (pixel == last_pixel)
      break;
  }
  if (this->buffer_ == nullptr) {
    this->bus_->end_pixels();
  } else {
    this->view_port_ = Rect(0, 0, this->width_, this->height_);
  }
  this->buffer_bitness_.raw = old_bitmess.raw;
  ESP_LOGD(TAG, "Fill took %dms %d", (unsigned) (millis() - now), pixel);
}

// Tell the display controller where we want to draw pixels.
// when called, the SPI should have already been enabled, only the D/C pin will be toggled here.
// return true when the window can be set, orherwise return false.

void DisplayDriver::display_buffer() {
  // check if something was displayed
  if ((!this->view_port_.is_set()) || (this->buffer_ == nullptr)) {
    return;
  }
  if (this->send_buffer(this->view_port_.x, this->view_port_.y, this->buffer_bitness_, this->buffer_,
                        this->view_port_.x, this->view_port_.y, this->view_port_.w, this->view_port_.h,
                        this->width_ - this->view_port_.x2())) {
    // invalidate watermarks

    this->view_port_ = Rect();
  }
}

bool DisplayDriver::send_buffer(int x, int y, ColorBitness &bitness, const uint8_t *data, int x_in_data, int y_in_data,
                                int width, int height, int x_pad) {
  size_t data_width = width + x_pad + x_in_data;
  uint8_t bytes_per_pixel = bitness.bytes_per_pixel;
  uint8_t devider = bitness.pixel_devider();
  ESP_LOGD(TAG, "Start display(display:[%d, %d], in_data:[%d + %d, %d], dimension:[%d, %d] %d", x, y, x_in_data, x_pad,
           y_in_data, width, height, data_width);
  auto now = millis();

  if (!this->set_addr_window(x, y, x + width - 1, y + height - 1)) {
    if (data == this->buffer_) {
      x_pad = 0;
      x_in_data = 0;
      y_in_data = 0;
      width = this->width_;
      height = this->height_;
    } else {
      return false;
    }
  }
  this->bus_->begin_pixels();

  if (devider > 1) {
    x_in_data = (x_in_data - (x_in_data % devider)) / devider;
    width = (width + (devider - (width % devider))) / devider;
    data_width = (data_width + (devider - (data_width % devider))) / devider;
  }
  data_width *= bytes_per_pixel;
  size_t start_pos = (y_in_data * data_width) + x_in_data;
  const uint8_t *addr = data + start_pos;
  size_t view_width = width * bytes_per_pixel;

  if (bitness == this->display_bitness_) {  //  && sw_time < mw_time
    if (x_in_data == 0 && x_pad == 0 && y_in_data == 0) {
      this->bus_->send_pixels(Rect(x, y, width, height), addr, view_width * height);
    } else {
      ESP_LOGD(TAG, "Copy buffer to display vw:%d dw:%d h:%d", view_width, view_width, height);
      for (uint16_t row = 0; row < height; row++) {
        // ESP_LOGD(TAG, "send from : 0x%x Row:%d", addr, row);
        this->bus_->send_pixels(Rect(x, y + row, width, y + row + 1), addr, view_width);
        addr += data_width;

        App.feed_wdt();
      }
    }
  } else {
    ESP_LOGD(TAG, "confird buffer before sending to display");
    uint8_t display_bytes_per_pixel = this->display_bitness_.bytes_per_pixel;
    size_t col = 0, row = 0;  // remaining number of pixels to write
    size_t bytes_send = 0;    // index into transfer_buffer
    uint32_t buf_color = 0;
    uint32_t disp_color = 0;
    uint8_t disp_part = 0;

    if (this->batch_buffer_ == nullptr) {
      ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
      this->batch_buffer_ = allocator.allocate(this->get_native_width() * display_bytes_per_pixel);
    }
    if (this->batch_buffer_ == nullptr) {
      ESP_LOGE(TAG, "Not enough (ps)ram memory for batch_buffer_.");
      this->mark_failed();
      return false;
    }

    while (true) {
      memcpy((void *) &buf_color, (const void *) addr, bytes_per_pixel);
      //  ESP_LOGD(TAG, "display (col:%d, row:%d,  start_pos:%d, bytes_send:%d ", col, row, start_pos, bytes_send);
      for (auto buf_part = 0; buf_part < devider; buf_part++) {
        Color color = ColorUtil::to_color(buf_color, bitness, this->palette_, buf_part);
        disp_color = ColorUtil::from_color(color, this->display_bitness_, this->palette_, disp_color, disp_part);
        if (++disp_part == this->display_bitness_.pixel_devider()) {
          memcpy((void *) (this->batch_buffer_ + bytes_send), (const void *) &disp_color, display_bytes_per_pixel);
          disp_part = 0;
          disp_color = 0;
          bytes_send += display_bytes_per_pixel;
          if (bytes_send >= TRANSFER_BUFFER_SIZE - display_bytes_per_pixel) {
            this->bus_->send_pixels(Rect(), this->batch_buffer_, bytes_send);
            bytes_send = 0;
            App.feed_wdt();
          }
        }
      }
      addr += bytes_per_pixel;
      // end of line? Skip to the next.
      if (++col == width) {
        start_pos += data_width;
        addr = data + start_pos;
        col = 0;
        row++;
      }
      if (row >= height) {
        break;
      }
    }

    // flush any balance.
    if (bytes_send != 0) {
      this->bus_->send_pixels(Rect(), this->batch_buffer_, bytes_send);
    }
  }

  this->bus_->end_pixels();

  ESP_LOGD(TAG, "Data write took %dms", (unsigned) (millis() - now));
  return true;
}

// note that this bypasses the buffer and writes directly to the display.
void DisplayDriver::draw_pixels_at(int x, int y, ColorBitness &bitness, const uint8_t *data, int x_in_data, int y_in_data, int width,
                                   int height, int x_pad) {
  if (width <= 0 || height <= 0)
    return;
  // if color mapping or software rotation is required, hand this off to the parent implementation. This will
  // do color conversion pixel-by-pixel into the buffer and draw it later. If this is happening the user has not
  // configured the renderer well.
  if (this->rotation_ == DISPLAY_ROTATION_0_DEGREES && !this->is_buffered()) {
    if (this->send_buffer(x, y, bitness, data, x_in_data, y_in_data, width, height, x_pad)) {
      return;
    }
  }
  Display::draw_pixels_at(x, y, bitness, data, x_in_data, y_in_data, width, height, x_pad);
}

void DisplayDriver::set_invert_colors(bool invert) {
  if (is_ready()) {
    ESP_LOGE(TAG, "You cant set invert color anymore.");
    return;
  }
  this->pre_invertcolors_ = invert;
}

void HOT DisplayDriver::draw_pixel_at(int x, int y, Color color) {
  if (!this->get_clipping().inside(x, y))
    return;  // NOLINT

  switch (this->rotation_) {
    case DISPLAY_ROTATION_0_DEGREES:
      break;
    case DISPLAY_ROTATION_90_DEGREES:
      std::swap(x, y);
      x = this->width_ - x - 1;
      break;
    case DISPLAY_ROTATION_180_DEGREES:
      x = this->width_ - x - 1;
      y = this->height_ - y - 1;
      break;
    case DISPLAY_ROTATION_270_DEGREES:
      std::swap(x, y);
      y = this->height_ - y - 1;
      break;
  }
  this->buffer_pixel_at(x, y, color);
  App.feed_wdt();
}

void HOT DisplayDriver::buffer_pixel_at(int x, int y, Color color) {
  if (x >= this->width_ || x < 0 || y >= this->height_ || y < 0 || !this->setup_buffer()) {
    ESP_LOGD(TAG, "buffer_pixel_at skipt.");
    return;
  }

  uint32_t old_color, new_color;

  uint8_t bytes_per_pixel = this->buffer_bitness_.bytes_per_pixel;
  uint8_t devider = this->buffer_bitness_.pixel_devider();
  const uint8_t *addr = this->buffer_;
  size_t width = this->width_, x_buf = x;

  if (devider > 1) {
    x_buf = (x - (x % devider)) / devider;
    width = (this->width_ + (devider - (this->width_ % devider))) / devider;
  }

  addr += ((y * width) + x_buf) * bytes_per_pixel;
  memcpy((void *) &old_color, (const void *) addr, bytes_per_pixel);
  new_color = ColorUtil::from_color(color, this->buffer_bitness_, this->palette_, old_color, x);

  if (old_color != new_color) {
    memcpy((void *) addr, (const void *) &new_color, bytes_per_pixel);
    this->view_port_.include(x, y);
  }
}

// ======================== IOBus

void IOBus::send_command(uint8_t command, const uint8_t *data, size_t len) {
  this->begin_commands();
  this->command(command);  // Send the command byte

  if (len > 0) {
    this->data(data, len);
  }
  this->end_commands();
}

void IOBus::send_data(const uint8_t *data, size_t len) {
  this->begin_commands();
  this->data(data, len);
  this->end_commands();
}

void IOBus::command(uint8_t value) {
  this->start_command();
  this->send_byte(value);
  this->end_command();
}

void IOBus::data(const uint8_t *value, size_t len) {
  this->start_data();
  if (len == 1) {
    this->send_byte(*value);
  } else {
    this->send_array(value, len);
  }
  this->end_data();
}

// ======================== SPIBus

void SPIBus::setup(DisplayDriver *driver) {
  this->dc_pin_->setup();  // OUTPUT
  this->dc_pin_->digital_write(false);

  this->spi_setup();
}

void SPIBus::dump_config() {
  ESP_LOGCONFIG(TAG, "  Interface: %s", "SPI");
  LOG_PIN("    CS Pin: ", this->cs_);
  LOG_PIN("    DC Pin: ", this->dc_pin_);
  ESP_LOGCONFIG(TAG, "    Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));
}

void SPIBus::start_command() { this->dc_pin_->digital_write(false); }
void SPIBus::end_command() { this->dc_pin_->digital_write(true); }

void SPIBus::start_data() { this->dc_pin_->digital_write(true); }

void SPIBus::begin_commands() {
  if (this->enabled_ == 0) {
    this->enable();
  }
  this->enabled_++;
}
void SPIBus::end_commands() {
  this->enabled_--;
  if (this->enabled_ == 0) {
    this->disable();
  }
}

void SPIBus::send_byte(uint8_t data) { this->write_byte(data); }
void SPIBus::send_array(const uint8_t *data, size_t len) { this->write_array(data, len); };

// ======================== SPI16DBus

void SPI16DBus::dump_config() {
  ESP_LOGCONFIG(TAG, "  Interface: %s", "SPI16D");
  LOG_PIN("    CS Pin: ", this->cs_);
  LOG_PIN("    DC Pin: ", this->dc_pin_);
  ESP_LOGCONFIG(TAG, "    Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));
}

void SPI16DBus::data(const uint8_t *value, size_t len) {
  this->start_data();
  if (len == 1) {
    this->send_byte(0x00);
    this->send_byte(value[0]);
  } else {
    this->send_array(value, len);
  }
  this->end_data();
}

// ======================== RGBbus

#ifdef USE_ESP32_VARIANT_ESP32S3
void RGBBus::setup(DisplayDriver *driver) {
  esph_log_config(TAG, "Setting up RPI_DPI_RGB");
  esp_lcd_rgb_panel_config_t config{};
  config.flags.fb_in_psram = 1;
  config.timings.h_res = driver->get_native_width();
  config.timings.v_res = this->get_native_height();
  config.timings.hsync_pulse_width = this->hsync_pulse_width_;
  config.timings.hsync_back_porch = this->hsync_back_porch_;
  config.timings.hsync_front_porch = this->hsync_front_porch_;
  config.timings.vsync_pulse_width = this->vsync_pulse_width_;
  config.timings.vsync_back_porch = this->vsync_back_porch_;
  config.timings.vsync_front_porch = this->vsync_front_porch_;
  config.timings.flags.pclk_active_neg = this->pclk_inverted_;
  config.timings.pclk_hz = this->pclk_frequency_;
  config.clk_src = LCD_CLK_SRC_PLL160M;
  config.sram_trans_align = 64;
  config.psram_trans_align = 64;
  size_t data_pin_count = sizeof(this->data_pins_) / sizeof(this->data_pins_[0]);
  for (size_t i = 0; i != data_pin_count; i++) {
    config.data_gpio_nums[i] = this->data_pins_[i]->get_pin();
  }
  config.data_width = data_pin_count;
  config.disp_gpio_num = -1;
  config.hsync_gpio_num = this->hsync_pin_->get_pin();
  config.vsync_gpio_num = this->vsync_pin_->get_pin();
  config.de_gpio_num = this->de_pin_->get_pin();
  config.pclk_gpio_num = this->pclk_pin_->get_pin();
  esp_err_t err = esp_lcd_new_rgb_panel(&config, &this->handle_);
  if (err != ESP_OK) {
    esph_log_e(TAG, "lcd_new_rgb_panel failed: %s", esp_err_to_name(err));
  }
  ESP_ERROR_CHECK(esp_lcd_panel_reset(this->handle_));
  ESP_ERROR_CHECK(esp_lcd_panel_init(this->handle_));
  esph_log_config(TAG, "RPI_DPI_RGB setup complete");
}

void RGBBus::dump_config() {
  ESP_LOGCONFIG("", "RPI_DPI_RGB LCD");
  LOG_PIN("  DE Pin: ", this->de_pin_);
  size_t data_pin_count = sizeof(this->data_pins_) / sizeof(this->data_pins_[0]);
  for (size_t i = 0; i != data_pin_count; i++)
    ESP_LOGCONFIG(TAG, "  Data pin %d: %s", i, (this->data_pins_[i])->dump_summary().c_str());
}

void RGBBus::send_pixels(Rect window, const uint8_t *data, size_t len) {
  esp_err_t err = esp_lcd_panel_draw_bitmap(this->handle_, window.x, window.y, window.x2(), window.x2(), data);
  if (err != ESP_OK)
    ESP_LOGE(TAG, "lcd_lcd_panel_draw_bitmap failed: %s", esp_err_to_name(err));
};
#endif

}  // namespace display
}  // namespace esphome
