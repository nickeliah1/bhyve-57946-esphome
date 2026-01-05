#pragma once

#include "esphome.h"
#include "driver/gpio.h"

// CS1622 LCD Controller Driver for Orbit B-Hyve
// Protocol similar to HT1621
// Pins: CS=GPIO21, WR=GPIO0, DATA=GPIO32

#define LCD_CS_PIN   GPIO_NUM_21
#define LCD_WR_PIN   GPIO_NUM_0
#define LCD_DATA_PIN GPIO_NUM_32

// CS1622 Commands (3-bit command mode prefix: 100)
#define CMD_SYS_DIS  0x00  // System disable
#define CMD_SYS_EN   0x01  // System enable
#define CMD_LCD_OFF  0x02  // LCD off
#define CMD_LCD_ON   0x03  // LCD on
#define CMD_BIAS     0x29  // 1/3 bias, 4 commons (typical for segment LCD)
#define CMD_RC_256K  0x18  // Internal RC oscillator

// ============================================================
// 7-Segment Font Table
// ============================================================
// Standard 7-segment layout:
//    AAAA
//   F    B
//    GGGG
//   E    C
//    DDDD
//
// Bit order: 0bxGFEDCBA (bit 0=A, bit 1=B, ... bit 6=G)

const uint8_t FONT_7SEG[128] = {
  // 0x00-0x1F: control chars (blank)
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  // 0x20-0x2F: space and punctuation
  0b0000000, // ' ' (space)
  0,0,0,0,0,0,0,0,0,0,0,0,
  0b1000000, // '-' (0x2D)
  0,0,
  // 0x30-0x39: digits 0-9
  0b0111111, // 0
  0b0000110, // 1
  0b1011011, // 2
  0b1001111, // 3
  0b1100110, // 4
  0b1101101, // 5
  0b1111101, // 6
  0b0000111, // 7
  0b1111111, // 8
  0b1101111, // 9
  // 0x3A-0x40: punctuation
  0,0,0,0,0,0,0,
  // 0x41-0x5A: uppercase A-Z
  0b1110111, // A
  0b1111100, // B (lowercase b looks better)
  0b0111001, // C
  0b1011110, // D (lowercase d)
  0b1111001, // E
  0b1110001, // F
  0b0111101, // G
  0b1110110, // H
  0b0000110, // I
  0b0011110, // J
  0b1110101, // K (approximation)
  0b0111000, // L
  0b0110111, // M (approximation - shows as upside-down U with middle)
  0b1010100, // N (lowercase n)
  0b0111111, // O
  0b1110011, // P
  0b1100111, // Q
  0b1010000, // R (lowercase r)
  0b1101101, // S (same as 5)
  0b1111000, // T (lowercase t)
  0b0111110, // U
  0b0111110, // V (same as U)
  0b0111110, // W (approximation)
  0b1110110, // X (same as H)
  0b1101110, // Y
  0b1011011, // Z (same as 2)
  // Rest can be 0
  0,0,0,0,0,0,
  // 0x61-0x7A: lowercase (same as uppercase)
  0b1110111, // a
  0b1111100, // b
  0b1011000, // c (lowercase)
  0b1011110, // d
  0b1111001, // e
  0b1110001, // f
  0b0111101, // g
  0b1110100, // h (lowercase)
  0b0000100, // i
  0b0011110, // j
  0b1110101, // k
  0b0111000, // l
  0b0010101, // m
  0b1010100, // n
  0b1011100, // o (lowercase)
  0b1110011, // p
  0b1100111, // q
  0b1010000, // r
  0b1101101, // s
  0b1111000, // t
  0b0011100, // u (lowercase)
  0b0011100, // v
  0b0011100, // w
  0b1110110, // x
  0b1101110, // y
  0b1011011, // z
  0,0,0,0,0  // remaining
};

// ============================================================
// Digit segment mapping - physical segment indices for each digit
// ============================================================
// Each digit has 7 segments: A, B, C, D, E, F, G
// These arrays map to physical segment indices (0-159)
// TODO: Fill in from segment scanning!

// Digit positions: [0]=leftmost ... [3]=rightmost
const uint8_t DIGIT_SEGMENTS[4][7] = {
  // Digit 0 (leftmost):  {A,  B,  C,  D,  E,  F,  G}
  {58, 56, 61, 60, 62, 57, 63},
  // Digit 1:
  {66, 64, 69, 68, 70, 65, 71},
  // Digit 2:
  {74, 72, 77, 76, 78, 73, 79},
  // Digit 3 (rightmost):
  {82, 80, 85, 84, 86, 81, 87},
};


class CS1622Display : public Component {
 public:
  void setup() override {
    // Delay LCD init to ensure boot strapping is complete
    // GPIO0 is a strapping pin - must be HIGH during boot
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    gpio_set_direction(LCD_CS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_WR_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_DATA_PIN, GPIO_MODE_OUTPUT);
    
    gpio_set_level(LCD_CS_PIN, 1);
    gpio_set_level(LCD_WR_PIN, 1);
    gpio_set_level(LCD_DATA_PIN, 1);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Initialize LCD controller
    send_command(CMD_SYS_EN);   // Enable system oscillator
    send_command(CMD_RC_256K);  // Use internal RC oscillator
    send_command(CMD_BIAS);     // Set bias and commons
    send_command(CMD_LCD_ON);   // Turn on LCD
    
    // Clear all segments
    clear();
    
    // Say HI! on startup
    show_greeting();
    
    ESP_LOGI("CS1622", "LCD initialized");
  }
  
  void show_greeting() {
    set_segment(147, true);
  }

  // Clear the 4-digit display
  void clear_digits() {
    display_text("    ");
  }

  // Display a single character on a digit position (0-3)
  void display_char(uint8_t digit_pos, char c) {
    if (digit_pos >= 4) return;
    
    uint8_t font_data = FONT_7SEG[(uint8_t)c];
    
    // Set each of the 7 segments for this digit
    for (uint8_t seg = 0; seg < 7; seg++) {
      bool on = (font_data >> seg) & 0x01;
      uint8_t segment_idx = DIGIT_SEGMENTS[digit_pos][seg];
      if (segment_idx > 0) {  // 0 means unmapped
        set_segment(segment_idx, on);
      }
    }
  }

  // Display a 4-character string on the 7-segment digits
  void display_text(const char* text) {
    for (uint8_t i = 0; i < 4 && text[i] != '\0'; i++) {
      display_char(i, text[i]);
    }
    ESP_LOGI("CS1622", "Display: %s", text);
  }

  // Display time in MM:SS format (seconds input)
  void display_time(uint32_t total_seconds) {
    uint8_t minutes = (total_seconds / 60) % 100;  // Cap at 99 minutes
    uint8_t seconds = total_seconds % 60;
    
    // Format as MM:SS
    char time_str[5];
    time_str[0] = '0' + (minutes / 10);
    time_str[1] = '0' + (minutes % 10);
    time_str[2] = '0' + (seconds / 10);
    time_str[3] = '0' + (seconds % 10);
    time_str[4] = '\0';
    
    // Display the digits
    for (uint8_t i = 0; i < 4; i++) {
      display_char(i, time_str[i]);
    }
    
    // Light up the colon (segment 75)
    set_segment(75, true);
  }

  // Clear time display (including colon)
  void clear_time() {
    display_text("    ");
    set_segment(75, false);  // Turn off colon
  }

  // Show zone number (1-6) on separate display area
  void show_zone_number(uint8_t zone) {
    set_segment(43, true);  // Light up "STATION" text
    
    // Display zone number on the zone 7-segment digit
    if (zone >= 1 && zone <= 6) {
      char zone_char = '0' + zone;
      display_zone_digit(zone_char);
    }
    ESP_LOGI("CS1622", "Zone: %d", zone);
  }

  // Clear zone number display
  void clear_zone_number() {
    set_segment(43, false);  // Turn off "STATION" text
    display_zone_digit(' ');  // Clear the zone digit
  }

  // Display a character on the zone 7-segment digit
  void display_zone_digit(char c) {
    uint8_t font_data = FONT_7SEG[(uint8_t)c];
    
    // Zone digit segment mapping: {A, B, C, D, E, F, G}
    // TODO: Fill in the actual segment numbers!
    const uint8_t ZONE_DIGIT[7] = {44, 46, 41, 42, 40, 45, 47};
    
    for (uint8_t seg = 0; seg < 7; seg++) {
      bool on = (font_data >> seg) & 0x01;
      if (ZONE_DIGIT[seg] > 0) {
        set_segment(ZONE_DIGIT[seg], on);
      }
    }
  }

  void clear() {
    // Write zeros to all 40 addresses (159 segments total)
    for (uint8_t addr = 0; addr < 40; addr++) {
      write_data(addr, 0x00);
    }
    // Also clear segment buffer
    memset(segment_buffer, 0, sizeof(segment_buffer));
  }

  // Write 4-bit data to a specific address
  void write_data(uint8_t addr, uint8_t data) {
    gpio_set_level(LCD_CS_PIN, 0);
    
    // Write mode command: 101 (3 bits)
    write_bits(0b101, 3);
    
    // Address: 6 bits
    write_bits(addr, 6);
    
    // Data: 4 bits
    write_bits(data & 0x0F, 4);
    
    gpio_set_level(LCD_CS_PIN, 1);
  }

  // Write raw segment data to LCD RAM
  void write_raw(uint8_t *data, uint8_t len) {
    gpio_set_level(LCD_CS_PIN, 0);
    
    // Write mode command starting at address 0
    write_bits(0b101, 3);
    write_bits(0, 6);  // Start at address 0
    
    // Write consecutive data
    for (uint8_t i = 0; i < len; i++) {
      write_bits(data[i] & 0x0F, 4);
    }
    
    gpio_set_level(LCD_CS_PIN, 1);
  }

  // Display a simple test pattern - all 159 segments ON
  void test_pattern() {
    for (uint8_t addr = 0; addr < 40; addr++) {
      segment_buffer[addr] = 0x0F;
      write_data(addr, 0x0F);
    }
  }

  // Test a single segment: address 0-31, bit 0-3
  void test_segment(uint8_t addr, uint8_t bit) {
    clear();
    if (addr < 32 && bit < 4) {
      write_data(addr, 1 << bit);
      ESP_LOGI("CS1622", "Testing addr=%d, bit=%d", addr, bit);
    }
  }

  // Scan through all 159 segments one by one
  void scan_segment(uint16_t index) {
    clear();
    if (index < 160) {  // 40 addresses × 4 bits = 160 max
      uint8_t addr = index / 4;
      uint8_t bit = index % 4;
      segment_buffer[addr] = (1 << bit);
      write_data(addr, 1 << bit);
      ESP_LOGI("CS1622", "Segment %d: addr=%d, bit=%d", index, addr, bit);
    }
  }

  uint16_t current_scan_index = 0;
  
  // Call this to advance to next segment (for button-driven scanning)
  void next_segment() {
    current_scan_index = (current_scan_index + 1) % 160;  // 159 segments (0-159)
    scan_segment(current_scan_index);
  }
  
  void prev_segment() {
    current_scan_index = (current_scan_index + 159) % 160;  // wrap backwards
    scan_segment(current_scan_index);
  }

  uint16_t get_current_index() { return current_scan_index; }

  // ============================================================
  // Segment Control (set individual segments without clearing)
  // ============================================================
  
  // Memory buffer to track segment states
  uint8_t segment_buffer[40] = {0};  // 40 addresses × 4 bits = 160 max (159 used)
  
  // Set a specific segment ON (index 0-255)
  void set_segment(uint16_t index, bool on) {
    uint8_t addr = index / 4;
    uint8_t bit = index % 4;
    if (addr < 64) {
      if (on) {
        segment_buffer[addr] |= (1 << bit);
      } else {
        segment_buffer[addr] &= ~(1 << bit);
      }
      write_data(addr, segment_buffer[addr]);
    }
  }
  
  // Set multiple segments from an array of indices
  void set_segments(const uint16_t* indices, uint8_t count, bool on) {
    for (uint8_t i = 0; i < count; i++) {
      set_segment(indices[i], on);
    }
  }
  
  // WiFi indicator segments (156-159)
  void show_wifi_connected() {
    set_segment(156, true);
    set_segment(157, true);
    set_segment(158, true);
    set_segment(159, true);
    ESP_LOGI("CS1622", "WiFi connected indicator ON");
  }
  
  void show_wifi_disconnected() {
    set_segment(156, false);
    set_segment(157, false);
    set_segment(158, false);
    set_segment(159, false);
    ESP_LOGI("CS1622", "WiFi disconnected indicator OFF");
  }

 private:
  void send_command(uint8_t cmd) {
    gpio_set_level(LCD_CS_PIN, 0);
    
    // Command mode: 100 (3 bits) + 8-bit command + 1 don't care bit
    write_bits(0b100, 3);
    write_bits(cmd, 8);
    write_bits(0, 1);
    
    gpio_set_level(LCD_CS_PIN, 1);
  }

  void write_bits(uint8_t data, uint8_t bits) {
    for (int8_t i = bits - 1; i >= 0; i--) {
      gpio_set_level(LCD_WR_PIN, 0);
      gpio_set_level(LCD_DATA_PIN, (data >> i) & 0x01);
      esp_rom_delay_us(4);
      gpio_set_level(LCD_WR_PIN, 1);
      esp_rom_delay_us(4);
    }
  }
};

// Global instance
CS1622Display *lcd_display = nullptr;
