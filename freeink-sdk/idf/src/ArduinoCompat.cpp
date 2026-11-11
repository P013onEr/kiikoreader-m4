// FreeInk SDK — ESP-IDF backing for idf/include/Arduino.h and SPI.h.
// Only the ESP-IDF component builds this file (see CMakeLists.txt at the repo root).

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <esp_adc/adc_oneshot.h>
#include <driver/ledc.h>
#include <esp_attr.h>
#include <esp_log.h>
#include <stdarg.h>

static const char* TAG = "freeink";

HardwareSerial Serial, Serial0;
SPIClass SPI;
TwoWire Wire(I2C_NUM_0), Wire1(I2C_NUM_1);
static adc_oneshot_unit_handle_t s_adc1 = nullptr;
static adc_cali_handle_t s_adc1Cali = nullptr;
static adc_atten_t s_adcAttenuation = ADC_ATTEN_DB_12;

void analogSetAttenuation(int) { s_adcAttenuation = ADC_ATTEN_DB_12; }

static bool adcReadPin(uint8_t pin, int* raw) {
  if (!raw) return false;
  adc_unit_t unit;
  adc_channel_t channel;
  if (adc_oneshot_io_to_channel(pin, &unit, &channel) != ESP_OK || unit != ADC_UNIT_1) return false;
  if (!s_adc1) {
    adc_oneshot_unit_init_cfg_t init = {};
    init.unit_id = ADC_UNIT_1;
    if (adc_oneshot_new_unit(&init, &s_adc1) != ESP_OK) return false;
  }
  adc_oneshot_chan_cfg_t config = {};
  config.atten = s_adcAttenuation;
  config.bitwidth = ADC_BITWIDTH_DEFAULT;
  if (adc_oneshot_config_channel(s_adc1, channel, &config) != ESP_OK) return false;
  if (!s_adc1Cali) {
    adc_cali_curve_fitting_config_t cali = {};
    cali.unit_id = ADC_UNIT_1;
    cali.chan = channel;
    cali.atten = s_adcAttenuation;
    cali.bitwidth = ADC_BITWIDTH_DEFAULT;
    adc_cali_create_scheme_curve_fitting(&cali, &s_adc1Cali);
  }
  return adc_oneshot_read(s_adc1, channel, raw) == ESP_OK;
}

int analogRead(uint8_t pin) {
  int raw = 0;
  return adcReadPin(pin, &raw) ? raw : 0;
}

int analogReadMilliVolts(uint8_t pin) {
  int raw = 0;
  if (!adcReadPin(pin, &raw)) return 0;
  int millivolts = 0;
  return s_adc1Cali && adc_cali_raw_to_voltage(s_adc1Cali, raw, &millivolts) == ESP_OK
             ? millivolts : raw;
}

double ledcSetup(uint8_t channel, double frequency, uint8_t resolutionBits) {
  (void)channel;
  ledc_timer_config_t timer = {};
  timer.speed_mode = LEDC_LOW_SPEED_MODE;
  timer.duty_resolution = static_cast<ledc_timer_bit_t>(resolutionBits);
  timer.timer_num = LEDC_TIMER_0;
  timer.freq_hz = static_cast<uint32_t>(frequency);
  timer.clk_cfg = LEDC_AUTO_CLK;
  return ledc_timer_config(&timer) == ESP_OK ? frequency : 0;
}

bool ledcAttachPin(uint8_t pin, uint8_t channel) {
  ledc_channel_config_t config = {};
  config.gpio_num = static_cast<gpio_num_t>(pin);
  config.speed_mode = LEDC_LOW_SPEED_MODE;
  config.channel = static_cast<ledc_channel_t>(channel);
  config.intr_type = LEDC_INTR_DISABLE;
  config.timer_sel = LEDC_TIMER_0;
  return ledc_channel_config(&config) == ESP_OK;
}

void ledcWrite(uint8_t channel, uint32_t duty) {
  ledc_set_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(channel), duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, static_cast<ledc_channel_t>(channel));
}

void ledcDetachPin(uint8_t pin) {
  gpio_reset_pin(static_cast<gpio_num_t>(pin));
}

bool TwoWire::begin(int sda, int scl, uint32_t frequency) {
  if (bus_) return true;
  i2c_master_bus_config_t cfg = {};
  cfg.i2c_port = port_;
  cfg.sda_io_num = static_cast<gpio_num_t>(sda);
  cfg.scl_io_num = static_cast<gpio_num_t>(scl);
  cfg.clk_source = I2C_CLK_SRC_DEFAULT;
  cfg.glitch_ignore_cnt = 7;
  cfg.flags.enable_internal_pullup = true;
  if (i2c_new_master_bus(&cfg, &bus_) != ESP_OK) {
    bus_ = nullptr;
    return false;
  }
  frequency_ = frequency ? frequency : frequency_;
  return true;
}

void TwoWire::end() {
  if (device_) {
    i2c_master_bus_rm_device(device_);
    device_ = nullptr;
  }
  if (bus_) {
    i2c_del_master_bus(bus_);
    bus_ = nullptr;
  }
  tx_.clear();
  rx_.clear();
  rxAt_ = 0;
}

void TwoWire::setClock(uint32_t frequency) { frequency_ = frequency; }
void TwoWire::setTimeOut(uint16_t timeoutMs) { timeoutMs_ = timeoutMs; }
void TwoWire::beginTransmission(uint8_t address) {
  address_ = address;
  if (device_ && deviceAddress_ != address) {
    i2c_master_bus_rm_device(device_);
    device_ = nullptr;
  }
  tx_.clear();
}
size_t TwoWire::write(uint8_t data) {
  tx_.push_back(data);
  return 1;
}
size_t TwoWire::write(const uint8_t* data, size_t length) {
  if (data && length) tx_.insert(tx_.end(), data, data + length);
  return length;
}
uint8_t TwoWire::endTransmission(bool) {
  if (!bus_) return 4;
  if (!device_) {
    i2c_device_config_t cfg = {};
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.device_address = address_;
    cfg.scl_speed_hz = frequency_;
    if (i2c_master_bus_add_device(bus_, &cfg, &device_) != ESP_OK) return 4;
    deviceAddress_ = address_;
  }
  return i2c_master_transmit(device_, tx_.data(), tx_.size(), timeoutMs_) == ESP_OK ? 0 : 4;
}
uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity, uint8_t) {
  if (!bus_) return 0;
  address_ = address;
  if (device_ && deviceAddress_ != address) {
    i2c_master_bus_rm_device(device_);
    device_ = nullptr;
  }
  if (!device_) {
    i2c_device_config_t cfg = {};
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.device_address = address_;
    cfg.scl_speed_hz = frequency_;
    if (i2c_master_bus_add_device(bus_, &cfg, &device_) != ESP_OK) return 0;
    deviceAddress_ = address_;
  }
  rx_.assign(quantity, 0);
  rxAt_ = 0;
  return i2c_master_receive(device_, rx_.data(), quantity, timeoutMs_) == ESP_OK ? quantity : 0;
}
int TwoWire::read() {
  return rxAt_ < rx_.size() ? rx_[rxAt_++] : -1;
}

int HardwareSerial::printf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  const int n = vprintf(fmt, ap);
  va_end(ap);
  return n;
}

void pinMode(uint8_t pin, uint8_t mode) {
  gpio_config_t cfg = {};
  cfg.pin_bit_mask = 1ULL << pin;
  // Outputs keep their input enabled so digitalRead() reads back the level, as on Arduino.
  cfg.mode = (mode & OUTPUT) == OUTPUT ? ((mode & OPEN_DRAIN) ? GPIO_MODE_INPUT_OUTPUT_OD : GPIO_MODE_INPUT_OUTPUT)
                                       : GPIO_MODE_INPUT;
  cfg.pull_up_en = (mode & PULLUP) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
  cfg.pull_down_en = (mode & PULLDOWN) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
  cfg.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&cfg);
}

static void IRAM_ATTR isrTrampoline(void* arg) { reinterpret_cast<void (*)(void)>(arg)(); }

void attachInterrupt(uint8_t pin, void (*isr)(void), int mode) {
  const gpio_num_t gpio = static_cast<gpio_num_t>(pin);
  static bool serviceReady = false;
  if (!serviceReady) {
    const esp_err_t serviceErr = gpio_install_isr_service(0);
    if (serviceErr != ESP_OK && serviceErr != ESP_ERR_INVALID_STATE) {
      ESP_LOGE(TAG, "gpio_install_isr_service: %s", esp_err_to_name(serviceErr));
      return;
    }
    serviceReady = true;
  }
  gpio_set_intr_type(gpio, mode == RISING    ? GPIO_INTR_POSEDGE
                           : mode == FALLING ? GPIO_INTR_NEGEDGE
                                             : GPIO_INTR_ANYEDGE);
  // The service may already be installed by another component; INVALID_STATE
  // is expected there, after which adding this pin handler remains valid.
  esp_err_t err = gpio_isr_handler_add(gpio, isrTrampoline, reinterpret_cast<void*>(isr));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "attachInterrupt(%d): %s", pin, esp_err_to_name(err));
    return;
  }
  gpio_intr_enable(gpio);
}

void detachInterrupt(uint8_t pin) {
  const gpio_num_t gpio = static_cast<gpio_num_t>(pin);
  gpio_intr_disable(gpio);
  gpio_isr_handler_remove(gpio);
  gpio_set_intr_type(gpio, GPIO_INTR_DISABLE);
}

// ── SPI ──────────────────────────────────────────────────────────────────────

static constexpr spi_host_device_t kHost = SPI2_HOST;
static constexpr size_t kChunk = 4092;  // one DMA descriptor's worth
static uint8_t* s_dma;                  // bounce buffer: callers pass PSRAM/flash data

void SPIClass::begin(int8_t sck, int8_t miso, int8_t mosi, int8_t) {
  if (!s_dma) s_dma = static_cast<uint8_t*>(heap_caps_malloc(kChunk, MALLOC_CAP_DMA));
  spi_bus_config_t bus = {};
  bus.sclk_io_num = sck;
  bus.miso_io_num = miso;
  bus.mosi_io_num = mosi;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = kChunk;
  const esp_err_t err = spi_bus_initialize(kHost, &bus, SPI_DMA_CH_AUTO);
  _ownsBus = err == ESP_OK;
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {  // INVALID_STATE: firmware owns the bus
    ESP_LOGE(TAG, "spi_bus_initialize: %s", esp_err_to_name(err));
  }
}

void SPIClass::end() {
  if (_dev) spi_bus_remove_device(_dev);
  _dev = nullptr;
  if (_ownsBus) spi_bus_free(kHost);
  _ownsBus = false;
}

void SPIClass::beginTransaction(const SPISettings& s) {
  if (_dev && (s.clock != _settings.clock || s.dataMode != _settings.dataMode)) {
    spi_bus_remove_device(_dev);
    _dev = nullptr;
  }
  if (!_dev) {
    spi_device_interface_config_t dev = {};
    dev.mode = s.dataMode;
    dev.clock_speed_hz = static_cast<int>(s.clock);
    dev.spics_io_num = -1;  // the drivers drive CS themselves
    dev.queue_size = 1;
    dev.flags = s.bitOrder == LSBFIRST ? SPI_DEVICE_BIT_LSBFIRST : 0;
    const esp_err_t err = spi_bus_add_device(kHost, &dev, &_dev);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "spi_bus_add_device: %s", esp_err_to_name(err));
      _dev = nullptr;
      return;
    }
    _settings = s;
  }
  spi_device_acquire_bus(_dev, portMAX_DELAY);  // keeps other devices (SD) off the bus
}

void SPIClass::endTransaction() {
  if (_dev) spi_device_release_bus(_dev);
}

uint8_t SPIClass::transfer(uint8_t data) {
  if (!_dev) return 0;
  spi_transaction_t t = {};
  t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
  t.length = 8;
  t.tx_data[0] = data;
  spi_device_polling_transmit(_dev, &t);
  return t.rx_data[0];
}

void SPIClass::writeBytes(const uint8_t* data, uint32_t size) {
  if (!_dev || !s_dma) return;
  while (size) {
    const size_t n = size < kChunk ? size : kChunk;
    memcpy(s_dma, data, n);
    spi_transaction_t t = {};
    t.length = n * 8;
    t.tx_buffer = s_dma;
    spi_device_polling_transmit(_dev, &t);
    data += n;
    size -= n;
  }
}
