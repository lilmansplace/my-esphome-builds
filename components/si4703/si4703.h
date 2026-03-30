#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include <Wire.h>  // Use Arduino Wire library directly

namespace esphome {
namespace si4703 {

// Si4703 Register Definitions
static const uint8_t SI4703_ADDRESS = 0x10;

// Register names
static const uint8_t DEVICEID = 0x00;
static const uint8_t SI4703_CHIPID = 0x01;  // Renamed to avoid ESP8266 conflict
static const uint8_t POWERCFG = 0x02;
static const uint8_t CHANNEL = 0x03;
static const uint8_t SYSCONFIG1 = 0x04;
static const uint8_t SYSCONFIG2 = 0x05;
static const uint8_t SYSCONFIG3 = 0x06;
static const uint8_t TEST1 = 0x07;
static const uint8_t TEST2 = 0x08;
static const uint8_t BOOTCONFIG = 0x09;
static const uint8_t STATUSRSSI = 0x0A;
static const uint8_t READCHAN = 0x0B;
static const uint8_t RDSA = 0x0C;
static const uint8_t RDSB = 0x0D;
static const uint8_t RDSC = 0x0E;
static const uint8_t RDSD = 0x0F;

// Register 0x02 - POWERCFG
static const uint8_t SMUTE = 15;
static const uint8_t DMUTE = 14;
static const uint8_t SKMODE = 10;
static const uint8_t SEEKUP = 9;
static const uint8_t SEEK = 8;
static const uint8_t ENABLE = 0;

// Register 0x03 - CHANNEL
static const uint8_t TUNE = 15;

// Register 0x04 - SYSCONFIG1
static const uint8_t RDS = 12;
static const uint8_t DE = 11;

// Register 0x05 - SYSCONFIG2
static const uint8_t SPACE1 = 5;
static const uint8_t SPACE0 = 4;
static const uint8_t VOLUME = 0;

// Register 0x0A - STATUSRSSI
static const uint8_t RDSR = 15;
static const uint8_t STC = 14;
static const uint8_t SFBL = 13;
static const uint8_t AFCRL = 12;
static const uint8_t RDSS = 11;
static const uint8_t STEREO = 8;

class Si4703Component : public Component, public i2c::I2CDevice {
 public:
  Si4703Component();
  
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_reset_pin(GPIOPin *pin) { 
    this->reset_pin_ = pin;
  }

  // Control functions
  void set_frequency(float frequency);
  void set_volume(int volume);
  void seek_up();
  void seek_down();
  void set_mute(bool mute);
  void set_power(bool power);
  
  // Query functions
  float get_frequency();
  int get_signal_strength();
  bool is_stereo();
  
  // RDS functions
  std::string get_rds_station_name();
  std::string get_rds_text();
  bool has_rds_data();

 protected:
  GPIOPin *reset_pin_{nullptr};
  uint16_t si4703_registers_[16];
  
  // I2C pin storage for Wire.begin()
  int8_t sda_pin_{-1};
  int8_t scl_pin_{-1};
  
  // RDS data storage
  char rds_station_name_[9] = {0};  // 8 chars + null terminator
  char rds_text_[65] = {0};         // 64 chars + null terminator
  uint16_t rds_blocks_[4] = {0};    // RDS block storage
  
  // Arduino Wire library methods
  bool read_registers_wire_();
  bool update_registers_wire_();
  void wait_for_seek_();
  
  // RDS parsing helpers
  void update_rds_();
  void decode_rds_station_name_(uint16_t block_b, uint16_t block_d);
  void decode_rds_text_(uint16_t block_b, uint16_t block_c, uint16_t block_d);
  
  // Helper to initialize Wire with correct pins
  void init_wire_();
};

}  // namespace si4703
}  // namespace esphome