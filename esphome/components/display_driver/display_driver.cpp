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

// store a 16 bit value in a buffer, big endian.
static inline void put16_be(uint8_t *buf, uint16_t value) {
  buf[0] = value >> 8;
  buf[1] = value;
}

DisplayDriver::DisplayDriver(uint8_t const *init_sequence, int16_t width, int16_t height, bool invert_colors)
    : init_sequence_{init_sequence}, pre_invertcolors_{invert_colors} {
  this->set_dimensions(width, height);
  uint8_t cmd, num_args, bits;
  const uint8_t *addr = init_sequence;
  while ((cmd = *addr++) != 0) {
    num_args = *addr++ & 0x7F;
    bits = *addr;
    switch (cmd) {
      case ILI9XXX_MADCTL: {
        this->swap_xy_ = (bits & MADCTL_MV) != 0;
        this->mirror_x_ = (bits & MADCTL_MX) != 0;
        this->mirror_y_ = (bits & MADCTL_MY) != 0;
        this->set_color_order((bits & MADCTL_BGR) ? COLOR_ORDER_BGR : COLOR_ORDER_RGB);
        break;
      }

      case ILI9XXX_PIXFMT: {
        if ((bits & 0xF) == 6)
          this->display_bitness_ = ColorBitness::COLOR_BITNESS_666;
        break;
      }

      default:
        break;
    }
    addr += num_args;
  }
}

void DisplayDriver::setup() {
  ESP_LOGD(TAG, "Setting up the display_driver");
  if (this->bus_ == nullptr) {
    this->mark_failed();
    return;
  }
  this->setup_pins();
  this->bus_->setup();

  this->setup_lcd();

  this->x_low_ = this->width_;
  this->y_low_ = this->height_;
  this->x_high_ = 0;
  this->y_high_ = 0;
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
    delay(10);
    this->reset_pin_->digital_write(true);
    delay(10);
  }
}

void DisplayDriver::setup_lcd() {
  uint8_t cmd, x, num_args;
  const uint8_t *addr = this->init_sequence_;
  this->bus_->start();
  while ((cmd = *addr++) > 0) {
    x = *addr++;
    num_args = x & 0x7F;

    this->bus_->send_command(cmd, addr, num_args);
    addr += num_args;
    if (x & 0x80) {
      this->bus_->end();
      delay(150);  // NOLINT
      this->bus_->start();
    }
  }

  this->bus_->send_command(this->pre_invertcolors_ ? ILI9XXX_INVON : ILI9XXX_INVOFF);

  // custom x/y transform and color order
  uint8_t mad = this->display_bitness_.color_order == COLOR_ORDER_BGR ? MADCTL_BGR : MADCTL_RGB;
  if (this->swap_xy_)
    mad |= MADCTL_MV;
  if (this->mirror_x_)
    mad |= MADCTL_MX;
  if (this->mirror_y_)
    mad |= MADCTL_MY;
  this->bus_->send_command(ILI9XXX_MADCTL, &mad, 1);

  mad = this->display_bitness_ == ColorBitness::COLOR_BITNESS_666 ? 0x66 : 0x55;
  this->bus_->send_command(ILI9XXX_PIXFMT, &mad, 1);

  this->bus_->end();
}

// should return the total size: return this->width_ * this->height_ * 2 // 16bit color
// values per bit is huge
size_t DisplayDriver::get_buffer_length_() {
  size_t d_width = this->padding_.w;
  uint8_t devider = this->buffer_bitness_.pixel_devider();

  d_width = (d_width + (devider - (d_width % devider))) / devider;

  return d_width * this->padding_.h * this->buffer_bitness_.bytes_per_pixel;
}

bool DisplayDriver::setup_buffer() {
  if (this->buffer_ != nullptr) {
    return true;
  }
  this->buffer_bitness_ = this->display_bitness_;
  switch (this->buffer_bitness_.bytes_per_pixel) {
    case 3:
      this->allocate_buffer_(this->get_buffer_length_());
      if (this->buffer_ != nullptr) {
        return true;
        ;
      }
      this->buffer_bitness_ = ColorBitness::COLOR_BITNESS_565;
      this->buffer_bitness_.right_aligned = false;
    case 2:
      this->allocate_buffer_(this->get_buffer_length_());
      if (this->buffer_ != nullptr) {
        return true;
      }
      if (this->palette_ = nullptr) {
        this->buffer_bitness_ = ColorBitness::COLOR_BITNESS_332;
      } else {
        this->buffer_bitness_ = ColorBitness::COLOR_BITNESS_8I;
      }
    default:
      this->allocate_buffer_(this->get_buffer_length_());
      if (this->buffer_ == nullptr) {
        this->mark_failed();
        return false;
      }
      return true;
  }
}

void DisplayDriver::allocate_buffer_(uint32_t buffer_length) {
  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  this->buffer_ = allocator.allocate(buffer_length);
}

void dump_bridness(std::string title, ColorBitness bitness) {
  std::string depth;
  switch (bitness.bits_per_pixel) {
    case 1:
      depth = "Monochrome";
      break;
    case 2:
      depth = "4 colors/grayscale";
      break;
    case 4:
      depth = "16 colors/grayscale";
      break;
    case 8:
      if (bitness.indexed) {
        depth = "8bit Indexed";
      } else if (bitness.grayscale) {
        depth = "256 colors/grayscale";
      } else {
        depth = "8bit 332 mode";
      }
      break;
    case 16:
      depth = "16bit 565 mode";
      break;
    case 18:
      depth = "18bit 666 mode";
      break;
    case 24:
      depth = "24bit 888 mode";
      break;
    default:
      depth = "Unknown color depth.";
      break;
  }
  ESP_LOGCONFIG(TAG, title.c_str(), depth.c_str());
}

void DisplayDriver::dump_config() {
  LOG_DISPLAY("", "Display Driver:", this);
  this->padding_.info("display window:");
  if (this->buffer_ != nullptr) {
    dump_bridness("  Buffer Color Dept: %s", this->buffer_bitness_);
  }
  dump_bridness("  Display Color Dept: %s", this->display_bitness_);
  // ESP_LOGCONFIG(TAG, "  Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));

  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  // LOG_PIN("  CS Pin: ", this->cs_);
  // LOG_PIN("  DC Pin: ", this->dc_pin_);

  ESP_LOGCONFIG(TAG, "  Swap_xy: %s", YESNO(this->swap_xy_));
  ESP_LOGCONFIG(TAG, "  Mirror_x: %s", YESNO(this->mirror_x_));
  ESP_LOGCONFIG(TAG, "  Mirror_y: %s", YESNO(this->mirror_y_));

  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "  => Failed to init Memory: YES!");
  }
  LOG_UPDATE_INTERVAL(this);
}

void DisplayDriver::fill(Color color) {
  if (!this->setup_buffer())
    return;
  uint8_t bytes_per_pixel = this->buffer_bitness_.bytes_per_pixel;
  size_t count = this->get_buffer_length_();
  const uint8_t *addr = this->buffer_;
  uint32_t new_color = ColorUtil::from_color(color, this->buffer_bitness_, this->palette_);

  while (count--) {
    memcpy((void *) addr, (const void *) &new_color, bytes_per_pixel);
    addr += bytes_per_pixel;
  }
}

void DisplayDriver::display_buffer() {
  // check if something was displayed
  if ((this->x_high_ < this->x_low_) || (this->y_high_ < this->y_low_) || (this->buffer_ == nullptr)) {
    return;
  }
  if (this->display_buffer_(this->buffer_, this->x_low_, this->y_low_, this->x_low_, this->y_low_,
                            this->y_high_ - this->x_low_ + 1, this->y_high_ - this->y_low_ + 1, this->buffer_bitness_,
                            this->width_ - this->x_high_ + 1)) {
    // invalidate watermarks
    this->x_low_ = this->padding_.w;
    this->y_low_ = this->padding_.h;
    this->x_high_ = 0;
    this->y_high_ = 0;
  }
}

// Tell the display controller where we want to draw pixels.
// when called, the SPI should have already been enabled, only the D/C pin will be toggled here.
// return true when the window can be set, orherwise return false.

bool DisplayDriver::set_addr_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  this->bus_->send_command(ILI9XXX_CASET);
  this->bus_->send_data(x1 >> 8);
  this->bus_->send_data(x1 & 0xFF);
  this->bus_->send_data(x2 >> 8);
  this->bus_->send_data(x2 & 0xFF);
  this->bus_->send_command(ILI9XXX_PASET);  // Page address set
  this->bus_->send_data(y1 >> 8);
  this->bus_->send_data(y1 & 0xFF);
  this->bus_->send_data(y2 >> 8);
  this->bus_->send_data(y2 & 0xFF);
  this->bus_->send_command(ILI9XXX_RAMWR);  // Write to RAM
  return true;
}

bool DisplayDriver::display_buffer_(const uint8_t *data, int x_display, int y_display, int x_in_data, int y_in_data,
                                    int width, int height, ColorBitness bitness, int x_pad) {
  size_t x_low = x_in_data;
  size_t y_low = y_in_data;
  size_t x_high = x_in_data + width - 1;
  size_t y_high = y_in_data + height - 1;
  size_t data_width = y_high + x_pad;
  uint8_t bytes_per_pixel = bitness.bytes_per_pixel;
  uint8_t devider = bitness.pixel_devider();

  auto now = millis();

  this->bus_->start();
  if (!this->set_addr_window(x_display, y_display, x_display + width - 1, y_display + height - 1)) {
    if (data == this->buffer_) {
      x_low = y_low = 0;
      x_high = this->width_;
      y_high = this->height_;
    } else {
      this->bus_->end();
      return false;
    }
  }
  if (devider > 1) {
    x_low = (x_low - (x_low % devider)) / devider;
    x_high = (x_high + (devider - (x_high % devider))) / devider;
    data_width = (data_width + (devider - (data_width % devider))) / devider;
  }
  data_width *= bytes_per_pixel;
  size_t start_pos = (y_low * data_width) + x_low;
  const uint8_t *addr = data + start_pos;
  size_t view_width = (x_high - x_low + 1) * bytes_per_pixel;

  ESP_LOGV(TAG,
           "Start display(xlow:%d, ylow:%d, xhigh:%d, yhigh:%d, width:%d, "
           "height:%d, buffer=%4x, display=%4x)",
           x_low, y_low, x_high, y_high, x_high - x_low + 1, y_high - y_low + 1, bitness, this->display_bitness_);

  if (bitness == this->display_bitness_) {  //  && sw_time < mw_time
    // 16 bit mode maps directly to display format

    for (uint16_t row = y_low; row < y_high; row++) {
      this->bus_->send_data(addr , view_width );
      addr += data_width;
    }
  } else {
    uint8_t transfer_buffer[TRANSFER_BUFFER_SIZE];

    ESP_LOGV(TAG, "Doing multiple write");
    uint8_t display_bytes_per_pixel = this->display_bitness_.bytes_per_pixel;
    size_t x_buffer = x_low;  // remaining number of pixels to write
    size_t bytes_send = 0;           // index into transfer_buffer
    uint32_t buf_color;
    uint32_t disp_color = 0;
    uint8_t disp_part = 0;
    while (true) {
      memcpy((void *) &buf_color, (const void *) addr, bytes_per_pixel);
      for (auto buf_part = 0; buf_part < devider; buf_part++) {
        Color color = ColorUtil::to_color(buf_color, bitness, this->palette_, buf_part);
        disp_color = ColorUtil::from_color(color, this->display_bitness_, this->palette_, disp_color, disp_part);
        if (++disp_part == this->display_bitness_.pixel_devider()) {
          memcpy((void *) &transfer_buffer[bytes_send], (const void *) disp_color, display_bytes_per_pixel);
          disp_part = 0;
          disp_color = 0;
          bytes_send += display_bytes_per_pixel;
          if (bytes_send == TRANSFER_BUFFER_SIZE) {
            this->bus_->send_data(transfer_buffer, bytes_send);
            bytes_send = 0;
            App.feed_wdt();
          }
        }
      }
      addr++;
      // end of line? Skip to the next.
      if (++x_buffer == x_high) {
        start_pos += data_width;
        addr = data + start_pos;
        x_buffer = x_low;
        y_low++;
      }
      if (y_low >= y_high) {
        break;
      }
    }

    // flush any balance.
    if (bytes_send != 0) {
      this->bus_->send_data(transfer_buffer, bytes_send);
    }
  }
  this->bus_->end();
  ESP_LOGV(TAG, "Data write took %dms", (unsigned) (millis() - now));
  return true;
}

// note that this bypasses the buffer and writes directly to the display.
void DisplayDriver::draw_pixels_at(const uint8_t *data, int x_display, int y_display, int x_in_data, int y_in_data,
                                   int width, int height, ColorBitness bitness, int x_pad) {
  if (width <= 0 || height <= 0)
    return;
  // if color mapping or software rotation is required, hand this off to the parent implementation. This will
  // do color conversion pixel-by-pixel into the buffer and draw it later. If this is happening the user has not
  // configured the renderer well.
  if (this->rotation_ == DISPLAY_ROTATION_0_DEGREES && !this->is_buffered()) {
    if (this->display_buffer_(data, x_display, y_display, x_in_data, y_in_data, width, height, bitness, x_pad)) {
      return;
    }
  }
  Display::draw_pixels_at(data, x_display, y_display, x_in_data, y_in_data, width, height, bitness, x_pad);
}

void DisplayDriver::set_invert_colors(bool invert) {
  this->pre_invertcolors_ = invert;
  if (is_ready()) {
    this->bus_->send_command(invert ? ILI9XXX_INVON : ILI9XXX_INVOFF);
  }
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
    return;
  }

  uint32_t old_color, new_color;

  uint8_t bytes_per_pixel = this->buffer_bitness_.bytes_per_pixel;
  uint8_t devider = this->buffer_bitness_.pixel_devider();
  const char *addr = (char *) this->buffer_;
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
    // low and high watermark may speed up drawing from buffer
    if (x < this->x_low_)
      this->x_low_ = x;
    if (y < this->y_low_)
      this->y_low_ = y;
    if (x > this->x_high_)
      this->x_high_ = x;
    if (y > this->y_high_)
      this->y_high_ = y;
  }
}

// ======================== displayInterface

void displayInterface::send_command(uint8_t command, const uint8_t *data, uint8_t len, bool start) {
  if (start || this->always_start_) {
    this->start();
  }
  this->command(command);  // Send the command byte

  if (len > 0) {
    this->data(data, len);
  }

  if (start || this->always_start_) {
    this->end();
  }
}

void displayInterface::send_data(const uint8_t *data, uint8_t len, bool start) {
  if (start || this->always_start_) {
    this->start();
  }
  this->data(data, len);
  if (start || this->always_start_) {
    this->end();
  }
}

void displayInterface::command(uint8_t value) {
  this->start_command();
  this->send_byte(value);
  this->end_command();
}

void displayInterface::data(const uint8_t *value, uint16_t len) {
  this->start_data();
  if (len == 1) {
    this->send_byte(value[0]);
  } else {
    this->send_array((uint8_t *) value, len);
  }
  this->end_data();
}

// ======================== SPI_Interface

void SPI_Interface::setup() {
  this->dc_pin_->setup();  // OUTPUT
  this->dc_pin_->digital_write(false);

  this->spi_setup();
}

void SPI_Interface::start_command() { this->dc_pin_->digital_write(false); }
void SPI_Interface::end_command() { this->dc_pin_->digital_write(true); }

void SPI_Interface::start_data() { this->dc_pin_->digital_write(true); }

void SPI_Interface::start() {
  if (this->enabled_ == 0) {
    this->enable();
  }
  this->enabled_++;
}
void SPI_Interface::end() {
  this->enabled_--;
  if (this->enabled_ == 0) {
    this->end();
    this->enabled_ = 0;
  }
}

void SPI_Interface::send_byte(uint8_t data) { this->write_byte(data); }
void SPI_Interface::send_array(const uint8_t *data, int16_t len) { this->write_array(data, len); };

// ======================== SPI16D_Interface

void SPI16D_Interface::data(const uint8_t *value, uint16_t len) {
  this->start_data();
  if (len == 1) {
    this->send_byte(0x00);
    this->send_byte(value[0]);
  } else {
    this->send_array(value, len);
  }
  this->end_data();
}

}  // namespace display
}  // namespace esphome
