#include "display_iobusses.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "esp_lcd_panel_ops.h"

namespace esphome {
namespace display {

static const char *const TAG = "DisplayDriver";

static const uint16_t SPI_SETUP_US = 100;         // estimated fixed overhead in microseconds for an SPI write
static const uint16_t SPI_MAX_BLOCK_SIZE = 4092;  // Max size of continuous SPI transfer

// ======================== displayInterface

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

// ======================== SPI_Interface

void SPIBus::setup() {
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

// ======================== SPI16D_Interface

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

void RGBBus::setup() {
  esph_log_config(TAG, "Setting up RPI_DPI_RGB");
  esp_lcd_rgb_panel_config_t config{};
  config.flags.fb_in_psram = 1;
  config.timings.h_res = this->width_;
  config.timings.v_res = this->height_;
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

}  // namespace display
}  // namespace esphome
