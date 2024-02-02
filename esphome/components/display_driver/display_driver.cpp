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
  this->preset_init_values();
}

void DisplayDriver::preset_init_values() {
  uint8_t cmd, num_args, bits;
  const uint8_t *addr = this->init_sequence_;

  this->display_bitness_.info("on preset_init_values.");
  while ((cmd = *addr++) != 0) {
    num_args = *addr++ & 0x7F;
    bits = *addr;
    switch (cmd) {
      case ILI9XXX_MADCTL: {
        this->swap_xy_ = (bits & MADCTL_MV) != 0;
        this->mirror_x_ = (bits & MADCTL_MX) != 0;
        this->mirror_y_ = (bits & MADCTL_MY) != 0;
        this->display_bitness_.color_order = (bits & MADCTL_BGR) ? COLOR_ORDER_BGR : COLOR_ORDER_RGB;
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
  this->display_bitness_.info("after preset_init_values.");
}

void DisplayDriver::setup() {
  ESP_LOGD(TAG, "Setting up the display_driver");
  if (this->bus_ == nullptr) {
    ESP_LOGE(TAG, "Databus interface not set.");
    this->mark_failed();
    return;
  }
  this->setup_pins();
  this->bus_->setup();

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
void DisplayDriver::finalize_init_values() {
  uint8_t pix = this->display_bitness_.bits_per_pixel == ColorBitness::COLOR_BITS_666 ? 0x66 : 0x55;
  uint8_t mad = this->display_bitness_.color_order == COLOR_ORDER_BGR ? MADCTL_BGR : MADCTL_RGB;
  if (this->swap_xy_)
    mad |= MADCTL_MV;
  if (this->mirror_x_)
    mad |= MADCTL_MX;
  if (this->mirror_y_)
    mad |= MADCTL_MY;

  this->bus_->send_command(this->pre_invertcolors_ ? ILI9XXX_INVON : ILI9XXX_INVOFF);
  this->bus_->send_command(ILI9XXX_MADCTL, &mad, 1);
  this->bus_->send_command(ILI9XXX_PIXFMT, &pix, 1);
  this->display_bitness_.color_order = MADCTL_RGB;
  this->display_bitness_.little_endian = true;
}

void DisplayDriver::setup_lcd() {
  uint8_t cmd, x, num_args;
  const uint8_t *addr = this->init_sequence_;

  this->bus_->start();

  while ((cmd = *addr++) > 0) {
    x = *addr++;
    num_args = x & 0x7F;
    if (cmd == ILI9XXX_DISPON) {
      this->finalize_init_values();
    }
    this->bus_->send_command(cmd, addr, num_args);
    addr += num_args;
    if (x & 0x80) {
      this->bus_->end();
      delay(150);  // NOLINT
      this->bus_->start();
    }
  }

  this->bus_->end();

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
  this->buffer_bitness_.raw_16 = this->display_bitness_.raw_16;
  switch (this->buffer_bitness_.bytes_per_pixel) {
    case 1:  // 2 pixels per byte
      this->buffer_ = allocator.allocate(this->get_buffer_size_());
      if (this->buffer_ != nullptr) {
        return true;
        ;
      }
      ESP_LOGW(TAG, "Not enough (ps)ram memory for 2 Pixels per byte.");
      this->buffer_bitness_ = ColorBitness::COLOR_BITNESS_565;
    case 2:  // 4 pixels per byte
      this->buffer_ = allocator.allocate(this->get_buffer_size_());
      if (this->buffer_ != nullptr) {
        return true;
        ;
      }
      ESP_LOGW(TAG, "Not enough (ps)ram memory for 4 Pixels per byte.");
      this->buffer_bitness_ = ColorBitness::COLOR_BITNESS_565;
    case 3:  // 8 pixels per byte
      this->buffer_ = allocator.allocate(this->get_buffer_size_());
      if (this->buffer_ == nullptr) {
        ESP_LOGE(TAG, "Not enough (ps)ram memory for single bit colors.");
        this->mark_failed();
        return false;
      }
      return true;
    default:
      switch (this->buffer_bitness_.bytes_per_pixel) {
        case 3:
          this->buffer_ = allocator.allocate(this->get_buffer_size_());
          if (this->buffer_ != nullptr) {
            return true;
            ;
          }
          ESP_LOGW(TAG, "Not enough (ps)ram memory for 3byte colors.");
          this->buffer_bitness_ = ColorBitness::COLOR_BITNESS_565;
          this->buffer_bitness_.right_aligned = false;
        case 2:
          this->buffer_ = allocator.allocate(this->get_buffer_size_());
          if (this->buffer_ != nullptr) {
            return true;
          }
          ESP_LOGW(TAG, "Not enough (ps)ram memory for 2byte colors.");
          if (this->palette_ = nullptr) {
            this->buffer_bitness_ = ColorBitness::COLOR_BITNESS_332;
          } else {
            this->buffer_bitness_ = ColorBitness::COLOR_BITNESS_8I;
          }
        default:
          this->buffer_ = allocator.allocate(this->get_buffer_size_());
          if (this->buffer_ == nullptr) {
            ESP_LOGE(TAG, "Not enough (ps)ram memory for single byte colors.");
            this->mark_failed();
            return false;
          }
          return true;
      }
  }
}
void DisplayDriver::dump_config() {
  LOG_DISPLAY("", "Display Driver:", this);
  this->padding_.info("display window:");
  if (this->buffer_ != nullptr) {
    this->buffer_bitness_.info("  Buffer Color Dept:");
  }
  this->display_bitness_.info("  Display Color Dept:");
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
  ESP_LOGV(TAG, "fill");
  if (!this->setup_buffer())
    return;
  uint8_t bytes_per_pixel = this->buffer_bitness_.bytes_per_pixel;
  const uint8_t *addr = this->buffer_;
  const uint8_t *end_addr = addr + this->get_buffer_size_();
  uint32_t new_color = ColorUtil::from_color(color, this->buffer_bitness_, this->palette_);

  while (true) {
    memcpy((void *) addr, (const void *) &new_color, bytes_per_pixel);
    addr += bytes_per_pixel;
    if (addr == end_addr)
      break;
  }
  this->view_port_ = Rect(0, 0, this->width_, this->height_);
}

// Tell the display controller where we want to draw pixels.
// when called, the SPI should have already been enabled, only the D/C pin will be toggled here.
// return true when the window can be set, orherwise return false.

bool DisplayDriver::set_addr_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  // this->bus_->start();

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

  // this->bus_->end();

  return true;
}

void DisplayDriver::display_buffer() {
  // check if something was displayed
  if ((!this->view_port_.is_set()) || (this->buffer_ == nullptr)) {
    return;
  }
  if (this->send_buffer(this->buffer_, this->view_port_.x, this->view_port_.y, this->view_port_.x, this->view_port_.y,
                        this->view_port_.w, this->view_port_.h, this->buffer_bitness_,
                        this->width_ - this->view_port_.x2())) {
    // invalidate watermarks

    this->view_port_ = Rect();
  }
}

bool DisplayDriver::send_buffer(const uint8_t *data, int x_display, int y_display, int x_in_data, int y_in_data,
                                int width, int height, ColorBitness bitness, int x_pad) {
  ESP_LOGV(TAG,
           "Start display(display:[%d, %d], in_data:[%d + %d, %d], dimension:[%d, %d]",
           x_display, y_display, x_in_data, x_pad, y_in_data, width, height);

  size_t data_width = width + x_pad;
  uint8_t bytes_per_pixel = bitness.bytes_per_pixel;
  uint8_t devider = bitness.pixel_devider();

  auto now = millis();

  // this->bus_->start();
  
  if (!this->set_addr_window(x_display, y_display, x_display + width-1, y_display + height-1)) {
    if (data == this->buffer_) {
      x_pad = 0;
      x_in_data = 0;
      y_in_data = 0;
      width = this->width_;
      height = this->height_;
    } else {
      this->bus_->end();
      return false;
    }
  }

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
    // 16 bit mode maps directly to display format
    ESP_LOGV(TAG, "Copy buffer to display vw:%d dw:%d h:%d", view_width, view_width, height);
    for (uint16_t row = 0; row < height; row++) {
     // ESP_LOGV(TAG, "send from : 0x%x Row:%d", addr, row);
      this->bus_->send_data(addr, view_width);
      addr += data_width;
      App.feed_wdt();
    }
  } else {
    ESP_LOGV(TAG, "confird buffer before sending to display");
    if (this->data_batch_ == nullptr) {
      ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
      this->data_batch_ = allocator.allocate(TRANSFER_BUFFER_SIZE);
    }

    uint8_t display_bytes_per_pixel = this->display_bitness_.bytes_per_pixel;
    size_t col = 0, row = 0;  // remaining number of pixels to write
    size_t bytes_send = 0;    // index into transfer_buffer
    uint32_t buf_color = 0;
    uint32_t disp_color = 0;
    uint8_t disp_part = 0;

    while (true) {
      memcpy((void *) &buf_color, (const void *) addr, bytes_per_pixel);
      //  ESP_LOGV(TAG, "display (col:%d, row:%d,  start_pos:%d, bytes_send:%d ", col, row, start_pos, bytes_send);
      for (auto buf_part = 0; buf_part < devider; buf_part++) {
        Color color = ColorUtil::to_color(buf_color, bitness, this->palette_, buf_part);
        disp_color = ColorUtil::from_color(color, this->display_bitness_, this->palette_, disp_color, disp_part);
        if (++disp_part == this->display_bitness_.pixel_devider()) {
          memcpy((void *) (this->data_batch_ + bytes_send), (const void *) &disp_color, display_bytes_per_pixel);
          disp_part = 0;
          disp_color = 0;
          bytes_send += display_bytes_per_pixel;
          if (bytes_send >= TRANSFER_BUFFER_SIZE - display_bytes_per_pixel) {
            this->bus_->send_data(this->data_batch_, bytes_send);
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
      this->bus_->send_data(this->data_batch_, bytes_send);
    }
  }

  // this->bus_->end();

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
    if (this->send_buffer(data, x_display, y_display, x_in_data, y_in_data, width, height, bitness, x_pad)) {
      return;
    }
  }
  Display::draw_pixels_at(data, x_display, y_display, x_in_data, y_in_data, width, height, bitness, x_pad);
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

// ======================== displayInterface

void displayInterface::send_command(uint8_t command, const uint8_t *data, uint8_t len, bool start) {
  this->start();
  this->command(command);  // Send the command byte

  if (len > 0) {
    this->data(data, len);
  }
  this->end();
}

void displayInterface::send_data(const uint8_t *data, uint8_t len, bool start) {
  this->start();
  this->data(data, len);
  this->end();
}

void displayInterface::command(uint8_t value) {
  this->start_command();
  this->send_byte(value);
  this->end_command();
}

void displayInterface::data(const uint8_t *value, uint16_t len) {
  this->start_data();
  if (len == 1) {
    this->send_byte(*value);
  } else {
    this->send_array(value, len);
  }
  this->end_data();
}

// ======================== SPI_Interface

void SPI_Interface::setup() {
  this->dc_pin_->setup();  // OUTPUT
  this->dc_pin_->digital_write(false);

  this->spi_setup();
}

void SPI_Interface::dump_config() {
  ESP_LOGCONFIG(TAG, "  Interface: %s", "SPI");
  LOG_PIN("    CS Pin: ", this->cs_);
  LOG_PIN("    DC Pin: ", this->dc_pin_);
  ESP_LOGCONFIG(TAG, "    Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));
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
    this->disable();
  }
}

void SPI_Interface::send_byte(uint8_t data) { this->write_byte(data); }
void SPI_Interface::send_array(const uint8_t *data, int16_t len) { this->write_array(data, len); };

// ======================== SPI16D_Interface

void SPI16D_Interface::dump_config() {
  ESP_LOGCONFIG(TAG, "  Interface: %s", "SPI16D");
  LOG_PIN("    CS Pin: ", this->cs_);
  LOG_PIN("    DC Pin: ", this->dc_pin_);
  ESP_LOGCONFIG(TAG, "    Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));
}

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
