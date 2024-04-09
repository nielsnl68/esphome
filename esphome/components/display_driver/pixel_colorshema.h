#pragma once
#include "esphome/core/color.h"
#include "esphome/core/log.h"
#include "esphome/components/display/display_color_utils.h"
namespace esphome {
namespace display {

static const char *const TAGC = "ColorSchema";
static const uint8_t NumOfModes = 11;

static const char *ColorModeStr[NumOfModes] = {
    "Unknown",        "1bit",           "2bit",           "4bit",           "8bit",
    "8bit 332 mode",  "16bit 565 mode", "18bit 666 mode", "24bit 888 mode", "16bit 4444 mode",
    "32bit 8888 mode"};

static const uint8_t ColorConfig[NumOfModes] = {0x00, 0x81, 0x41, 0x21, 0x11, 0x11, 0x12, 0x13, 0x13, 0x12, 0x14};

static const uint16_t ColorBits[2][NumOfModes] = {
    {0x0000, 0x0001, 0x0002, 0x0004, 0x0008, 0x3320, 0x5650, 0x6660, 0x8880, 0x4444, 0x8888},
    {0x0000, 0x0001, 0x0002, 0x0004, 0x0008, 0x3320, 0x8880, 0x8880, 0x8880, 0x4444, 0x8888},
};

static const char *ColorOrderStr[3] = {"RGB", "BGR", "GRB"};

template<int In, int Out> inline ALWAYS_INLINE uint8_t shift_bits(uint8_t src) {
  if (!In || !Out) {
    return 0;
  } else if (In == 1 && In < Out) {
    return -src >> (8 - Out);
  } else if (In < Out) {
    return src * ((255 / ((1 << In) - 1)) >> (8 - Out));
  } else if (In > Out) {
    return src >> (In - Out);
  } else {
    return src;
  }
}

class ColorSchema {
 public:
  void setSchema(ColorBitness mode, ColorOrder color_order = ColorOrder::COLOR_ORDER_RGB, bool byte_aligned = false,
                 bool little_endian = false) {
    this->pixel_mode = mode;
    this->color_order = color_order;
    this->byte_aligned = byte_aligned;
    this->little_endian = little_endian;
    this->refresh_config_();
  }

  void setSchema(const ColorSchema &rhs) {  // NOLINT
    mode = rhs.mode;
    refresh_config_();
  }

  void setTransparant(bool transparant = false) { this->transparant = transparant; }
  void set_palette(const uint8_t *palette) { this->palette_ = palette; }

  inline ALWAYS_INLINE int array_stride(int width) {
    return (width + this->pixels_per_byte - 1) / this->pixels_per_byte;
  }
  inline ALWAYS_INLINE int array_stride(int width, int height) { return array_stride(width) * height; }
  inline ALWAYS_INLINE int bytes_stride(int width) { return array_stride(width) * this->bytes_per_pixel; }
  inline ALWAYS_INLINE int bytes_stride(int width, int height) {
    return array_stride(width, height) * this->bytes_per_pixel;
  }

  bool is_on(int pixel = 0) const { return raw_color.is_on(); }
  bool is_transparent(int pixel = 0) const { return false; }
  void rgb_encode(uint8_t *pixel, int position = 0) {
    this->raw[0] = r;
    this->raw[1] = g;
    this->raw[2] = b;
    this->raw[3] = w;
  //  this->raw = little_endian ? convert_little_endian(this->raw) : convert_big_endian(this->raw);
  }

  virtual void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &w, int pixel = 0) const {
    uint32_t value; //= (little_endian ==1) ? convert_little_endian(this->raw) : convert_big_endian(this->raw);
    r = this->raw[0]; // (value >> (part1_pos)) & ((1 << part1) - 1);
    g = this->raw[1]; // (value >> (part2_pos)) & ((1 << part2) - 1);
    b = this->raw[2]; // (value >> (part2_pos)) & ((1 << part3) - 1);
    w = 0;
  }

  inline ALWAYS_INLINE void info(const char *prefix = "ColorSchema:", const char *tag = TAGC) {
    ESP_LOGCONFIG(tag, prefix);
    ESP_LOGCONFIG(tag, "    Pixel mode   : %s", ColorModeStr[this->pixel_mode]);
    ESP_LOGCONFIG(tag, "    Color Order  : %s", ColorOrderStr[this->color_order]);

    ESP_LOGCONFIG(tag, "    Little Ending: %s", YESNO(this->little_endian));
    ESP_LOGCONFIG(tag, "    Byte Aligned : %s", YESNO(this->byte_aligned));
    ESP_LOGCONFIG("", "");
    ESP_LOGCONFIG(tag, "    Transparant : %s", YESNO(this->transparant));
    ESP_LOGCONFIG("", "");

    if (bytes_per_pixel > 1) {
      ESP_LOGCONFIG(tag, "    Bytes p Pixel: %d bytes", this->bytes_per_pixel);
    } else if (pixels_per_byte == 1) {
      ESP_LOGCONFIG(tag, "    Bytes p Pixel: %d byte", this->bytes_per_pixel);
    } else {
      ESP_LOGCONFIG(tag, "    Pixel p Byte : %d bits", this->bytes_per_pixel);
    }
  }

 protected:
  void refresh_config_() {
    if (this->pixel_mode < 5) {
      this->byte_aligned = 0;
    }
    if (this->pixel_mode < 6) {
      this->little_endian = 0;
    }
    this->cfg = ColorConfig[pixel_mode];
    this->parts = ColorBits[byte_aligned][pixel_mode];
    if (this->pixels_per_byte >= 1) {
      this->mask = (1 << this->pixels_per_byte) - 1;
    }
    if (mode >= ColorBitness::COLOR_BITNESS_332) {
      uint8_t swap;
      switch (color_order) {
        case ColorOrder::COLOR_ORDER_BGR:
          swap = this->part1;
          this->part1 = this->part3;
          this->part3 = swap;

          part3_pos = 0;
          part2_pos = part3_pos + part1;
          part1_pos = part2_pos + part2;
          part4_pos = part1_pos + part3;
          break;
        case ColorOrder::COLOR_ORDER_GRB:
          swap = this->part1;
          this->part1 = this->part2;
          this->part2 = swap;

          part2_pos = 0;
          part1_pos = part2_pos + part1;
          part3_pos = part1_pos + part2;
          part4_pos = part3_pos + part3;
          break;
        default:
          part1_pos = 0;
          part2_pos = part1_pos + part1;
          part3_pos = part2_pos + part2;
          part4_pos = part3_pos + part3;
      }
    }
  }


  union {
    struct {
      uint16_t pixel_mode : 4;
      uint16_t color_order : 2;
      uint16_t byte_aligned : 1;
      uint16_t little_endian : 1;
      uint16_t transparant : 1;
    };
    uint16_t mode;
  };

  union {
    struct {
      uint8_t pixels_per_byte : 4;
      uint8_t bytes_per_pixel : 4;
      uint8_t mask;
    };
    struct {
      uint8_t cfg;
    };
  };

  union {
    struct {
      uint16_t part1 : 4;
      uint16_t part2 : 4;
      uint16_t part3 : 4;
      uint16_t part4 : 4;
    };
    uint16_t parts;
  };
  uint8_t part1_pos = 0;
  uint8_t part2_pos = 0;
  uint8_t part3_pos = 0;
  uint8_t part4_pos = 0;

  Color raw_color;
  const Color *palette_;
};


template<bool BigEndian> struct PixelRGB565RGB_Endiness : ColorSchema<ColorBitness::COLOR_BITNESS_565> {
  uint16_t raw_16;

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const { return true; }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const { return false; }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t w, int pixel = 0) {
    uint16_t value = r << (6 + 5) | g << (5) | b;
    this->raw_16 = BigEndian ? convert_big_endian(value) : convert_little_endian(value);
  }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &w, int pixel = 0) const {
    r = value >> (6 + 5);
    g = (value >> (5)) & ((1 << 6) - 1);
    b = (value >> 0) & ((1 << 5) - 1);
    w = 0;
  }
} PACKED;

typedef PixelRGB565_Endiness<ColorBitness::RGB565_LE, false> PixelRGB565_LE;
typedef PixelRGB565_Endiness<ColorBitness::RGB565, true> PixelRGB565_BE;

struct PixelRGB888 : ColorSchema<ColorBitness::RGB888, 8, 8, 8, 0, 0, 3> {
  uint8_t raw_8[3];

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const { return true; }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const { return false; }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t w, int pixel = 0) {
    this->raw_8[0] = r;
    this->raw_8[1] = g;
    this->raw_8[2] = b;
  }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &w, int pixel = 0) const {
    r = this->raw_8[0];
    g = this->raw_8[1];
    b = this->raw_8[2];
    w = 0;
  }
} PACKED;

struct PixelRGBA4444 : ColorSchema {
  uint8_t raw_8[2];

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const { return true; }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const { return (this->raw_8[1] & 0xF) < 0x8; }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t w, int pixel = 0) {
    this->raw_8[0] = r << 4 | g;
    this->raw_8[1] = b << 4 | w;
  }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &w, int pixel = 0) const {
    r = this->raw_8[0] >> 4;
    g = this->raw_8[0] & 0xF;
    b = this->raw_8[1] >> 4;
    w = this->raw_8[1] & 0xF;
  }
} PACKED;

struct PixelRGBA8888 : ColorSchema {
  uint8_t raw_8[4];

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const { return true; }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const { return this->raw_8[3] < 0x80; }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t w, int pixel = 0) {
    this->raw_8[0] = r;
    this->raw_8[1] = g;
    this->raw_8[2] = b;
    this->raw_8[3] = a;
  }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &w, int pixel = 0) const {
    r = this->raw_8[0];
    g = this->raw_8[1];
    b = this->raw_8[2];
    a = this->raw_8[3];
    w = 0;
  }
} PACKED;

struct PixelW1 : ColorSchema<ColorBitness::W1, 0, 0, 0, 0, 1, 1, 8, true> {
  uint8_t raw_8;

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const { return (this->raw_8 & (1 << (7 - pixel))) ? true : false; }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const { return false; }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t w, int pixel = 0) {
    const int mask = 1 << (7 - pixel);
    this->raw_8 &= ~mask;
    if (w)
      this->raw_8 |= mask;
  }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &w, int pixel = 0) const {
    r = 0;
    g = 0;
    b = 0;
    a = 0;
    w = (this->raw_8 & (1 << (7 - pixel))) ? 1 : 0;
  }
} PACKED;

struct PixelW2 : ColorSchema<ColorBitness::W2, 0, 0, 0, 0, 2, 1, 4> {
  uint8_t raw_8;

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const { return true; }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const { return false; }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t w, int pixel = 0) {
    this->raw_8 &= ~(3 << (pixel * 2));
    this->raw_8 |= w << (pixel * 2);
  }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &w, int pixel = 0) const {
    r = 0;
    g = 0;
    b = 0;
    a = 0;
    w = (this->raw_8 >> (pixel * 2)) & 0x3;
  }
} PACKED;

struct PixelW4 : ColorSchema<ColorBitness::W4, 0, 0, 0, 0, 4, 1, 2> {
  uint8_t raw_8;

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const { return true; }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const { return false; }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t w, int pixel = 0) {
    if (pixel) {
      this->raw_8 &= ~0xF0;
      this->raw_8 |= w << 4;
    } else {
      this->raw_8 &= ~0x0F;
      this->raw_8 |= w;
    }
  }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &w, int pixel = 0) const {
    r = 0;
    g = 0;
    b = 0;
    a = 0;
    if (pixel)
      w = this->raw_8 >> 4;
    else
      w = this->raw_8 & 0xF;
  }
} PACKED;

template<ColorBitness Format, bool Key> struct PixelW8_Keyed : ColorSchema<Format, 0, 0, 0, 0, 8, 1> {
  uint8_t raw_8;

  inline ALWAYS_INLINE bool is_on(int pixel = 0) const { return true; }
  inline ALWAYS_INLINE bool is_transparent(int pixel = 0) const { return Key ? this->raw_8 == 1 : false; }
  inline ALWAYS_INLINE void encode(uint8_t r, uint8_t g, uint8_t b, uint8_t w, int pixel = 0) { this->raw_8 = w; }
  inline ALWAYS_INLINE void decode(uint8_t &r, uint8_t &g, uint8_t &b, uint8_t &w, int pixel = 0) const {
    r = 0;
    g = 0;
    b = 0;
    a = 0;
    w = this->raw_8;
  }
} PACKED;

typedef PixelW8_Keyed<ColorBitness::COLOR_BITNESS_8BITS, false> PixelW8;
typedef PixelW8_Keyed<ColorBitness::COLOR_BITNESS_8BITS, true> PixelW8_KEY;

}  // namespace display
}  // namespace esphome
