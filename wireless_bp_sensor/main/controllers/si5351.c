#include "si5351.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "si5351";

#define XTAL 25000 //25 MHz, units in kHz 
#define PLLA_FREQUENCY 800000
#define MIN_FREQUENCY 1000
#define MAX_FREQUENCY 150000

#define SCL_PIN 23
#define SDA_PIN 22
#define I2C_FREQUENCY 200000
#define SI5351_SLAVE_ADDRESS 0x60

static uint32_t si5351_current_frequency = 0; //so skeer.

i2c_master_dev_handle_t si5351_handle;

//function defs
static int si5351_write_register(uint8_t reg, uint8_t value);
static int si5351_disable(void);
static int si5351_enable(void);
static int si5351_config_plla(void);
static int si5351_write_mso(uint32_t target_frequency);
static int si5351_config_clk0(void);
static int si5351_config_load_capacitance(void);
static int si5351_wait_lock(void);
static int si5351_plla_xtal(void);
static int si5351_blast_write(uint8_t start_register, uint8_t* write_buffer, uint8_t write_buffer_length);

int si5351_init(void) {
  esp_err_t ret;

  i2c_master_bus_config_t i2c_mst_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = I2C_NUM_0,
    .scl_io_num = SCL_PIN,
    .sda_io_num = SDA_PIN,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
  };

  i2c_master_bus_handle_t bus_handle;
  ret = i2c_new_master_bus(&i2c_mst_config, &bus_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "i2c bus create failed");
    return -1;
  }

  i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = SI5351_SLAVE_ADDRESS,
    .scl_speed_hz = I2C_FREQUENCY,
  };

  ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &si5351_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "i2c device add failed");
    return -1;
  }

  if (si5351_disable() < 0) return -1;
  if (si5351_config_clk0() < 0) return -1;
  if (si5351_config_load_capacitance() < 0) return -1;
  if (si5351_plla_xtal() < 0) return -1;
  if (si5351_config_plla() < 0) return -1;
  if (si5351_wait_lock() < 0) return -1;

  return 0;
}

int si5351_set_frequency(uint32_t target_frequency) {
  if (target_frequency < MIN_FREQUENCY || target_frequency > MAX_FREQUENCY) {
    ESP_LOGE(TAG, "Set frequency out of bounds");
    return -1;
  }

  if (si5351_disable() < 0) return -1;
  if (si5351_write_mso(target_frequency) < 0) return -1;
  if (si5351_write_register(177, 0x20) < 0) return -1;
  if (si5351_wait_lock() < 0) return -1;
  if (si5351_enable() < 0) return -1;

  si5351_current_frequency = target_frequency;
  return 0;
}

int si5351_sweep_frequencies(sweep_config_t sweep_config) {
  uint32_t current_frequency = sweep_config.start_frequency;

  while (current_frequency <= sweep_config.stop_frequency) {
    if (current_frequency > MAX_FREQUENCY) {
      current_frequency = MAX_FREQUENCY;
    } else if (current_frequency < MIN_FREQUENCY) {
      current_frequency = MIN_FREQUENCY;
    }

    if (si5351_set_frequency(current_frequency) < 0) return -1;

    current_frequency += sweep_config.step_size;
    vTaskDelay(pdMS_TO_TICKS(sweep_config.hold_ms));
  }

  //ESP_LOGI(TAG, "Frequency sweep complete");
  return 0;
}

uint32_t si5351_get_frequency(void) {
  return si5351_current_frequency;
}

static int si5351_write_register(uint8_t reg, uint8_t value) {
  uint8_t tx[2] = { reg, value };
  esp_err_t ret = i2c_master_transmit(si5351_handle, tx, sizeof(tx), -1);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "I2C write failed at reg 0x%02X (err=0x%x)", reg);
    return -1;
  }
  return 0;
}

static int si5351_blast_write(uint8_t start_register, uint8_t* write_buffer, uint8_t write_buffer_length) {
  uint8_t current_register = start_register;
  for (int buffer_index = 0; buffer_index < write_buffer_length; buffer_index++) {
    if (si5351_write_register(current_register, write_buffer[buffer_index]) < 0) return -1;
    current_register++;
  }
  return 0;
}

static int si5351_disable(void) {
  return si5351_write_register(0x03, 0xFF);
}

static int si5351_enable(void) {
  return si5351_write_register(0x03, 0xFE);
}

static int si5351_wait_lock(void) {
  uint8_t status;
  for (int i = 0; i < 50; i++) {
    uint8_t reg = 0x00;
    esp_err_t ret = i2c_master_transmit_receive(si5351_handle, &reg, 1, &status, 1, -1);
    if (ret != ESP_OK) return -1;
    if ((status & (1 << 5)) == 0) {
      return 0;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  ESP_LOGW(TAG, "PLLA failed to lock within timeout");
  return -1;
}

static int si5351_config_plla(void) {
  const int32_t mult = 32;
  const int32_t num = 0;
  const int32_t denom = 1;

  int32_t P1 = 128 * mult + (128 * num) / denom - 512;
  int32_t P2 = (128 * num) % denom;
  int32_t P3 = denom;

  uint8_t baseaddr = 26;
  uint8_t buf[8];

  buf[0] = (uint8_t)((P3 >> 8) & 0xFF);
  buf[1] = (uint8_t)(P3 & 0xFF);
  buf[2] = (uint8_t)(((P1 >> 16) & 0x03) | ((0 & 0x3) << 2) | ((0 & 0x7) << 4));
  buf[3] = (uint8_t)((P1 >> 8) & 0xFF);
  buf[4] = (uint8_t)(P1 & 0xFF);
  buf[5] = (uint8_t)(((P3 >> 12) & 0xF0) | ((P2 >> 16) & 0x0F));
  buf[6] = (uint8_t)((P2 >> 8) & 0xFF);
  buf[7] = (uint8_t)(P2 & 0xFF);

  if (si5351_blast_write(baseaddr, buf, sizeof(buf)) < 0) return -1;
  if (si5351_write_register(177, (1 << 7) | (1 << 5)) < 0) return -1;

  return 0;
}

static int si5351_write_mso(uint32_t f_khz) {
  const uint32_t f_pll_khz = PLLA_FREQUENCY;
  uint32_t a = f_pll_khz / f_khz;
  uint32_t rem = f_pll_khz % f_khz;
  uint32_t divBy4 = 0, rdiv = 0;
  uint32_t P1, P2, P3;

  if (a == 4 && rem == 0) {
    P1 = 0; P2 = 0; P3 = 1; divBy4 = 0x3;
  } else {
    const uint32_t c = 1048575;
    uint64_t b = ((uint64_t)rem * c) / f_khz;
    P3 = c;
    uint64_t t = (128ULL * b) / c;
    P1 = 128U * a + (uint32_t)t - 512U;
    P2 = (uint32_t)(128ULL * b - t * c);
  }

  uint8_t buf[8];
  buf[0] = (P3 >> 8) & 0xFF;
  buf[1] = (P3) & 0xFF;
  buf[2] = ((P1 >> 16) & 0x03) | ((divBy4 & 0x3) << 2) | ((rdiv & 0x7) << 4);
  buf[3] = (P1 >> 8) & 0xFF;
  buf[4] = (P1) & 0xFF;
  buf[5] = ((P3 >> 12) & 0xF0) | ((P2 >> 16) & 0x0F);
  buf[6] = (P2 >> 8) & 0xFF;
  buf[7] = (P2) & 0xFF;

  if (si5351_blast_write(42, buf, sizeof(buf)) < 0) return -1;
  return 0;
}

static int si5351_config_clk0(void) {
  return si5351_write_register(16, 0x0F);
}

static int si5351_config_load_capacitance(void) {
  return si5351_write_register(183, 0xC0);
}

static int si5351_plla_xtal(void) {
  return si5351_write_register(15, 0x00);
}

