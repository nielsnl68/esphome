#pragma once
#include "esphome/core/color.h"
#include "esphome/core/log.h"

namespace esphome {
namespace display {

static const char *const TAGC = "ColorUtil";
static const uint8_t NumOfModes = 11;

enum ColorMode : uint8_t {
  COLOR_MODE_UNKNOWN = 0,
  COLOR_MODE_1BIT = 1,
  COLOR_MODE_2BITS = 2,
  COLOR_MODE_4BITS = 3,
  COLOR_MODE_8BITS = 4,
  COLOR_MODE_332 = 5,
  COLOR_MODE_565 = 6,
  COLOR_MODE_666 = 7,
  COLOR_MODE_888 = 8,
  COLOR_MODE_4444 = 9,
  COLOR_MODE_8888 = 10,
};

static const char *ColorModeStr[NumOfModes] = {
    "Unknown",        "1bit",           "2bit",           "4bit",           "8bit",
    "8bit 332 mode",  "16bit 565 mode", "18bit 666 mode", "24bit 888 mode", "16bit 4444 mode",
    "32bit 8888 mode"};

static const uint8_t ColorConfig[NumOfModes] = {0x00, 0x81, 0x41, 0x21, 0x11, 0x01, 0x02, 0x03, 0x03, 0x02, 0x04};

static const uint32_t ColorBits[2][NumOfModes] = {
    {0x0000, 0x0001, 0x0002, 0x0004, 0x0008, 0x3320, 0x5650, 0x6660, 0x8880, 0x4444, 0x8888},
    {0x0000, 0x0001, 0x0002, 0x0004, 0x0008, 0x3320, 0x8880, 0x8880, 0x8880, 0x4444, 0x8888},
};

enum ColorOrder : uint8_t { COLOR_ORDER_RGB = 0, COLOR_ORDER_BGR = 1, COLOR_ORDER_GRB = 2 };
static const char *ColorOrderStr[3] = {"RGB", "BGR", "GRB"};

struct ColorSchema {
  static const uint16_t CB_UNKNOWN = 0x0000;

  union {
    struct {
      uint16_t pixel_mode : 4;
      uint16_t byte_aligned : 1;
      uint16_t little_endian : 1;
      uint16_t color_order : 2;
      uint16_t transparant : 1;
    };
    struct {
      uint8_t mode;
      uint8_t preset;
    };
    uint16_t raw = 0;
  };

  union {
    struct {
      uint8_t pixel_devider : 4;
      uint8_t bytes_per_pixel : 4;
    };
    uint8_t cfg = 0;
  } ;

  union {
    struct {
      uint8_t r;
      uint8_t g;
      uint8_t b;
      uint8_t w;
    };
    uint32_t raw = 0;
  } bits;

  inline ColorSchema() ALWAYS_INLINE {
    raw = 0;
    bits.raw = 0;
  }

  inline ColorSchema(uint8_t bt) ALWAYS_INLINE {
    raw = bt;
    bits.raw = ColorBits[byte_aligned][pixel_mode];
    this->cfg = ColorConfig[pixel_mode];
  }

  inline ColorSchema(ColorMode mode, bool aligned = false, bool little_endian = false,
                     uint8_t color_order = ColorOrder::COLOR_ORDER_RGB) ALWAYS_INLINE {
    this->pixel_mode = mode;
    this->byte_aligned = aligned;
    this->little_endian = little_endian;
    this->color_order = color_order;
    this->bits.raw = ColorBits[byte_aligned][pixel_mode];
    this->cfg = ColorConfig[pixel_mode];
  }

  inline void setByteAligned(bool little_endian = false) ALWAYS_INLINE {
    this->little_endian = little_endian;
    this->bits.raw = ColorBits[byte_aligned][pixel_mode];
  }

  ColorSchema operator=(const ColorSchema &rhs) {  // NOLINT
    this->raw = (this->raw & 0xF000) | (rhs.raw & 0x0FFF);
    return *this;
  }
  ColorSchema operator=(uint16_t colorbitmess) {  // NOLINT
    this->raw = (this->raw & 0xF000) | (colorbitmess & 0x0FFF);
    return *this;
  }

  inline bool operator==(const ColorSchema &rhs) {  // NOLINT
    return this->raw == rhs.raw;
  }
  inline bool operator==(uint16_t colorbitmess) {  // NOLINT
    return this->raw == colorbitmess;
  }
  inline bool operator!=(const ColorSchema &rhs) {  // NOLINT
    return this->raw != rhs.raw;
  }
  inline bool operator!=(uint32_t colorbitmess) {  // NOLINT
    return this->raw != colorbitmess;
  }

  inline uint8_t pixel_devider_() ALWAYS_INLINE { return (uint8_t) 1 << pixel_devider; }

  inline void info(const char *prefix = "Bitmess info:", const char *tag = TAGC) {
    ESP_LOGCONFIG(tag, prefix);
    ESP_LOGCONFIG(tag, "    Bitmess raw  : 0x%04x ", raw);
    ESP_LOGCONFIG(tag, "    Pixel mode   : %s", ColorModeStr[pixel_mode]);
    if (bytes_per_pixel > 1) {
      ESP_LOGCONFIG(tag, "    Bytes p Pixel: %d bytes", bytes_per_pixel);
    } else if (pixel_devider == 1) {
      ESP_LOGCONFIG(tag, "    Bytes p Pixel: %d byte", bytes_per_pixel);
    } else {
      ESP_LOGCONFIG(tag, "    Pixel p Byte : %d bits", bytes_per_pixel);
    }
    ESP_LOGCONFIG(tag, "    Color Order  : %s", ColorOrderStr[color_order]);

    ESP_LOGCONFIG(tag, "    Little Ending: %s", YESNO(little_endian));
    ESP_LOGCONFIG(tag, "    Byte Aligned : %s", YESNO(byte_aligned));
  }
};

class ColorUtil {
 public:
  inline static uint8_t esp_scale(uint8_t i, uint8_t scale, uint8_t max_value = 255) { return (max_value * i / scale); }
  inline static uint8_t esp_scale8(uint8_t i, uint8_t scale) { return (uint16_t(i) * (1 + uint16_t(scale))) / 256; }

  static Color convert(uint8_t *from_color, ColorSchema from_bitness, uint8_t from_position, uint8_t *to_color,
                       ColorSchema to_bitness, uint8_t to_position, const uint8_t *palette = nullptr) {
    Color color_return = Color::BLACK;
    uint32_t colorcode = 0;
    memcpy(from_color, &colorcode, from_bitness.bytes_per_pixel);

    if (from_bitness.little_endian && from_bitness.bytes_per_pixel > 1) {
      if (from_bitness.bytes_per_pixel == 2) {
        colorcode = ((colorcode & 0xff00) >> 8) | ((colorcode & 0x00ff) << 8);
      } else {
        colorcode = ((colorcode & 0xff0000) >> 16) + ((colorcode & 0x0000ff) << 16) + (colorcode & 0x00ff00);
      }
    }

    if (from_bitness.pixel_devider > 1) {
      from_position = (from_position % from_bitness.pixel_devider_());
      if (from_bitness.little_endian)
        from_position = 7 - from_position;
      uint8_t pixel_pos = from_position * (8 / from_bitness.pixel_devider());
      uint8_t mask = ((1 << (0x10 >> from_bitness.pixel_devider)) - 1) << pixel_pos;
      colorcode = (colorcode & mask) >> pixel_pos;
    }

    if (from_bitness.indexed) {
      color_return = ColorUtil::index8_to_color_palette888(colorcode, palette);
    } else if (from_bitness.grayscale) {
      color_return = color_return.fade_to_white(
          esp_scale(colorcode & (from_bitness.pixel_devider() - 1), from_bitness.pixel_devider()));
    } else {
    }
  }

  // depricated
  static Color to_color(uint32_t colorcode, ColorOrder color_order, ColorSchema bitness = ColorSchema::CB888,
                        bool right_bit_aligned = true) {
    bitness.byte_aligned = !right_bit_aligned;
    bitness.color_order = color_order;

    return ColorUtil::to_color(colorcode, bitness, nullptr);
  }

  static Color to_color(uint32_t colorcode, ColorSchema bitness, const uint8_t *palette = nullptr,
                        uint8_t position = 0) {
    Color color_return = Color::BLACK;
    if (bitness.pixel_devider > 1) {
      if (bitness.little_endian)
        position = 7 - (position % bitness.pixel_devider());
      uint8_t pixel_pos = (position % bitness.pixel_devider()) * (8 / bitness.pixel_devider());
      uint8_t mask = ((1 << (0x10 >> bitness.pixel_devider)) - 1) << pixel_pos;
      colorcode = (colorcode & mask) >> pixel_pos;
    }

    if (bitness.indexed) {
      return ColorUtil::index8_to_color_palette888(colorcode, palette);
    }
    if (bitness.grayscale) {
      return color_return.fade_to_white(esp_scale(colorcode & (bitness.pixel_devider() - 1), bitness.pixel_devider()));
    }

    uint8_t red_color, green_color, blue_color;
    ColorBits bits = get_color_bits(bitness);

    if (bitness.little_endian && bitness.bytes_per_pixel > 1) {
      if (bitness.bytes_per_pixel == 2) {
        colorcode = ((colorcode & 0xff00) >> 8) | ((colorcode & 0x00ff) << 8);
      } else {
        colorcode = ((colorcode & 0xff0000) >> 16) + ((colorcode & 0x0000ff) << 16) + (colorcode & 0x00ff00);
      }
    }

    if (bitness.byte_aligned) {
      red_color = esp_scale(((colorcode >> 16) & 0xFF), (1 << bits.red_bits) - 1);
      green_color = esp_scale(((colorcode >> 8) & 0xFF), ((1 << bits.green_bits) - 1));
      blue_color = esp_scale(((colorcode >> 0) & 0xFF), (1 << bits.blue_bits) - 1);
    } else {
      red_color = esp_scale(((colorcode >> (bits.green_bits + bits.blue_bits)) & ((1 << bits.red_bits) - 1)),
                            ((1 << bits.red_bits) - 1));
      green_color =
          esp_scale(((colorcode >> bits.blue_bits) & ((1 << bits.green_bits) - 1)), ((1 << bits.green_bits) - 1));
      blue_color = esp_scale(((colorcode >> 0) & ((1 << bits.blue_bits) - 1)), ((1 << bits.blue_bits) - 1));
    }

    switch (bitness.color_order) {
      case COLOR_ORDER_RGB:
        color_return.r = red_color;
        color_return.g = green_color;
        color_return.b = blue_color;
        break;
      case COLOR_ORDER_BGR:
        color_return.b = red_color;
        color_return.g = green_color;
        color_return.r = blue_color;
        break;
      case COLOR_ORDER_GRB:
        color_return.g = red_color;
        color_return.r = green_color;
        color_return.b = blue_color;
        break;
    }
    return color_return;
  }

  static uint32_t from_color(Color color, ColorSchema bitness, const uint8_t *palette = nullptr, uint32_t old_color = 0,
                             uint8_t position = 0) {
    if (bitness.indexed) {
      return ColorUtil::color_to_index8_palette888(color, palette);
    }

    if (bitness.grayscale) {
      uint8_t result = esp_scale(color.white, 255, bitness.pixel_devider() - 1);
      if (bitness.pixel_devider > 1) {
        if (bitness.little_endian)
          position = 7 - (position % bitness.pixel_devider());
        uint8_t pixel_pos = (position % bitness.pixel_devider()) * (8 / bitness.pixel_devider());
        uint8_t mask = ((1 << (0x10 >> bitness.pixel_devider)) - 1) << pixel_pos;
        result = (old_color & ~mask) | (result << pixel_pos);
      }
      return result;
    }

    ColorBits bits = get_color_bits(bitness);
    uint16_t red_color, green_color, blue_color;
    u_int32_t color_val = 0;
    red_color = esp_scale8(color.red, ((1 << bits.red_bits) - 1));
    green_color = esp_scale8(color.green, ((1 << bits.green_bits) - 1));
    blue_color = esp_scale8(color.blue, (1 << bits.blue_bits) - 1);
    if (bitness.byte_aligned) {
      switch (bitness.color_order) {
        case COLOR_ORDER_RGB:
          color_val = red_color << 16 | green_color << 8 | blue_color;
          break;
        case COLOR_ORDER_BGR:
          color_val = blue_color << 16 | green_color << 8 | red_color;
          break;
        case COLOR_ORDER_GRB:
          color_val = green_color << 16 | red_color << 8 | blue_color;
          break;
      }
    } else {
      switch (bitness.color_order) {
        case COLOR_ORDER_RGB:
          color_val = red_color << (bits.blue_bits + bits.green_bits) | green_color << bits.blue_bits | blue_color;
          break;
        case COLOR_ORDER_BGR:
          color_val = blue_color << (bits.red_bits + bits.green_bits) | green_color << bits.red_bits | red_color;
          break;
        case COLOR_ORDER_GRB:
          color_val = green_color << (bits.blue_bits + bits.red_bits) | red_color << bits.blue_bits | blue_color;
          break;
      }
    }
    if (!bitness.little_endian && bitness.bytes_per_pixel > 1) {
      if (bitness.bytes_per_pixel == 2) {
        color_val = ((color_val & 0xff00) >> 8) | ((color_val & 0x00ff) << 8);
      } else {
        color_val = ((color_val & 0xff0000) >> 16) | ((color_val & 0x0000ff) << 16) | (color_val & 0x00ff00);
      }
    }

    return color_val;
  }

  static inline Color rgb332_to_color(uint8_t rgb332_color) {
    return ColorUtil::to_color((uint32_t) rgb332_color, ColorSchema::CB332);
  }

  static uint8_t color_to_332(Color color, ColorOrder color_order = ColorOrder::COLOR_ORDER_RGB) {
    ColorSchema cb = ColorSchema::CB332;
    cb.color_order = color_order;
    return ColorUtil::from_color(color, cb, nullptr);
  }
  static uint16_t color_to_565(Color color, ColorOrder color_order = ColorOrder::COLOR_ORDER_RGB) {
    ColorSchema cb = ColorSchema::CB565;
    cb.color_order = color_order;
    return ColorUtil::from_color(color, cb, nullptr);
  }

  static uint32_t color_to_grayscale4(Color color) {
    uint32_t gs4 = esp_scale8(color.white, 15);
    return gs4;
  }

  /***
   * Converts a Color value to an 8bit index using a 24bit 888 palette.
   * Uses euclidiean distance to calculate the linear distance between
   * two points in an RGB cube, then iterates through the full palette
   * returning the closest match.
   * @param[in] color The target color.
   * @param[in] palette The 256*3 byte RGB palette.
   * @return The 8 bit index of the closest color (e.g. for display buffer).
   */

  static uint8_t color_to_index8_palette888(Color color, const uint8_t *palette) {
    uint8_t closest_index = 0;
    uint32_t minimum_dist2 = UINT32_MAX;  // Smallest distance^2 to the target
                                          // so far
    // int8_t(*plt)[][3] = palette;
    int16_t tgt_r = color.r;
    int16_t tgt_g = color.g;
    int16_t tgt_b = color.b;
    uint16_t x, y, z;
    // Loop through each row of the palette
    for (uint16_t i = 0; i < 256; i++) {
      // Get the pallet rgb color
      int16_t plt_r = (int16_t) palette[i * 3 + 0];
      int16_t plt_g = (int16_t) palette[i * 3 + 1];
      int16_t plt_b = (int16_t) palette[i * 3 + 2];
      // Calculate euclidean distance (linear distance in rgb cube).
      x = (uint32_t) std::abs(tgt_r - plt_r);
      y = (uint32_t) std::abs(tgt_g - plt_g);
      z = (uint32_t) std::abs(tgt_b - plt_b);
      uint32_t dist2 = x * x + y * y + z * z;
      if (dist2 < minimum_dist2) {
        minimum_dist2 = dist2;
        closest_index = (uint8_t) i;
      }
    }
    return closest_index;
  }
  /***
   * Converts an 8bit palette index (e.g. from a display buffer) to a color.
   * @param[in] index The index to look up.
   * @param[in] palette The 256*3 byte RGB palette.
   * @return The RGBW Color object looked up by the palette.
   */
  static Color index8_to_color_palette888(uint8_t index, const uint8_t *palette) {
    Color color = Color(palette[index * 3 + 0], palette[index * 3 + 1], palette[index * 3 + 2], 0);
    return color;
  }
};
}  // namespace display
}  // namespace esphome
