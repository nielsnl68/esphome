#pragma once

#include <cstdarg>
#include <vector>

#include "rect.h"

#include "esphome/core/color.h"
#include "esphome/core/automation.h"
#include "esphome/core/time.h"
#include "display_color_utils.h"

#ifdef USE_GRAPH
#include "esphome/components/graph/graph.h"
#endif

#ifdef USE_QR_CODE
#include "esphome/components/qr_code/qr_code.h"
#endif

#ifdef USE_GRAPHICAL_DISPLAY_MENU
#include "esphome/components/graphical_display_menu/graphical_display_menu.h"
#endif

namespace esphome {
namespace display {

/** TextAlign is used to tell the display class how to position a piece of text. By default
 * the coordinates you enter for the print*() functions take the upper left corner of the text
 * as the "anchor" point. You can customize this behavior to, for example, make the coordinates
 * refer to the *center* of the text.
 *
 * All text alignments consist of an X and Y-coordinate alignment. For the alignment along the X-axis
 * these options are allowed:
 *
 * - LEFT (x-coordinate of anchor point is on left)
 * - CENTER_HORIZONTAL (x-coordinate of anchor point is in the horizontal center of the text)
 * - RIGHT (x-coordinate of anchor point is on right)
 *
 * For the Y-Axis alignment these options are allowed:
 *
 * - TOP (y-coordinate of anchor is on the top of the text)
 * - CENTER_VERTICAL (y-coordinate of anchor is in the vertical center of the text)
 * - BASELINE (y-coordinate of anchor is on the baseline of the text)
 * - BOTTOM (y-coordinate of anchor is on the bottom of the text)
 *
 * These options are then combined to create combined TextAlignment options like:
 * - TOP_LEFT (default)
 * - CENTER (anchor point is in the middle of the text bounds)
 * - ...
 */
enum class TextAlign {
  TOP = 0x00,
  CENTER_VERTICAL = 0x01,
  BASELINE = 0x02,
  BOTTOM = 0x04,

  LEFT = 0x00,
  CENTER_HORIZONTAL = 0x08,
  RIGHT = 0x10,

  TOP_LEFT = TOP | LEFT,
  TOP_CENTER = TOP | CENTER_HORIZONTAL,
  TOP_RIGHT = TOP | RIGHT,

  CENTER_LEFT = CENTER_VERTICAL | LEFT,
  CENTER = CENTER_VERTICAL | CENTER_HORIZONTAL,
  CENTER_RIGHT = CENTER_VERTICAL | RIGHT,

  BASELINE_LEFT = BASELINE | LEFT,
  BASELINE_CENTER = BASELINE | CENTER_HORIZONTAL,
  BASELINE_RIGHT = BASELINE | RIGHT,

  BOTTOM_LEFT = BOTTOM | LEFT,
  BOTTOM_CENTER = BOTTOM | CENTER_HORIZONTAL,
  BOTTOM_RIGHT = BOTTOM | RIGHT,
};

/** ImageAlign is used to tell the display class how to position a image. By default
 * the coordinates you enter for the image() functions take the upper left corner of the image
 * as the "anchor" point. You can customize this behavior to, for example, make the coordinates
 * refer to the *center* of the image.
 *
 * All image alignments consist of an X and Y-coordinate alignment. For the alignment along the X-axis
 * these options are allowed:
 *
 * - LEFT (x-coordinate of anchor point is on left)
 * - CENTER_HORIZONTAL (x-coordinate of anchor point is in the horizontal center of the image)
 * - RIGHT (x-coordinate of anchor point is on right)
 *
 * For the Y-Axis alignment these options are allowed:
 *
 * - TOP (y-coordinate of anchor is on the top of the image)
 * - CENTER_VERTICAL (y-coordinate of anchor is in the vertical center of the image)
 * - BOTTOM (y-coordinate of anchor is on the bottom of the image)
 *
 * These options are then combined to create combined TextAlignment options like:
 * - TOP_LEFT (default)
 * - CENTER (anchor point is in the middle of the image bounds)
 * - ...
 */
enum class ImageAlign {
  TOP = 0x00,
  CENTER_VERTICAL = 0x01,
  BOTTOM = 0x02,

  LEFT = 0x00,
  CENTER_HORIZONTAL = 0x04,
  RIGHT = 0x08,

  TOP_LEFT = TOP | LEFT,
  TOP_CENTER = TOP | CENTER_HORIZONTAL,
  TOP_RIGHT = TOP | RIGHT,

  CENTER_LEFT = CENTER_VERTICAL | LEFT,
  CENTER = CENTER_VERTICAL | CENTER_HORIZONTAL,
  CENTER_RIGHT = CENTER_VERTICAL | RIGHT,

  BOTTOM_LEFT = BOTTOM | LEFT,
  BOTTOM_CENTER = BOTTOM | CENTER_HORIZONTAL,
  BOTTOM_RIGHT = BOTTOM | RIGHT,

  HORIZONTAL_ALIGNMENT = LEFT | CENTER_HORIZONTAL | RIGHT,
  VERTICAL_ALIGNMENT = TOP | CENTER_VERTICAL | BOTTOM
};

enum DisplayType {
  DISPLAY_TYPE_NONE = 0,
  DISPLAY_TYPE_BINARY = 1,
  DISPLAY_TYPE_GRAYSCALE = 2,
  DISPLAY_TYPE_COLOR = 3,
};

enum DisplayRotation {
  DISPLAY_ROTATION_0_DEGREES = 0,
  DISPLAY_ROTATION_90_DEGREES = 90,
  DISPLAY_ROTATION_180_DEGREES = 180,
  DISPLAY_ROTATION_270_DEGREES = 270,
};

class Display;
class DisplayPage;
class DisplayOnPageChangeTrigger;

using display_writer_t = std::function<void(Display &)>;

#define LOG_DISPLAY(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, prefix type); \
    ESP_LOGCONFIG(TAG, "%s  Rotations: %d °", prefix, (obj)->rotation_); \
    ESP_LOGCONFIG(TAG, "%s  Dimensions: %dpx x %dpx", prefix, (obj)->get_width(), (obj)->get_height()); \
  }

/// Turn the pixel OFF.
extern const Color COLOR_OFF;
/// Turn the pixel ON.
extern const Color COLOR_ON;

class BaseImage {
 public:
  virtual void draw(int x, int y, Display *display, Color color_on, Color color_off) = 0;
  virtual int get_width() const = 0;
  virtual int get_height() const = 0;
};

class BaseFont {
 public:
  virtual void print(int x, int y, Display *display, Color color, const char *text) = 0;
  virtual void measure(const char *str, int *width, int *x_offset, int *baseline, int *height) = 0;
};

class Display : public PollingComponent {
 public:
  void update() override;
  void set_dimensions(int16_t width, int16_t height) {
    this->height_ = height;
    this->width_ = width;
  }
  void set_offsets(int16_t offset_x, int16_t offset_y) {
    this->offset_x_ = offset_x;
    this->offset_y_ = offset_y;
  }

  /// Get the width of the display in pixels with rotation applied.
  virtual int get_width();
  /// Get the height of the display in pixels with rotation applied.
  virtual int get_height();

  /// Get the native display width in pixels.
  int get_native_width() { return this->width_; }
  /// Get the native display height in pixels.
  int get_native_height() { return this->height_; }

  /// software method to set the display rotation.
  void set_rotation(DisplayRotation rotation) { this->rotation_ = rotation; }
  DisplayRotation get_rotation() const { return this->rotation_; }

  /// Internal method to set display auto clearing.
  void set_auto_clear(bool auto_clear_enabled) { this->auto_clear_enabled_ = auto_clear_enabled; }

  /// Deprecated
  virtual DisplayType get_display_type() { return DISPLAY_TYPE_NONE; };

  /// Fill the entire screen with the given color.
  virtual void fill(Color color);

  /// Clear the entire screen by filling it with OFF pixels.
  void clear();

  /// Set a single pixel at the specified coordinates to default color.
  inline void draw_pixel_at(int x, int y) { this->draw_pixel_at(x, y, COLOR_ON); }

  /// Set a single pixel at the specified coordinates to the given color.
  virtual void draw_pixel_at(int x, int y, Color color) = 0;

  /** Given an array of pixels encoded in the nominated format, draw these into the display's buffer.
   * The naive implementation here will work in all cases, but can be overridden by sub-classes
   * in order to optimise the procedure.
   * The parameters describe a rectangular block of pixels, potentially within a larger buffer.
   *
   * \param x_display The starting destination x position
   * \param y_display The starting destination y position
   * \param width the width of the pixel block
   * \param height the height of the pixel block
   * \param data A pointer to the start of the data to be copied
   * \param bitness Defines the number of bits and their format for each pixel
   * \param x_in_data The initial x-offset into the source buffer.
   * \param y_in_data The initial y-offset into the source buffer.
   * \param x_pad How many pixels are in each line after the end of the pixels to be copied.
   *
   * The length of each source buffer line (stride) will be x_offset + w + x_pad.
   */

  virtual void draw_pixels_at(const uint8_t *data, int x_display, int y_display, int x_in_data, int y_in_data,
                              int width, int height, ColorBitness bitness, int x_pad = 0);

  /// Convenience overload for base case where the pixels are packed into the buffer with no gaps (e.g. suits LVGL.)
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, ColorOrder order,
                      ColorBitness bitness, bool big_endian) {
    bitness.color_order = order;
    bitness.little_endian = !big_endian;
    this->draw_pixels_at(ptr, x_start, y_start, 0, 0, w, h, bitness, 0);
  }
  // #depricated
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, ColorOrder order,
                      ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad){
    bitness.color_order = order;
    bitness.little_endian = !big_endian;
    this->draw_pixels_at(ptr, x_start, y_start, x_offset, y_offset, w, h, bitness, x_pad);
  }
  /// Draw a straight line from the point [x1,y1] to [x2,y2] with the given color.
  void line(int x1, int y1, int x2, int y2, Color color = COLOR_ON);

  /// Draw a horizontal line from the point [x,y] to [x+width,y] with the given color.
  void horizontal_line(int x, int y, int width, Color color = COLOR_ON);

  /// Draw a vertical line from the point [x,y] to [x,y+width] with the given color.
  void vertical_line(int x, int y, int height, Color color = COLOR_ON);

  /// Draw the outline of a rectangle with the top left point at [x1,y1] and the bottom right point at
  /// [x1+width,y1+height].
  void rectangle(int x1, int y1, int width, int height, Color color = COLOR_ON);

  /// Fill a rectangle with the top left point at [x1,y1] and the bottom right point at [x1+width,y1+height].
  void filled_rectangle(int x1, int y1, int width, int height, Color color = COLOR_ON);

  /// Draw the outline of a circle centered around [center_x,center_y] with the radius radius with the given color.
  void circle(int center_x, int center_xy, int radius, Color color = COLOR_ON);

  /// Fill a circle centered around [center_x,center_y] with the radius radius with the given color.
  void filled_circle(int center_x, int center_y, int radius, Color color = COLOR_ON);

  /// Draw the outline of a triangle contained between the points [x1,y1], [x2,y2] and [x3,y3] with the given color.
  void triangle(int x1, int y1, int x2, int y2, int x3, int y3, Color color = COLOR_ON);

  /// Fill a triangle contained between the points [x1,y1], [x2,y2] and [x3,y3] with the given color.
  void filled_triangle(int x1, int y1, int x2, int y2, int x3, int y3, Color color = COLOR_ON);

  /** Print `text` with the anchor point at [x,y] with `font`.
   *
   * @param x The x coordinate of the text alignment anchor point.
   * @param y The y coordinate of the text alignment anchor point.
   * @param font The font to draw the text with.
   * @param color The color to draw the text with.
   * @param align The alignment of the text.
   * @param text The text to draw.
   */
  void print(int x, int y, BaseFont *font, Color color, TextAlign align, const char *text);

  /** Print `text` with the top left at [x,y] with `font`.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param font The font to draw the text with.
   * @param color The color to draw the text with.
   * @param text The text to draw.
   */
  void print(int x, int y, BaseFont *font, Color color, const char *text);

  /** Print `text` with the anchor point at [x,y] with `font`.
   *
   * @param x The x coordinate of the text alignment anchor point.
   * @param y The y coordinate of the text alignment anchor point.
   * @param font The font to draw the text with.
   * @param align The alignment of the text.
   * @param text The text to draw.
   */
  void print(int x, int y, BaseFont *font, TextAlign align, const char *text);

  /** Print `text` with the top left at [x,y] with `font`.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param font The font to draw the text with.
   * @param text The text to draw.
   */
  void print(int x, int y, BaseFont *font, const char *text);

  /** Evaluate the printf-format `format` and print the result with the anchor point at [x,y] with `font`.
   *
   * @param x The x coordinate of the text alignment anchor point.
   * @param y The y coordinate of the text alignment anchor point.
   * @param font The font to draw the text with.
   * @param color The color to draw the text with.
   * @param align The alignment of the text.
   * @param format The format to use.
   * @param ... The arguments to use for the text formatting.
   */
  void printf(int x, int y, BaseFont *font, Color color, TextAlign align, const char *format, ...)
      __attribute__((format(printf, 7, 8)));

  /** Evaluate the printf-format `format` and print the result with the top left at [x,y] with `font`.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param font The font to draw the text with.
   * @param color The color to draw the text with.
   * @param format The format to use.
   * @param ... The arguments to use for the text formatting.
   */
  void printf(int x, int y, BaseFont *font, Color color, const char *format, ...) __attribute__((format(printf, 6, 7)));

  /** Evaluate the printf-format `format` and print the result with the anchor point at [x,y] with `font`.
   *
   * @param x The x coordinate of the text alignment anchor point.
   * @param y The y coordinate of the text alignment anchor point.
   * @param font The font to draw the text with.
   * @param align The alignment of the text.
   * @param format The format to use.
   * @param ... The arguments to use for the text formatting.
   */
  void printf(int x, int y, BaseFont *font, TextAlign align, const char *format, ...)
      __attribute__((format(printf, 6, 7)));

  /** Evaluate the printf-format `format` and print the result with the top left at [x,y] with `font`.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param font The font to draw the text with.
   * @param format The format to use.
   * @param ... The arguments to use for the text formatting.
   */
  void printf(int x, int y, BaseFont *font, const char *format, ...) __attribute__((format(printf, 5, 6)));

  /** Evaluate the strftime-format `format` and print the result with the anchor point at [x,y] with `font`.
   *
   * @param x The x coordinate of the text alignment anchor point.
   * @param y The y coordinate of the text alignment anchor point.
   * @param font The font to draw the text with.
   * @param color The color to draw the text with.
   * @param align The alignment of the text.
   * @param format The strftime format to use.
   * @param time The time to format.
   */
  void strftime(int x, int y, BaseFont *font, Color color, TextAlign align, const char *format, ESPTime time)
      __attribute__((format(strftime, 7, 0)));

  /** Evaluate the strftime-format `format` and print the result with the top left at [x,y] with `font`.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param font The font to draw the text with.
   * @param color The color to draw the text with.
   * @param format The strftime format to use.
   * @param time The time to format.
   */
  void strftime(int x, int y, BaseFont *font, Color color, const char *format, ESPTime time)
      __attribute__((format(strftime, 6, 0)));

  /** Evaluate the strftime-format `format` and print the result with the anchor point at [x,y] with `font`.
   *
   * @param x The x coordinate of the text alignment anchor point.
   * @param y The y coordinate of the text alignment anchor point.
   * @param font The font to draw the text with.
   * @param align The alignment of the text.
   * @param format The strftime format to use.
   * @param time The time to format.
   */
  void strftime(int x, int y, BaseFont *font, TextAlign align, const char *format, ESPTime time)
      __attribute__((format(strftime, 6, 0)));

  /** Evaluate the strftime-format `format` and print the result with the top left at [x,y] with `font`.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param font The font to draw the text with.
   * @param format The strftime format to use.
   * @param time The time to format.
   */
  void strftime(int x, int y, BaseFont *font, const char *format, ESPTime time) __attribute__((format(strftime, 5, 0)));

  /** Draw the `image` with the top-left corner at [x,y] to the screen.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param image The image to draw.
   * @param color_on The color to replace in binary images for the on bits.
   * @param color_off The color to replace in binary images for the off bits.
   */
  void image(int x, int y, BaseImage *image, Color color_on = COLOR_ON, Color color_off = COLOR_OFF);

  /** Draw the `image` at [x,y] to the screen.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param image The image to draw.
   * @param align The alignment of the image.
   * @param color_on The color to replace in binary images for the on bits.
   * @param color_off The color to replace in binary images for the off bits.
   */
  void image(int x, int y, BaseImage *image, ImageAlign align, Color color_on = COLOR_ON, Color color_off = COLOR_OFF);

#ifdef USE_GRAPH
  /** Draw the `graph` with the top-left corner at [x,y] to the screen.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param graph The graph id to draw
   * @param color_on The color to replace in binary images for the on bits.
   */
  void graph(int x, int y, graph::Graph *graph, Color color_on = COLOR_ON);

  /** Draw the `legend` for graph with the top-left corner at [x,y] to the screen.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param graph The graph id for which the legend applies to
   * @param graph The graph id for which the legend applies to
   * @param graph The graph id for which the legend applies to
   * @param name_font The font used for the trace name
   * @param value_font The font used for the trace value and units
   * @param color_on The color of the border
   */
  void legend(int x, int y, graph::Graph *graph, Color color_on = COLOR_ON);
#endif  // USE_GRAPH

#ifdef USE_QR_CODE
  /** Draw the `qr_code` with the top-left corner at [x,y] to the screen.
   *
   * @param x The x coordinate of the upper left corner.
   * @param y The y coordinate of the upper left corner.
   * @param qr_code The qr_code to draw
   * @param color_on The color to replace in binary images for the on bits.
   */
  void qr_code(int x, int y, qr_code::QrCode *qr_code, Color color_on = COLOR_ON, int scale = 1);
#endif

#ifdef USE_GRAPHICAL_DISPLAY_MENU
  /**
   * @param x The x coordinate of the upper left corner
   * @param y The y coordinate of the upper left corner
   * @param menu The GraphicalDisplayMenu to draw
   * @param width Width of the menu
   * @param height Height of the menu
   */
  void menu(int x, int y, graphical_display_menu::GraphicalDisplayMenu *menu, int width, int height);
#endif  // USE_GRAPHICAL_DISPLAY_MENU

  /** Get the text bounds of the given string.
   *
   * @param x The x coordinate to place the string at, can be 0 if only interested in dimensions.
   * @param y The y coordinate to place the string at, can be 0 if only interested in dimensions.
   * @param text The text to measure.
   * @param font The font to measure the text bounds with.
   * @param align The alignment of the text. Set to TextAlign::TOP_LEFT if only interested in dimensions.
   * @param x1 A pointer to store the returned x coordinate of the upper left corner in.
   * @param y1 A pointer to store the returned y coordinate of the upper left corner in.
   * @param width A pointer to store the returned text width in.
   * @param height A pointer to store the returned text height in.
   */
  void get_text_bounds(int x, int y, const char *text, BaseFont *font, TextAlign align, int *x1, int *y1, int *width,
                       int *height);

  /// Internal method to set the display writer lambda.
  void set_writer(display_writer_t &&writer);

  void show_page(DisplayPage *page);
  void show_next_page();
  void show_prev_page();

  void set_pages(std::vector<DisplayPage *> pages);

  const DisplayPage *get_active_page() const { return this->page_; }

  void add_on_page_change_trigger(DisplayOnPageChangeTrigger *t) { this->on_page_change_triggers_.push_back(t); }


  /** Set the clipping rectangle for further drawing
   *
   * @param[in]  rect:       Pointer to Rect for clipping (or NULL for entire screen)
   *
   * return true if success, false if error
   */
  void start_clipping(Rect rect);
  void start_clipping(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    start_clipping(Rect(left, top, right - left, bottom - top));
  };

  /** Add a rectangular region to the invalidation region
   * - This is usually called when an element has been modified
   *
   * @param[in]  rect: Rectangle to add to the invalidation region
   */
  void extend_clipping(Rect rect);
  void extend_clipping(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    this->extend_clipping(Rect(left, top, right - left, bottom - top));
  };

  /** substract a rectangular region to the invalidation region
   *  - This is usually called when an element has been modified
   *
   * @param[in]  rect: Rectangle to add to the invalidation region
   */
  void shrink_clipping(Rect rect);
  void shrink_clipping(uint16_t left, uint16_t top, uint16_t right, uint16_t bottom) {
    this->shrink_clipping(Rect(left, top, right - left, bottom - top));
  };

  /** Reset the invalidation region
   */
  void end_clipping();

  /** Get the current the clipping rectangle
   *
   * return rect for active clipping region
   */
  Rect get_clipping() const;

  bool is_clipping() const { return !this->clipping_rectangle_.empty(); }

  /** Check if pixel is within region of display.
   */
  bool clip(int x, int y);

 protected:
  bool clamp_x_(int x, int w, int &min_x, int &max_x);
  bool clamp_y_(int y, int h, int &min_y, int &max_y);
  void vprintf_(int x, int y, BaseFont *font, Color color, TextAlign align, const char *format, va_list arg);

  virtual void do_update_();
  void clear_clipping_();
  virtual void display_buffer(){}

  /**
   * This method fills a triangle using only integer variables by using a
   * modified bresenham algorithm.
   * It is mandatory that [x2,y2] and [x3,y3] lie on the same horizontal line,
   * so y2 must be equal to y3.
   */
  void filled_flat_side_triangle_(int x1, int y1, int x2, int y2, int x3, int y3, Color color);
  void sort_triangle_points_by_y_(int *x1, int *y1, int *x2, int *y2, int *x3, int *y3);

  DisplayRotation rotation_{DISPLAY_ROTATION_0_DEGREES};
  optional<display_writer_t> writer_{};
  DisplayPage *page_{nullptr};
  DisplayPage *previous_page_{nullptr};
  std::vector<DisplayOnPageChangeTrigger *> on_page_change_triggers_;
  bool auto_clear_enabled_{true};
  std::vector<Rect> clipping_rectangle_;

  uint16_t width_{0};
  uint16_t height_{0};
  int16_t offset_x_{0};
  int16_t offset_y_{0};

  bool processing_update_ = false;
  bool needs_update_ = false;
};

class DisplayPage {
 public:
  DisplayPage(display_writer_t writer);
  void show();
  void show_next();
  void show_prev();
  void set_parent(Display *parent);
  void set_prev(DisplayPage *prev);
  void set_next(DisplayPage *next);
  const display_writer_t &get_writer() const;

 protected:
  Display *parent_;
  display_writer_t writer_;
  DisplayPage *prev_{nullptr};
  DisplayPage *next_{nullptr};
};

template<typename... Ts> class DisplayPageShowAction : public Action<Ts...> {
 public:
  TEMPLATABLE_VALUE(DisplayPage *, page)

  void play(Ts... x) override {
    auto *page = this->page_.value(x...);
    if (page != nullptr) {
      page->show();
    }
  }
};

template<typename... Ts> class DisplayPageShowNextAction : public Action<Ts...> {
 public:
  DisplayPageShowNextAction(Display *buffer) : buffer_(buffer) {}

  void play(Ts... x) override { this->buffer_->show_next_page(); }

  Display *buffer_;
};

template<typename... Ts> class DisplayPageShowPrevAction : public Action<Ts...> {
 public:
  DisplayPageShowPrevAction(Display *buffer) : buffer_(buffer) {}

  void play(Ts... x) override { this->buffer_->show_prev_page(); }

  Display *buffer_;
};

template<typename... Ts> class DisplayIsDisplayingPageCondition : public Condition<Ts...> {
 public:
  DisplayIsDisplayingPageCondition(Display *parent) : parent_(parent) {}

  void set_page(DisplayPage *page) { this->page_ = page; }
  bool check(Ts... x) override { return this->parent_->get_active_page() == this->page_; }

 protected:
  Display *parent_;
  DisplayPage *page_;
};

class DisplayOnPageChangeTrigger : public Trigger<DisplayPage *, DisplayPage *> {
 public:
  explicit DisplayOnPageChangeTrigger(Display *parent) { parent->add_on_page_change_trigger(this); }
  void process(DisplayPage *from, DisplayPage *to);
  void set_from(DisplayPage *p) { this->from_ = p; }
  void set_to(DisplayPage *p) { this->to_ = p; }

 protected:
  DisplayPage *from_{nullptr};
  DisplayPage *to_{nullptr};
};

}  // namespace display
}  // namespace esphome
