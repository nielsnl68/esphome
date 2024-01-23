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
        this->color_order_ = (bits & MADCTL_BGR) ? COLOR_ORDER_BGR : COLOR_ORDER_RGB;
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
  uint8_t mad = this->color_order_ == COLOR_ORDER_BGR ? MADCTL_BGR : MADCTL_RGB;
  if (this->swap_xy_)
    mad |= MADCTL_MV;
  if (this->mirror_x_)
    mad |= MADCTL_MX;
  if (this->mirror_y_)
    mad |= MADCTL_MY;
  this->bus_->send_command(ILI9XXX_MADCTL, &mad, 1);

  mad = this->display_bitness_==ColorBitness::COLOR_BITNESS_666 ? 0x66 : 0x55;
  this->bus_->send_command(ILI9XXX_PIXFMT, &mad, 1);

  this->bus_->end();
}

// should return the total size: return this->width_ * this->height_ * 2 // 16bit color
// values per bit is huge
size_t DisplayDriver::get_buffer_length_() {
  size_t result = this->width_ * this->height_;
  switch (this->buffer_bitness_) {
    case ColorBitness::COLOR_BITNESS_1:
      return result / 8;
    case ColorBitness::COLOR_BITNESS_2:
      return result / 4;
    case ColorBitness::COLOR_BITNESS_4:
      return result / 2;
    case ColorBitness::COLOR_BITNESS_565:
      return result * 2;
    case ColorBitness::COLOR_BITNESS_666:
    case ColorBitness::COLOR_BITNESS_888:
      return result * 3;
    default:
      return result;
  }
}

void DisplayDriver::setup_buffer() {
  if (this->buffer_ != nullptr) {
    return;
  }
  this->buffer_bitness_ = this->display_bitness_;
  switch (this->buffer_bitness_) {
    case ColorBitness::COLOR_BITNESS_666:
    case ColorBitness::COLOR_BITNESS_888:
      this->allocate_buffer_(this->get_buffer_length_());
      if (this->buffer_ != nullptr) {
        return;
      }
      this->buffer_bitness_ = ColorBitness::COLOR_BITNESS_565;
    case ColorBitness::COLOR_BITNESS_565:
      this->allocate_buffer_(this->get_buffer_length_());
      if (this->buffer_ != nullptr) {
        return;
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
      }
  }
}

void DisplayDriver::allocate_buffer_(uint32_t buffer_length) {
  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  this->buffer_ = allocator.allocate(buffer_length);
}

void dunp_bridness(std::string title, ColorBitness bridness) {
  std::string depth;
  switch (bridness) {
    case ColorBitness::COLOR_BITNESS_1:
      depth = "Monochrome";
      break;
    case ColorBitness::COLOR_BITNESS_2:
      depth = "4 colors/grayscale";
      break;
    case ColorBitness::COLOR_BITNESS_4:
      depth = "16 colors/grayscale";
      break;
    case ColorBitness::COLOR_BITNESS_8:
      depth = "256 colors/grayscale";
      break;
    case ColorBitness::COLOR_BITNESS_8I:
      depth = "8bit Indexed";
      break;
    case ColorBitness::COLOR_BITNESS_565:
      depth = "16bit 565 mode";
      break;
    case ColorBitness::COLOR_BITNESS_666:
      depth = "18bit 666 mode";
      break;
    case ColorBitness::COLOR_BITNESS_888:
      depth = "24bit 888 mode";
      break;
    default:
      depth = "8bit 332 mode";
      break;
  }
  ESP_LOGCONFIG(TAG, title.c_str(), depth.c_str());
}

void DisplayDriver::dump_config() {
  LOG_DISPLAY("", "Display Driver:", this);
  ESP_LOGCONFIG(TAG, "  Width Offset: %u", this->offset_x_);
  ESP_LOGCONFIG(TAG, "  Height Offset: %u", this->offset_y_);
  if (this->buffer_ != nullptr) {
    dunp_bridness("  Buffer Color Dept: %s", this->buffer_bitness_);
  }
  dunp_bridness("  Display Color Dept: %s", this->display_bitness_);
  //ESP_LOGCONFIG(TAG, "  Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));

  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  //LOG_PIN("  CS Pin: ", this->cs_);
  //LOG_PIN("  DC Pin: ", this->dc_pin_);

  ESP_LOGCONFIG(TAG, "  Swap_xy: %s", YESNO(this->swap_xy_));
  ESP_LOGCONFIG(TAG, "  Mirror_x: %s", YESNO(this->mirror_x_));
  ESP_LOGCONFIG(TAG, "  Mirror_y: %s", YESNO(this->mirror_y_));

  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "  => Failed to init Memory: YES!");
  }
  LOG_UPDATE_INTERVAL(this);
}

void DisplayDriver::fill(Color color) {
  this->setup_buffer();
  uint8_t bytes_per_pixel = (uint8_t) this->buffer_bitness_ >> 16;
  size_t count = this->get_buffer_length_();
  size_t loop = 0;
  uint32_t new_color = ColorUtil::from_color(color, this->buffer_bitness_, this->palette_);

  while (count--) {
    memcpy(this->buffer_ + loop, (const void *) &new_color, bytes_per_pixel);
    loop += bytes_per_pixel;
  }
}

void HOT DisplayDriver::buffer_pixel_at(int x, int y, Color color) {
  if (x >= this->width_ || x < 0 || y >= this->height_ || y < 0) {
    return;
  }
  uint32_t pos = (y * this->width_) + x;
  uint16_t new_color = ColorUtil::from_color(color, this->buffer_bitness_, this->palette_);
  bool updated = false;
  this->setup_buffer();

  switch (this->buffer_bitness_) {
    case ColorBitness::COLOR_BITNESS_888:
    case ColorBitness::COLOR_BITNESS_666:
      pos = pos * 3;
      if ((this->buffer_[pos + 0] != (uint8_t) (new_color >> 16)) &&
          (this->buffer_[pos + 1] != (uint8_t) (new_color >> 8)) &&
          (this->buffer_[pos + 2] != (uint8_t) (new_color >> 0))) {
        this->buffer_[pos + 0] = (uint8_t) (new_color >> 16);
        this->buffer_[pos + 1] = (uint8_t) (new_color >> 8);
        this->buffer_[pos + 2] = (uint8_t) (new_color >> 0);
        updated = true;
      }
      break;
    case ColorBitness::COLOR_BITNESS_565:
      pos = pos * 2;
      if ((this->buffer_[pos + 0] != (uint8_t) (new_color >> 8) &&
           (this->buffer_[pos + 1] != (uint8_t) (new_color >> 0)))) {
        this->buffer_[pos + 0] = (uint8_t) (new_color >> 8);
        this->buffer_[pos + 1] = (uint8_t) (new_color >> 0);
        updated = true;
      }
      break;
    case ColorBitness::COLOR_BITNESS_332:
    case ColorBitness::COLOR_BITNESS_8I:
    case ColorBitness::COLOR_BITNESS_8:
      if (this->buffer_[pos] != new_color) {
        this->buffer_[pos] = new_color;
        updated = true;
      }
    default:

      break;
  }

  if (updated) {
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

void DisplayDriver::display_buffer() {
  uint8_t transfer_buffer[TRANSFER_BUFFER_SIZE];
  // check if something was displayed
  if ((this->x_high_ < this->x_low_) || (this->y_high_ < this->y_low_)) {
    return;
  }

  // we will only update the changed rows to the display

  size_t x_low = this->x_low_;
  size_t y_low = this->y_low_;
  size_t x_high = this->x_high_;
  size_t y_high = this->y_high_;
  size_t d_width = this->width_;
  uint8_t bytes_per_pixel = (uint8_t) this->buffer_bitness_ >> 16;
  uint8_t devider = this->buffer_bitness_ >> 24;

  size_t mhz = 40; ///// this->data_rate_ / 1000000;
  // estimate time for a single write
//  size_t sw_time = this->width_ * h * 16 / mhz + this->width_ * h * 2 / SPI_MAX_BLOCK_SIZE * SPI_SETUP_US * 2;
  // estimate time for multiple writes
  // size_t mw_time = (w * h * 16) / mhz + w * h * 2 / TRANSFER_BUFFER_SIZE * SPI_SETUP_US;

  auto now = millis();

  this->bus_->start();
  if (!this->set_addr_window(x_low, y_low, x_high, y_high)) {
    x_low = y_low = 0;
    x_high = this->width_;
    y_high = this->height_;
  }
  if (devider > 1) {
    x_low = (x_low - (x_low % devider)) / devider;
    x_high = (x_high + (devider - (x_high % devider))) / devider;
    d_width = (d_width + (devider - (d_width % devider))) / devider;
  }
  size_t w = x_high - x_low + 1;
  size_t h = y_high - y_low + 1;

  size_t rem = h * w;  // remaining number of pixels to write
  size_t idx = 0;      // index into transfer_buffer
  size_t pixel = 0;    // pixel number offset
  size_t pos = y_low * d_width + x_low;

  ESP_LOGV(TAG,
           "Start display(xlow:%d, ylow:%d, xhigh:%d, yhigh:%d, width:%d, "
           "height:%d, buffer=%d, display=%d)",  // , sw_time=%dus, mw_time=%dus
           this->x_low_, this->y_low_, this->x_high_, this->y_high_, w, h, this->buffer_bitness_,
           this->display_bitness_);  //, sw_time, mw_time

  if (this->buffer_bitness_ == this->display_bitness_) {  //  && sw_time < mw_time
    // 16 bit mode maps directly to display format
    ESP_LOGV(TAG, "Doing single write of %d bytes", this->width_ * h * 2);
    for (uint16_t row = y_low; row < y_high; row++) {
      this->bus_->send_data(this->buffer_ + row * d_width * bytes_per_pixel, w * bytes_per_pixel);
    }
  } else {
    ESP_LOGV(TAG, "Doing multiple write");

    while (rem-- != 0) {
      uint32_t color_val;
      Color color;
      switch (this->buffer_bitness_) {
        case ColorBitness::COLOR_BITNESS_332:
          color = (ColorUtil::rgb332_to_color(this->buffer_[pos++]));
          break;
        case ColorBitness::COLOR_BITNESS_8I:
          color =  ColorUtil::index8_to_color_palette888(this->buffer_[pos++], this->palette_);
          break;
        default:  // case COLOR_BITNESS_16:
          color_val = (buffer_[pos * 2] << 8) + buffer_[pos * 2 + 1];
          pos++;
          break;
      }
      if (this->get_18bit_mode()) {
        transfer_buffer[idx++] = (uint8_t) ((color_val & 0xF800) >> 8);  // Blue
        transfer_buffer[idx++] = (uint8_t) ((color_val & 0x7E0) >> 3);   // Green
        transfer_buffer[idx++] = (uint8_t) (color_val << 3);             // Red
      } else {
        put16_be(transfer_buffer + idx, color_val);
        idx += 2;
      }
      if (idx == TRANSFER_BUFFER_SIZE) {
        this->bus_->send_data(transfer_buffer, idx);
        idx = 0;
        App.feed_wdt();
      }
      // end of line? Skip to the next.
      if (++pixel == w) {
        pixel = 0;
        pos += this->width_ - w;
      }
    }
    // flush any balance.
    if (idx != 0) {
      this->bus_->send_data(transfer_buffer, idx);
    }
  }
  this->bus_->end();
  ESP_LOGV(TAG, "Data write took %dms", (unsigned) (millis() - now));
  // invalidate watermarks
  this->x_low_ = this->width_;
  this->y_low_ = this->height_;
  this->x_high_ = 0;
  this->y_high_ = 0;
}

// note that this bypasses the buffer and writes directly to the display.
void DisplayDriver::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr,
                                   ColorOrder order, ColorBitness bitness, bool big_endian,
                                   int x_offset, int y_offset, int x_pad) {
  if (w <= 0 || h <= 0)
    return;
  // if color mapping or software rotation is required, hand this off to the parent implementation. This will
  // do color conversion pixel-by-pixel into the buffer and draw it later. If this is happening the user has not
  // configured the renderer well.
  if (this->rotation_ != DISPLAY_ROTATION_0_DEGREES || bitness != COLOR_BITNESS_565 || !big_endian ||
      this->get_18bit_mode()) {
    return draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset, y_offset,
                                            x_pad);
  }
  this->bus_->start();
  this->set_addr_window(x_start, y_start, x_start + w - 1, y_start + h - 1);
  // x_ and y_offset are offsets into the source buffer, unrelated to our own offsets into the display.
  if (x_offset == 0 && x_pad == 0 && y_offset == 0) {
    // we could deal here with a non-zero y_offset, but if x_offset is zero, y_offset probably will be so don't bother
    this->bus_->send_data(ptr, w * h * 2);
  } else {
    auto stride = x_offset + w + x_pad;
    for (size_t y = 0; y != h; y++) {
      this->bus_->send_data(ptr + (y + y_offset) * stride + x_offset, w * 2);
    }
  }
  this->bus_->end();
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

}  // namespace display_driver
}  // namespace esphome
