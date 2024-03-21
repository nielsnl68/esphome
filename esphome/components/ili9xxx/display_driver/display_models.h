#pragma once
#include "esphome/core/helpers.h"
#include "esphome/components/display_driver/display_driver.h"
#include "../display_defines.h"
#include "../display_init.h"

#include <cinttypes>

namespace esphome {
namespace ili9xxX {

class Ili9xxxDriver : public DisplayDriver {
 protected:
  void preset_init_values() override {
    uint8_t cmd, num_args, bits;
    const uint8_t *addr = this->init_sequence_;

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
  }

  void finalize_init_values() override {
    uint8_t pix = this->display_bitness_.pixel_mode == ColorBitness::COLOR_BITS_666 ? 0x66 : 0x55;
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
    delay(50);
    this->bus_->send_command(ILI9XXX_DISPON);
    delay(150);
  }
  bool set_addr_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) override {
    this->bus_->begin_commands();

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

    this->bus_->end_commands();

    return true;
  }
};

// clang-format on

//-----------   M5Stack display --------------
class Display_M5Stack : public Ili9xxxDriver {
 public:
  Display_M5Stack() {
    DisplayDriver(INITCMD_M5STACK, 320, 240, true);
  }
};

//-----------   M5CORE display --------------
class Display_M5CORE : public Ili9xxxDriver {
 public:
  Display_M5CORE() { DisplayDriver(INITCMD_M5CORE, 320, 240, true); }
};

//-----------   ST7789V display --------------
class Display_ST7789V : public Ili9xxxDriver {
 public:
  Display_ST7789V()  {DisplayDriver(INITCMD_ST7789V, 240, 320, false);}
};

//-----------   Display__24_TFT display --------------
class Display_ILI9341 : public Ili9xxxDriver {
 public:
  Display_ILI9341() {DisplayDriver(INITCMD_ILI9341, 240, 320, false);}
};

//-----------   Display__24_TFT rotated display --------------
class Display_ILI9342 : public Ili9xxxDriver {
 public:
  Display_ILI9342() { DisplayDriver(INITCMD_ILI9341, 320, 240, false); }
};

//-----------   Display__??_TFT rotated display --------------
class Display_ILI9481 : public Ili9xxxDriver {
 public:
  Display_ILI9481() { DisplayDriver(INITCMD_ILI9481, 480, 320, false); }
};

//-----------   ILI9481 in 18 bit mode --------------
class Display_ILI948118 : public Ili9xxxDriver {
 public:
  Display_ILI948118() { DisplayDriver(INITCMD_ILI9481_18, 320, 480, true); }
};

//-----------   Display__35_TFT rotated display --------------
class Display_ILI9486 : public Ili9xxxDriver {
 public:
  Display_ILI9486() {DisplayDriver(INITCMD_ILI9486, 480, 320, false); }
};

//-----------   Display__35_TFT rotated display --------------
class Display_ILI9488 : public Ili9xxxDriver {
 public:
  Display_ILI9488() { DisplayDriver(INITCMD_ILI9488, 480, 320, true); }
};

//-----------   Display__35_TFT origin colors rotated display --------------
class Display_ILI9488A : public Ili9xxxDriver {
 public:
  Display_ILI9488A() { DisplayDriver(INITCMD_ILI9488_A, 480, 320, true); }
};

//-----------   Display__35_TFT rotated display --------------
class Display_ST7796 : public Ili9xxxDriver {
 public:
  Display_ST7796() { DisplayDriver(INITCMD_ST7796, 320, 480, false); }
};

class Display_S3Box : public Ili9xxxDriver {
 public:
  Display_S3Box() { DisplayDriver(INITCMD_S3BOX, 320, 240, false); }
};

class Display_S3BoxLite : public Ili9xxxDriver {
 public:
  Display_S3BoxLite() {DisplayDriver(INITCMD_S3BOXLITE, 320, 240, true);}
};

/**/

}  // namespace ili9xxx
}  // namespace esphome
