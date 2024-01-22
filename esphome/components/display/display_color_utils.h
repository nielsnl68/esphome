#pragma once
#include "esphome/core/color.h"

namespace esphome {
namespace display {
enum ColorOrder : uint8_t { COLOR_ORDER_RGB = 0, COLOR_ORDER_BGR = 1, COLOR_ORDER_GRB = 2 };

/**
 * the ColorDepth values tells how a pixel is stored:
 * DDDD: is the divider
 * xBBB: number of bytes per pixel
 * I: Indexed value
 * G: grayscale
 * FF FFFF: color structure in bits
 */

enum ColorBitness : uint8_t {
  COLOR_BITNESS_888 = 0x1318,
  COLOR_BITNESS_666 = 0x1312,
  COLOR_BITNESS_565 = 0x1210,
  COLOR_BITNESS_332 = 0x1108,
  COLOR_BITNESS_8I = 0x1188,
  COLOR_BITNESS_8 = 0x1148,
  COLOR_BITNESS_4 = 0x2144,
  COLOR_BITNESS_2 = 0x4142,
  COLOR_BITNESS_1 = 0x8101,
};
static const uint16_t COLOR_BITNESS_COLOR_BITS = 0x003F;
static const uint16_t COLOR_BITNESS_GRAYSCALE = 0x0040;
static const uint16_t COLOR_BITNESS_INDEXED = 0x0080;
static const uint16_t COLOR_BITNESS_BYTES = 0x0700;
static const uint16_t COLOR_BITNESS_DUMMY = 0x0800;
static const uint16_t COLOR_BITNESS_DEVIDER = 0xF000;

inline static uint8_t esp_scale(uint8_t i, uint8_t scale, uint8_t max_value = 255) { return (max_value * i / scale); }
static ColorBits get_color_bits(ColorBitness bitness) {
  ColorBits result;
  switch (bitness) {
    case COLOR_BITNESS_888:
      result.first_bits = 8;
      result.second_bits = 8;
      result.third_bits = 8;
      break;
    case COLOR_BITNESS_666:
      result.first_bits = 6;
      result.second_bits = 6;
      result.third_bits = 6;
      break;
    case COLOR_BITNESS_565:
      result.first_bits = 5;
      result.second_bits = 6;
      result.third_bits = 5;
      break;
    case COLOR_BITNESS_4:
      result.first_bits = 3;
      result.second_bits = 3;
      result.third_bits = 2;
      break;
  }
  return result;
}

struct {
  uint8_t first_bits = 0;
  uint8_t second_bits = 0;
  uint8_t third_bits = 0;
} ColorBits;

class ColorUtil {
 public:
  // depricated
  static Color to_color(uint32_t colorcode, ColorOrder color_order,
                        ColorBitness color_bitness = ColorBitness::COLOR_BITNESS_888, bool right_bit_aligned = true) {
    static Color ColorUtil::to_color(colorcode, color_bitness, nullptr, color_order, right_bit_aligned);
  }

  static Color to_color(uint32_t colorcode, ColorBitness color_bitness, const uint8_t *palette = nullptr,
                        ColorOrder color_order = ColorOrder.COLOR_ORDER_RGB, bool right_bit_aligned = true) {
    if (color_bitness & COLOR_BITNESS_INDEXED) {
      return ColorUtil::index8_to_color_palette888(colorcode, palette);
    }
    if (color_bitness & COLOR_BITNESS_GRAYSCALE) {
      return Color::BLACK;
    }

    uint8_t first_color, second_color, third_color;
    ColorBits bits = get_color_bits(color_bitness);

    first_color = right_bit_aligned
                      ? esp_scale(((colorcode >> (bits.second_bits + bits.third_bits)) & ((1 << bits.first_bits) - 1)),
                                  ((1 << bits.first_bits) - 1))
                      : esp_scale(((colorcode >> 16) & 0xFF), (1 << bits.first_bits) - 1);

    second_color = right_bit_aligned ? esp_scale(((colorcode >> bits.third_bits) & ((1 << bits.second_bits) - 1)),
                                                 ((1 << bits.second_bits) - 1))
                                     : esp_scale(((colorcode >> 8) & 0xFF), ((1 << bits.second_bits) - 1));

    third_color = right_bit_aligned
                      ? esp_scale(((colorcode >> 0) & ((1 << bits.third_bits) - 1)), ((1 << bits.third_bits) - 1))
                      : esp_scale(((colorcode >> 0) & 0xFF), (1 << bits.third_bits) - 1);

    Color color_return;

    switch (color_order) {
      case COLOR_ORDER_RGB:
        color_return.r = first_color;
        color_return.g = second_color;
        color_return.b = third_color;
        break;
      case COLOR_ORDER_BGR:
        color_return.b = first_color;
        color_return.g = second_color;
        color_return.r = third_color;
        break;
      case COLOR_ORDER_GRB:
        color_return.g = first_color;
        color_return.r = second_color;
        color_return.b = third_color;
        break;
    }
    return color_return;
  }

  static uint32_t from_color(Color color, ColorBitness color_bitness, const uint8_t *palette = nullptr,
                             ColorOrder color_order = ColorOrder.COLOR_ORDER_RGB, bool right_bit_aligned = true) {
    if (color_bitness & COLOR_BITNESS_INDEXED) {
      return ColorUtil::color_to_index8_palette888(color, palette);
    }

    if (color_bitness & COLOR_BITNESS_GRAYSCALE) {
      return ColorUtil::color_to_grayscale4(color);
    }

    ColorBits bits = get_color_bits(color_bitness);
    uint16_t red_color, green_color, blue_color;

    red_color = esp_scale8(color.red, ((1 << bits.first_bits) - 1));
    green_color = esp_scale8(color.green, ((1 << bits.second_bits) - 1));
    blue_color = esp_scale8(color.blue, (1 << bits.third_bits) - 1);

    switch (color_order) {
      case COLOR_ORDER_RGB:
        return red_color << (bits.third_bits + bits.second_bits) | green_color << bits.third_bits | blue_color;
      case COLOR_ORDER_BGR:
        return blue_color << (bits.first_bits + bits.second_bits) | green_color << bits.first_bits | red_color;
      case COLOR_ORDER_GRB:
        return green_color << (bits.third_bits + bits.first_bits) | red_color << bits.third_bits | blue_color;
    }
    return 0;
  }

  static inline Color rgb332_to_color(uint8_t rgb332_color) {
    return to_color((uint32_t) rgb332_color, COLOR_ORDER_RGB, COLOR_BITNESS_332);
  }
  static uint8_t color_to_332(Color color, ColorOrder color_order = ColorOrder::COLOR_ORDER_RGB) {
    uint16_t red_color, green_color, blue_color;

    red_color = esp_scale8(color.red, ((1 << 3) - 1));
    green_color = esp_scale8(color.green, ((1 << 3) - 1));
    blue_color = esp_scale8(color.blue, (1 << 2) - 1);

    switch (color_order) {
      case COLOR_ORDER_RGB:
        return red_color << 5 | green_color << 2 | blue_color;
      case COLOR_ORDER_BGR:
        return blue_color << 6 | green_color << 3 | red_color;
      case COLOR_ORDER_GRB:
        return green_color << 5 | red_color << 2 | blue_color;
    }
    return 0;
  }
  static uint16_t color_to_565(Color color, ColorOrder color_order = ColorOrder::COLOR_ORDER_RGB) {
    uint16_t red_color, green_color, blue_color;

    red_color = esp_scale8(color.red, ((1 << 5) - 1));
    green_color = esp_scale8(color.green, ((1 << 6) - 1));
    blue_color = esp_scale8(color.blue, (1 << 5) - 1);

    switch (color_order) {
      case COLOR_ORDER_RGB:
        return red_color << 11 | green_color << 5 | blue_color;
      case COLOR_ORDER_BGR:
        return blue_color << 11 | green_color << 5 | red_color;
      case COLOR_ORDER_GRB:
        return green_color << 10 | red_color << 5 | blue_color;
    }
    return 0;
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
  // static uint8_t color_to_index8_palette888(Color color, uint8_t *palette) {
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
