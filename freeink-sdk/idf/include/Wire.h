#pragma once

#if defined(ARDUINO)
#error "FreeInk's ESP-IDF Arduino shim must not be built with arduino-esp32; use the real core's headers."
#endif

// FreeInk SDK — bounded Arduino Wire compatibility over the IDF master driver.

#include <Arduino.h>
#include <driver/i2c_master.h>
#include <vector>

class TwoWire {
 public:
  explicit TwoWire(i2c_port_num_t port = I2C_NUM_0) : port_(port) {}
  bool begin(int sda = -1, int scl = -1, uint32_t frequency = 0);
  void end();
  void setClock(uint32_t frequency);
  void setTimeOut(uint16_t timeoutMs);
  void beginTransmission(uint8_t address);
  size_t write(uint8_t data);
  size_t write(const uint8_t* data, size_t length);
  uint8_t endTransmission(bool sendStop = true);
  uint8_t requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop = true);
  int available() const { return static_cast<int>(rx_.size() - rxAt_); }
  int read();

 private:
  i2c_port_num_t port_;
  i2c_master_bus_handle_t bus_ = nullptr;
  i2c_master_dev_handle_t device_ = nullptr;
  uint32_t frequency_ = 100000;
  uint16_t timeoutMs_ = 50;
  uint8_t address_ = 0;
  uint8_t deviceAddress_ = 0xFF;
  std::vector<uint8_t> tx_;
  std::vector<uint8_t> rx_;
  size_t rxAt_ = 0;
};
extern TwoWire Wire;
extern TwoWire Wire1;
