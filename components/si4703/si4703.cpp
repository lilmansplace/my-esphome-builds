#include "si4703.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace si4703 {

static const char *const TAG = "si4703";
static const char *const VERSION = "V13_DUAL_PLATFORM_2025_11_07";

Si4703Component::Si4703Component() {
  ESP_LOGI(TAG, "Si4703Component Constructor - Version: %s", VERSION);
}

void Si4703Component::init_wire_() {
  // Get I2C pins from ESPHome's I2C bus configuration
  // The parent I2CDevice class has the bus_ pointer
  if (this->bus_ != nullptr) {
    // Extract SDA and SCL pins from the parent bus
    // ESPHome's I2C bus doesn't expose pins directly, so we need to 
    // let ESPHome initialize I2C and just use the existing Wire instance
    ESP_LOGI(TAG, "Using ESPHome I2C bus configuration");
  }
  
  // Wire is already initialized by ESPHome's I2C component
  // We just need to use it directly
  ESP_LOGI(TAG, "Arduino Wire library ready");
}

void Si4703Component::setup() {
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "Si4703 SETUP - Version: %s", VERSION);
  ESP_LOGI(TAG, "Platform: %s", ARDUINO_BOARD);
  ESP_LOGI(TAG, "========================================");
  
  // Initialize reset pin
  if (this->reset_pin_ == nullptr) {
    ESP_LOGE(TAG, "CRITICAL: Reset pin is NULL!");
    this->mark_failed();
    return;
  }
  
  ESP_LOGI(TAG, "Configuring reset pin...");
  this->reset_pin_->setup();
  
  // Initialize Wire library access
  this->init_wire_();
  
  // Perform Si4703 reset sequence
  ESP_LOGI(TAG, "=== Starting Si4703 reset sequence ===");
  
  // Step 1: Put Si4703 into reset
  ESP_LOGD(TAG, "Step 1: Asserting reset (LOW)");
  this->reset_pin_->digital_write(false);
  delay(1);
  
  // Step 2: I2C already configured by ESPHome
  ESP_LOGD(TAG, "Step 2: I2C configured by ESPHome component");
  delay(1);
  
  // Step 3: Release reset
  ESP_LOGD(TAG, "Step 3: Releasing reset (HIGH)");
  this->reset_pin_->digital_write(true);
  delay(1);
  
  // Step 4: Wait for stabilization
  ESP_LOGD(TAG, "Step 4: Waiting for stabilization...");
  delay(200);
  
  ESP_LOGI(TAG, "Reset sequence complete, attempting I2C communication...");
  
  // Try to read registers using Arduino Wire
  ESP_LOGI(TAG, "Reading Si4703 registers using Arduino Wire library...");
  
  // Try up to 3 times
  bool read_success = false;
  for (int attempt = 1; attempt <= 3; attempt++) {
    ESP_LOGI(TAG, "Communication attempt %d/3...", attempt);
    read_success = this->read_registers_wire_();
    
    if (read_success) {
      ESP_LOGI(TAG, "✓ Communication successful on attempt %d!", attempt);
      break;
    }
    
    ESP_LOGW(TAG, "✗ Attempt %d failed, retrying...", attempt);
    delay(100);
  }
  
  if (!read_success) {
    ESP_LOGE(TAG, "========================================");
    ESP_LOGE(TAG, "FAILED to communicate with Si4703!");
    ESP_LOGE(TAG, "========================================");
    ESP_LOGE(TAG, "");
    ESP_LOGE(TAG, "Tried 3 times but Si4703 is not responding.");
    ESP_LOGE(TAG, "");
    ESP_LOGE(TAG, "Common causes:");
    ESP_LOGE(TAG, "  1. Wiring issue - check all connections");
    ESP_LOGE(TAG, "  2. Power issue - verify 3.3V is stable");
    ESP_LOGE(TAG, "  3. SEN pin not grounded (must be GND for I2C mode)");
    ESP_LOGE(TAG, "  4. Pull-up resistors missing (2.2k-4.7k)");
    ESP_LOGE(TAG, "  5. Defective Si4703 module");
    ESP_LOGE(TAG, "");
    ESP_LOGE(TAG, "Verify wiring for your platform:");
#ifdef USE_ESP8266
    ESP_LOGE(TAG, "  ESP8266:");
    ESP_LOGE(TAG, "    Si4703 SDIO -> ESP8266 GPIO4 (D2/SDA)");
    ESP_LOGE(TAG, "    Si4703 SCLK -> ESP8266 GPIO5 (D1/SCL)");
    ESP_LOGE(TAG, "    Si4703 RST  -> ESP8266 GPIO14 (D5)");
#elif defined(USE_ESP32)
    ESP_LOGE(TAG, "  ESP32:");
    ESP_LOGE(TAG, "    Si4703 SDIO -> ESP32 SDA pin (check your YAML)");
    ESP_LOGE(TAG, "    Si4703 SCLK -> ESP32 SCL pin (check your YAML)");
    ESP_LOGE(TAG, "    Si4703 RST  -> ESP32 RST pin (check your YAML)");
#endif
    ESP_LOGE(TAG, "  Common to all:");
    ESP_LOGE(TAG, "    Si4703 VCC  -> 3.3V (NOT 5V!)");
    ESP_LOGE(TAG, "    Si4703 GND  -> GND");
    ESP_LOGE(TAG, "    Si4703 SEN  -> GND (CRITICAL!)");
    ESP_LOGE(TAG, "========================================");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "✓ Successfully communicated with Si4703!");
  
  // Read and validate chip ID
  uint16_t device_id = this->si4703_registers_[DEVICEID];
  uint16_t chip_id = this->si4703_registers_[SI4703_CHIPID];
  
  ESP_LOGI(TAG, "Device ID: 0x%04X, Chip ID: 0x%04X", device_id, chip_id);
  
  // Check for invalid chip ID
  if (device_id == 0x0000 && chip_id == 0x0000) {
    ESP_LOGE(TAG, "========================================");
    ESP_LOGE(TAG, "ERROR: Invalid Chip ID (0x0000)!");
    ESP_LOGE(TAG, "========================================");
    ESP_LOGE(TAG, "I2C communication works but chip returned zeros.");
    ESP_LOGE(TAG, "");
    ESP_LOGE(TAG, "Expected: Device ID 0x1242, Chip ID 0x58xx");
    ESP_LOGE(TAG, "Got:      Device ID 0x0000, Chip ID 0x0000");
    ESP_LOGE(TAG, "");
    ESP_LOGE(TAG, "This usually means:");
    ESP_LOGE(TAG, "  - SEN pin is NOT grounded (must be tied to GND!)");
    ESP_LOGE(TAG, "  - Module is in wrong mode (SPI vs I2C)");
    ESP_LOGE(TAG, "  - Power supply issue");
    ESP_LOGE(TAG, "========================================");
    this->mark_failed();
    return;
  }
  
  // Validate chip ID pattern
  if (device_id == 0x1242) {
    ESP_LOGI(TAG, "✓ Valid Device ID confirmed!");
  } else {
    ESP_LOGW(TAG, "Unexpected Device ID: 0x%04X (expected 0x1242)", device_id);
  }
  
  if ((chip_id & 0xFF00) == 0x5800) {
    ESP_LOGI(TAG, "✓ Confirmed Si4703 chip!");
  } else if ((chip_id & 0xFF00) == 0x4800) {
    ESP_LOGW(TAG, "Detected Si4702 chip (0x%04X) - no RDS support", chip_id);
  } else {
    ESP_LOGW(TAG, "Unexpected Chip ID: 0x%04X", chip_id);
  }

  // Initialize the Si4703
  ESP_LOGI(TAG, "Configuring Si4703 registers...");
  
  // Step 1: Enable 32.768kHz crystal oscillator
  ESP_LOGD(TAG, "Enabling crystal oscillator...");
  this->si4703_registers_[TEST1] = 0x8100;  // XOSCEN bit
  
  if (!this->update_registers_wire_()) {
    ESP_LOGE(TAG, "Failed to enable oscillator");
    this->mark_failed();
    return;
  }
  
  delay(500);  // Wait for crystal to stabilize

  // Step 2: Read registers again after oscillator starts
  if (!this->read_registers_wire_()) {
    ESP_LOGE(TAG, "Failed to read registers after oscillator enable");
    this->mark_failed();
    return;
  }
  
  // Step 3: Configure POWERCFG register
  this->si4703_registers_[POWERCFG] = 0x4001;  // DMUTE=1, ENABLE=1
  
  // Step 4: Configure SYSCONFIG1 register  
  this->si4703_registers_[SYSCONFIG1] |= (1 << RDS);  // Enable RDS
  this->si4703_registers_[SYSCONFIG1] |= (1 << DE);   // 50µs de-emphasis (USA)
  
  // Step 5: Configure SYSCONFIG2 register
  // Set channel spacing to 100kHz (SPACE = 01)
  this->si4703_registers_[SYSCONFIG2] &= ~(1 << SPACE1);  // Clear SPACE1 (bit 5)
  this->si4703_registers_[SYSCONFIG2] |= (1 << SPACE0);   // Set SPACE0 (bit 4)
  // Result: SPACE[1:0] = 01 = 100kHz spacing (Europe/Japan standard)
  
  this->si4703_registers_[SYSCONFIG2] &= 0xFFF0;  // Clear volume bits
  this->si4703_registers_[SYSCONFIG2] |= 0x0001;  // Set volume to minimum initially
  
  if (!this->update_registers_wire_()) {
    ESP_LOGE(TAG, "Failed to write initial configuration");
    this->mark_failed();
    return;
  }
  
  delay(110);  // Powerup time

  ESP_LOGI(TAG, "✓✓✓ Si4703 INITIALIZED SUCCESSFULLY! ✓✓✓");
  ESP_LOGI(TAG, "========================================");
}

void Si4703Component::loop() {
  // Periodic updates can go here if needed
}

void Si4703Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Si4703 FM Radio:");
  ESP_LOGCONFIG(TAG, "  Version: %s", VERSION);
  ESP_LOGCONFIG(TAG, "  I2C Address: 0x%02X", this->address_);
  
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Status: FAILED - device not responding");
  } else {
    ESP_LOGI(TAG, "  Status: OK - ready for operation");
    
    // Read current chip info
    if (this->read_registers_wire_()) {
      uint16_t device_id = this->si4703_registers_[DEVICEID];
      uint16_t chip_id = this->si4703_registers_[SI4703_CHIPID];
      ESP_LOGCONFIG(TAG, "  Device ID: 0x%04X", device_id);
      ESP_LOGCONFIG(TAG, "  Chip ID: 0x%04X", chip_id);
    }
  }
  
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

bool Si4703Component::read_registers_wire_() {
  // Si4703 always reads starting from register 0x0A
  // Read 32 bytes (16 registers x 2 bytes each)
  
  Wire.requestFrom((uint8_t)SI4703_ADDRESS, (uint8_t)32);
  
  // Wait for data with timeout
  unsigned long start = millis();
  while (Wire.available() < 32 && (millis() - start) < 100) {
    delay(1);
  }
  
  if (Wire.available() < 32) {
    ESP_LOGV(TAG, "Wire.requestFrom() failed - only %d bytes available", Wire.available());
    // Clear any partial data
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }
  
  // Read data starting from register 0x0A
  int reg = 0x0A;
  for (int i = 0; i < 16; i++) {
    uint8_t high_byte = Wire.read();
    uint8_t low_byte = Wire.read();
    this->si4703_registers_[reg] = (high_byte << 8) | low_byte;
    
    reg++;
    if (reg == 0x10) reg = 0;  // Wrap around to register 0
  }
  
  return true;
}

bool Si4703Component::update_registers_wire_() {
  // Si4703 writes starting at register 0x02, auto-increments to 0x07
  
  Wire.beginTransmission(SI4703_ADDRESS);
  
  // Write registers 0x02 through 0x07
  for (int reg = 0x02; reg <= 0x07; reg++) {
    Wire.write(this->si4703_registers_[reg] >> 8);    // High byte
    Wire.write(this->si4703_registers_[reg] & 0xFF);  // Low byte
  }
  
  uint8_t error = Wire.endTransmission();
  
  if (error != 0) {
    ESP_LOGV(TAG, "Wire.endTransmission() failed with error %d", error);
    return false;
  }
  
  return true;
}

void Si4703Component::set_frequency(float frequency) {
  if (this->is_failed()) {
    return;
  }
  
  ESP_LOGD(TAG, "Setting frequency to %.1f MHz", frequency);
  
  // Channel = (Frequency * 10 - 875)
  // Example: 96.3 MHz = 963 - 875 = 88
  int channel = (int)(frequency * 10.0 - 875);
  
  if (channel < 0 || channel > 511) {
    ESP_LOGW(TAG, "Invalid frequency: %.1f MHz", frequency);
    return;
  }
  
  if (!this->read_registers_wire_()) {
    return;
  }
  
  // Set channel and tune bit
  this->si4703_registers_[CHANNEL] &= 0xFE00;  // Clear channel bits
  this->si4703_registers_[CHANNEL] |= channel;
  this->si4703_registers_[CHANNEL] |= (1 << TUNE);  // Start tune
  
  if (!this->update_registers_wire_()) {
    ESP_LOGW(TAG, "Failed to write frequency");
    return;
  }
  
  delay(60);  // Tuning delay
  
  // Wait for STC (Seek/Tune Complete)
  for (int i = 0; i < 50; i++) {
    if (!this->read_registers_wire_()) {
      return;
    }
    if ((this->si4703_registers_[STATUSRSSI] & (1 << STC)) != 0) {
      break;
    }
    delay(10);
  }
  
  // Clear tune bit
  if (!this->read_registers_wire_()) {
    return;
  }
  this->si4703_registers_[CHANNEL] &= ~(1 << TUNE);
  this->update_registers_wire_();
  
  // Wait for STC to clear
  for (int i = 0; i < 50; i++) {
    if (!this->read_registers_wire_()) {
      return;
    }
    if ((this->si4703_registers_[STATUSRSSI] & (1 << STC)) == 0) {
      break;
    }
    delay(10);
  }
  
  ESP_LOGI(TAG, "Tuned to %.1f MHz", frequency);
}

void Si4703Component::set_volume(int volume) {
  if (this->is_failed()) {
    return;
  }
  
  if (volume < 0) volume = 0;
  if (volume > 15) volume = 15;
  
  ESP_LOGD(TAG, "Setting volume to %d", volume);
  
  if (!this->read_registers_wire_()) {
    return;
  }
  
  this->si4703_registers_[SYSCONFIG2] &= 0xFFF0;  // Clear volume
  this->si4703_registers_[SYSCONFIG2] |= volume;
  this->update_registers_wire_();
}

void Si4703Component::seek_up() {
  if (this->is_failed()) {
    return;
  }
  
  ESP_LOGD(TAG, "Seeking up...");
  
  if (!this->read_registers_wire_()) {
    return;
  }
  
  this->si4703_registers_[POWERCFG] |= (1 << SEEKUP);
  this->si4703_registers_[POWERCFG] |= (1 << SEEK);
  this->update_registers_wire_();
  this->wait_for_seek_();
}

void Si4703Component::seek_down() {
  if (this->is_failed()) {
    return;
  }
  
  ESP_LOGD(TAG, "Seeking down...");
  
  if (!this->read_registers_wire_()) {
    return;
  }
  
  this->si4703_registers_[POWERCFG] &= ~(1 << SEEKUP);
  this->si4703_registers_[POWERCFG] |= (1 << SEEK);
  this->update_registers_wire_();
  this->wait_for_seek_();
}

void Si4703Component::wait_for_seek_() {
  // Wait for seek complete
  for (int i = 0; i < 200; i++) {
    if (!this->read_registers_wire_()) {
      return;
    }
    if ((this->si4703_registers_[STATUSRSSI] & (1 << STC)) != 0) {
      break;
    }
    delay(10);
  }
  
  // Clear seek bit
  if (!this->read_registers_wire_()) {
    return;
  }
  this->si4703_registers_[POWERCFG] &= ~(1 << SEEK);
  this->update_registers_wire_();
  
  // Wait for STC clear
  for (int i = 0; i < 100; i++) {
    if (!this->read_registers_wire_()) {
      return;
    }
    if ((this->si4703_registers_[STATUSRSSI] & (1 << STC)) == 0) {
      break;
    }
    delay(10);
  }
}

float Si4703Component::get_frequency() {
  if (this->is_failed() || !this->read_registers_wire_()) {
    return 0.0;
  }
  
  int channel = this->si4703_registers_[READCHAN] & 0x03FF;
  return (channel + 875) / 10.0;
}

int Si4703Component::get_signal_strength() {
  if (this->is_failed() || !this->read_registers_wire_()) {
    return 0;
  }
  
  return this->si4703_registers_[STATUSRSSI] & 0x00FF;
}

bool Si4703Component::is_stereo() {
  if (this->is_failed() || !this->read_registers_wire_()) {
    return false;
  }
  
  return (this->si4703_registers_[STATUSRSSI] & (1 << STEREO)) != 0;
}

bool Si4703Component::has_rds_data() {
  if (this->is_failed() || !this->read_registers_wire_()) {
    return false;
  }
  // Check if RDS is ready
  return (this->si4703_registers_[STATUSRSSI] & (1 << RDSR)) != 0;
}

std::string Si4703Component::get_rds_station_name() {
  if (!this->has_rds_data()) {
    return "No RDS";
  }
  
  // RDS Program Service (PS) name - stored in groups 0A and 0B
  // This is a simplified version - full RDS parsing is complex
  static char ps_name[9] = {0};
  static int ps_index = 0;
  
  uint16_t rdsb = this->si4703_registers_[RDSB];
  uint16_t rdsc = this->si4703_registers_[RDSC];
  uint16_t rdsd = this->si4703_registers_[RDSD];
  
  // Extract group type from block B
  uint8_t group_type = (rdsb >> 12) & 0x0F;
  
  // Group 0A or 0B contains PS name
  if (group_type == 0) {
    // PS name position (2 characters at a time)
    uint8_t ps_pos = (rdsb & 0x03) * 2;
    
    if (ps_pos < 8) {
      // Extract 2 characters from block D
      ps_name[ps_pos] = (rdsd >> 8) & 0xFF;
      ps_name[ps_pos + 1] = rdsd & 0xFF;
      ps_name[8] = '\0';
      
      // Clean up non-printable characters
      for (int i = 0; i < 8; i++) {
        if (ps_name[i] < 32 || ps_name[i] > 126) {
          ps_name[i] = ' ';
        }
      }
    }
  }
  
  // Return the PS name if we have at least some characters
  if (ps_name[0] != 0) {
    return std::string(ps_name);
  }
  
  return "Unknown";
}

std::string Si4703Component::get_rds_text() {
  if (!this->has_rds_data()) {
    return "No RDS";
  }
  
  // RDS Radio Text (RT) - stored in groups 2A and 2B
  // This is a simplified version
  static char radio_text[65] = {0};
  
  uint16_t rdsb = this->si4703_registers_[RDSB];
  uint16_t rdsc = this->si4703_registers_[RDSC];
  uint16_t rdsd = this->si4703_registers_[RDSD];
  
  // Extract group type
  uint8_t group_type = (rdsb >> 12) & 0x0F;
  
  // Group 2A contains radio text
  if (group_type == 2) {
    // Text position (4 characters at a time for 2A)
    bool version_a = (rdsb & 0x0800) == 0;
    uint8_t text_pos = (rdsb & 0x0F) * (version_a ? 4 : 2);
    
    if (text_pos < 64) {
      if (version_a) {
        // Group 2A: 4 characters from C and D
        radio_text[text_pos] = (rdsc >> 8) & 0xFF;
        radio_text[text_pos + 1] = rdsc & 0xFF;
        radio_text[text_pos + 2] = (rdsd >> 8) & 0xFF;
        radio_text[text_pos + 3] = rdsd & 0xFF;
      } else {
        // Group 2B: 2 characters from D only
        radio_text[text_pos] = (rdsd >> 8) & 0xFF;
        radio_text[text_pos + 1] = rdsd & 0xFF;
      }
      
      // Clean up non-printable characters
      for (int i = text_pos; i < text_pos + (version_a ? 4 : 2) && i < 64; i++) {
        if (radio_text[i] < 32 || radio_text[i] > 126) {
          radio_text[i] = ' ';
        }
        // Check for end marker
        if (radio_text[i] == 0x0D) {
          radio_text[i] = '\0';
          break;
        }
      }
    }
  }
  
  // Return text if we have something
  if (radio_text[0] != 0) {
    std::string text(radio_text);
    // Trim trailing spaces
    size_t end = text.find_last_not_of(" ");
    if (end != std::string::npos) {
      text = text.substr(0, end + 1);
    }
    return text.empty() ? "No text" : text;
  }
  
  return "No text";
}

void Si4703Component::set_mute(bool mute) {
  if (this->is_failed() || !this->read_registers_wire_()) {
    return;
  }
  
  if (mute) {
    this->si4703_registers_[POWERCFG] &= ~(1 << DMUTE);
  } else {
    this->si4703_registers_[POWERCFG] |= (1 << DMUTE);
  }
  this->update_registers_wire_();
}

void Si4703Component::set_power(bool power) {
  ESP_LOGI(TAG, "=== set_power(%s) called ===", power ? "ON" : "OFF");
  
  if (this->is_failed()) {
    ESP_LOGW(TAG, "Cannot set power - component is failed");
    return;
  }
  
  if (!this->read_registers_wire_()) {
    ESP_LOGW(TAG, "Cannot set power - failed to read registers");
    return;
  }
  
  if (power) {
    ESP_LOGI(TAG, "Powering ON: Setting ENABLE + DMUTE");
    this->si4703_registers_[POWERCFG] |= (1 << ENABLE);   // Enable chip
    this->si4703_registers_[POWERCFG] |= (1 << DMUTE);    // Unmute (DMUTE=1 means unmuted)
  } else {
    ESP_LOGI(TAG, "Powering OFF: Clearing ENABLE + DMUTE + Setting SMUTE");
    this->si4703_registers_[POWERCFG] &= ~(1 << ENABLE);  // Disable chip (low power)
    this->si4703_registers_[POWERCFG] &= ~(1 << DMUTE);   // Digital mute (DMUTE=0 means muted)
    this->si4703_registers_[POWERCFG] |= (1 << SMUTE);    // Soft mute (extra muting)
  }
  
  if (!this->update_registers_wire_()) {
    ESP_LOGW(TAG, "Failed to write power registers");
    return;
  }
  
  delay(power ? 110 : 10);
  ESP_LOGI(TAG, "Power state changed to %s", power ? "ON" : "OFF");
}

void Si4703Component::update_rds_() {
  if (this->is_failed() || !this->read_registers_wire_()) {
    return;
  }
  
  // Check if RDS is ready
  if ((this->si4703_registers_[STATUSRSSI] & (1 << RDSR)) == 0) {
    return;  // No RDS data ready
  }
  
  // Read RDS blocks
  this->rds_blocks_[0] = this->si4703_registers_[RDSA];
  this->rds_blocks_[1] = this->si4703_registers_[RDSB];
  this->rds_blocks_[2] = this->si4703_registers_[RDSC];
  this->rds_blocks_[3] = this->si4703_registers_[RDSD];
  
  // Get group type from block B
  uint16_t block_b = this->rds_blocks_[1];
  uint8_t group_type = (block_b >> 12) & 0x0F;
  
  // Group 0 = Basic tuning and switching (station name)
  if (group_type == 0) {
    decode_rds_station_name_(this->rds_blocks_[1], this->rds_blocks_[3]);
  }
  // Group 2 = Radio text
  else if (group_type == 2) {
    decode_rds_text_(this->rds_blocks_[1], this->rds_blocks_[2], this->rds_blocks_[3]);
  }
}

void Si4703Component::decode_rds_station_name_(uint16_t block_b, uint16_t block_d) {
  // Extract character position (0-3)
  uint8_t char_pos = (block_b & 0x03) * 2;
  
  if (char_pos < 8) {
    // Extract two characters from block D
    this->rds_station_name_[char_pos] = (block_d >> 8) & 0xFF;
    this->rds_station_name_[char_pos + 1] = block_d & 0xFF;
    this->rds_station_name_[8] = '\0';  // Null terminator
    
    // Replace non-printable characters with spaces
    for (int i = 0; i < 8; i++) {
      if (this->rds_station_name_[i] < 32 || this->rds_station_name_[i] > 126) {
        this->rds_station_name_[i] = ' ';
      }
    }
  }
}

void Si4703Component::decode_rds_text_(uint16_t block_b, uint16_t block_c, uint16_t block_d) {
  // Get text segment address (0-15)
  uint8_t text_segment = block_b & 0x0F;
  uint8_t char_pos = text_segment * 4;
  
  if (char_pos < 64) {
    // Extract four characters from blocks C and D
    this->rds_text_[char_pos] = (block_c >> 8) & 0xFF;
    this->rds_text_[char_pos + 1] = block_c & 0xFF;
    this->rds_text_[char_pos + 2] = (block_d >> 8) & 0xFF;
    this->rds_text_[char_pos + 3] = block_d & 0xFF;
    this->rds_text_[64] = '\0';  // Null terminator
    
    // Replace non-printable characters with spaces
    for (int i = 0; i < 64; i++) {
      if (this->rds_text_[i] < 32 || this->rds_text_[i] > 126) {
        this->rds_text_[i] = ' ';
      }
    }
    
    // Check for end-of-text marker (0x0D)
    for (int i = 0; i < 64; i++) {
      if (this->rds_text_[i] == 0x0D) {
        this->rds_text_[i] = '\0';
        break;
      }
    }
  }
}


}  // namespace si4703
}  // namespace esphome