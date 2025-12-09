#include <string.h>
#include <math.h>
#include <inttypes.h>
#include "adf4351.h"
#include "sweep_configs.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_err.h"

static const char *TAG = "adf4351";

#define PIN_HIGH 1
#define PIN_LOW  0
#define REF_OSC 25000
#define BCSD 80          // fPFD/300kHz; keep your value
#define LOCK_HOLD_MS 50  // keep your value

#define R4_RF_DIVIDER_POS    (20)
#define R4_RF_DIVIDER_MASK   (0b111 << R4_RF_DIVIDER_POS)
#define R1_RF_PRESCALER_POS  (27)
#define R1_RF_PRESCALER_MASK (0b1 << R1_RF_PRESCALER_POS)
#define R0_RF_INT_POS        (15)
#define R0_RF_INT_MASK       (((1<<16) - 1) << R0_RF_INT_POS)

#define MAX_FREQ 550000
#define MIN_FREQ 35000

//gpio defs
#define PIN_NUM_MOSI  18    // example
#define PIN_NUM_MISO  -1   // ADF4351 doesn't need MISO
#define PIN_NUM_CLK   19    // example
#define PIN_NUM_LE    22    // LE (latch) to ADF4351
#define PIN_NUM_LD    23   // LD (lock detect) from ADF4351

#define ADF_SPI_HOST SPI2_HOST

static spi_device_handle_t s_adf_spi = NULL;
adf4351_status_t adf4351_status;

//helper macros
#define GET_UPTIME_MS() (uint32_t)(esp_timer_get_time() / 1000ULL)


//function decs
static esp_err_t adf4351_spi_init(void);
static esp_err_t adf4351_gpio_init(void);
static int adf4351_write_register(uint32_t reg_value);
static void wait_band_select_us(void);
static int wait_for_lock_ld(uint32_t timeout_ms);
static int update_rf_divider(adf4351_status_t* updated);
static int update_rf_prescaler(adf4351_status_t* updated);
static int update_rf_freq(adf4351_status_t* updated);
static int calc_adf4351_params(uint32_t frequency, adf4351_status_t* updated);

static esp_err_t adf4351_spi_init(void) {
    spi_bus_config_t busconfig = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4, //only need 4 bytes per reg
        .flags = 0,
        .intr_flags = 0
    };

    ESP_RETURN_ON_ERROR(spi_bus_initialize(ADF_SPI_HOST, &busconfig, SPI_DMA_CH_AUTO), TAG, "spi_bus_initialize failed");

    spi_device_interface_config_t deviceconfig = {
        .clock_speed_hz = 1000000, //1 MHz
        .mode = 0,                 //spi mode zero
        .spics_io_num = -1,                //no cs
        .queue_size = 1,
        .flags = 0,
        .input_delay_ns = 0
    };

    ESP_RETURN_ON_ERROR(spi_bus_add_device(ADF_SPI_HOST, &deviceconfig, &s_adf_spi), TAG, "spi_bus_add_device failed");
    return ESP_OK;
}

static esp_err_t adf4351_gpio_init(void) {
    gpio_config_t le_btn_config = {
        .pin_bit_mask = (1ULL << PIN_NUM_LE),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&le_btn_config), TAG, "LE gpio_config failed");

    gpio_config_t ld_btn_config = {
        .pin_bit_mask = (1ULL << PIN_NUM_LD),
        .mode = GPIO_MODE_INPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&ld_btn_config), TAG, "LE gpio_config failed");

    return ESP_OK;
}

int adf4351_init(void) {
    if (adf4351_gpio_init() != ESP_OK) {
        ESP_LOGE(TAG, "GPIO not ready");
        return -1;
    }
    if (adf4351_spi_init() != ESP_OK) {
        ESP_LOGE(TAG, "SPI not ready");
        return -1;
    }

    //Blast write all 6 registers on startup
    uint32_t register_values[6] = {
        0x00400005, //R5
        0x00E5043C, //R4 -> adjusts power and VCO divider
        0x00E49F43, //R3
        0x18005FC2, //R2
        0x000083E9, //R1 -> adjusts prescaler and MOD
        0x00400000  //R0 -> adjusts INT and FRAC
    };

    int err;
    for (int reg = 0; reg<6; reg++) {
        err = adf4351_write_register(register_values[reg]);
        if (err < 0) {
            ESP_LOGE(TAG, "Setup blast write error\n");
            return -1;
        }
    }

    wait_band_select_us();
    if (wait_for_lock_ld(LOCK_HOLD_MS) < 0) {
        ESP_LOGE(TAG,"Loop lock timed out");
        return -1;
    }

    //store status of chip, 
    adf4351_status.r4 = 0x00E1403C;
    adf4351_status.r2 = 0x18004FC2;
    adf4351_status.r1 = 0x000083E9;
    adf4351_status.r0 = 0x00400000;
    adf4351_status.current_freq_kHz = 50000;
    adf4351_status.div = 64;
    adf4351_status.ps = 4;
    adf4351_status.INT = 128;


    ESP_LOGI(TAG,"adf4351 setup complete\n");

    return 0;
}

static int adf4351_write_register(uint32_t reg_value) {
    uint8_t tx_buffer[4];

    tx_buffer[0] = (reg_value >> 24) & 0xFF; //MSB
    tx_buffer[1] = (reg_value >> 16) & 0xFF;
    tx_buffer[2] = (reg_value >> 8)  & 0xFF;
    tx_buffer[3] = (reg_value >> 0)  & 0xFF; //LSB

    gpio_set_level(PIN_NUM_LE, PIN_HIGH); //make sure pin is high for a short while before sending write
    ets_delay_us(1);

    gpio_set_level(PIN_NUM_LE, PIN_LOW);

    spi_transaction_t t = {0};
    t.length    = 8 * sizeof(tx_buffer); // bits
    t.tx_buffer = tx_buffer;
    esp_err_t err = spi_device_transmit(s_adf_spi, &t);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI write error (%d)", (int)err);
        return -1;
    }

    // LE high -> latch
    gpio_set_level(PIN_NUM_LE, PIN_HIGH);
    return 0;
}

static void wait_band_select_us(void) {
    uint32_t t_us = (uint32_t)((10.0 * BCSD * 1e6) /(double)REF_OSC);
    ets_delay_us(t_us); //blocking but whatever 
}

static int wait_for_lock_ld(uint32_t timeout_ms)
{
    uint32_t start = GET_UPTIME_MS(); //uptime in ms
    while (1) {
        int val = gpio_get_level(PIN_NUM_LD);
        if (val < 0) {
            ESP_LOGE(TAG, "LD pin read error");
            return -1;
        }
        if (val == 1) {
            return 0; // locked
        }
        if ((GET_UPTIME_MS() - start) > timeout_ms) {
            return -1; // timed out
        }
        ets_delay_us(10);
    }
}
static int calc_adf4351_params(uint32_t frequency, adf4351_status_t* updated_adf4351_status) {
    uint16_t div, prescaler;
    if (35000 <= frequency && frequency < 56250) {
        div = 64;
        prescaler = 4;
    }
    else if (56250 <= frequency && frequency< 68750) {
        div = 64;
        prescaler = 8;
    }
    else if (68750 <= frequency && frequency < 112500) {
        div = 32;
        prescaler = 4;
    }
    else if (112500 <= frequency && frequency < 137500) {
        div = 32;
        prescaler = 8;
    }
    else if (137500 <= frequency && frequency < 225000) {
        div = 16;
        prescaler = 4;
    }
    else if (225000 <= frequency && frequency < 275000) {
        div = 16;
        prescaler = 8;
    }
    else if (275000 <= frequency && frequency < 450000) {
        div = 8;
        prescaler = 4;
    }
    else if (450000 <= frequency && frequency < 550000) {
        div = 8;
        prescaler = 8;
    }
    else {
        ESP_LOGE(TAG, "Frequency out of bounds in adf4351_set_frequency()\n");
        return -1; 
    }
    
    uint16_t INT = (uint16_t)(( (uint64_t)frequency * div + REF_OSC/2 ) / REF_OSC); //round dont floor
    
    updated_adf4351_status->div = div;
    updated_adf4351_status->ps = prescaler;
    updated_adf4351_status->INT = INT;

    uint32_t rf_programmed = (REF_OSC*INT)/div;
    updated_adf4351_status->current_freq_kHz = rf_programmed; //puts actual freq as seen by pll in struct. Rounds user's input

    return 0;

}

int adf4351_set_frequency(uint32_t frequency) {
    adf4351_status_t updated_adf4351_status = adf4351_status; //initially copy the global state to preserve r0, r1, r4 etc
    
    int ret;
    ret = calc_adf4351_params(frequency, &updated_adf4351_status);
    if (ret < 0) {
        ESP_LOGE(TAG, "Frequency out of bounds, adf4351_set_frequency()");
        return -1;
    }
    
    if (updated_adf4351_status.div != adf4351_status.div) { //write to div if changed
        ret = update_rf_divider(&updated_adf4351_status);
        if (ret < 0) {
            return -1;
        }
    }

    if (updated_adf4351_status.ps != adf4351_status.ps) { //write to prescaler if changed 
        ret = update_rf_prescaler(&updated_adf4351_status);
        if (ret < 0) {
            return -1;
        }
    }

    ret = update_rf_freq(&updated_adf4351_status); //always write new INT
    if (ret < 0) {
            return -1;
    }

    adf4351_status = updated_adf4351_status;

    wait_band_select_us(); //
    if (wait_for_lock_ld(LOCK_HOLD_MS) < 0) {
        ESP_LOGE(TAG, "Loop lock timed out");
        return -1;
    }

    ESP_LOGI(TAG, "adf4351 frequency updated");
    return 0;

}


static int update_rf_divider(adf4351_status_t* updated_adf4351_status) {
    uint8_t div_setting;
    switch (updated_adf4351_status->div) {
        case 8: div_setting =  0b011; break;
        case 16: div_setting = 0b100; break; //added div = 16
        case 32: div_setting = 0b101; break;
        case 64: div_setting = 0b110; break;
        default: return -1; // Invalid divider
    }

    updated_adf4351_status->r4 &= ~R4_RF_DIVIDER_MASK;
    updated_adf4351_status->r4 |= (div_setting << R4_RF_DIVIDER_POS);
    int ret = adf4351_write_register(updated_adf4351_status->r4);
    if (ret < 0) {
        ESP_LOGE(TAG, "Error updating rf divider");
        return -1;
    }
    return 0;
}

static int update_rf_prescaler(adf4351_status_t* updated_adf4351_status) {
    uint8_t prescaler_setting;
    switch(updated_adf4351_status->ps) {
        case 4: prescaler_setting = 0b0; break;
        case 8: prescaler_setting = 0b1; break;
        default: return -1; //Invalid prescaler
    }
    updated_adf4351_status->r1 &= ~R1_RF_PRESCALER_MASK;
    updated_adf4351_status->r1 |= (prescaler_setting << R1_RF_PRESCALER_POS);
    int ret = adf4351_write_register(updated_adf4351_status->r1);
    if (ret < 0) {
        ESP_LOGE(TAG, "Error updating rf prescaler");
        return -1;
    }
    return 0;
}

static int update_rf_freq(adf4351_status_t* updated_adf4351_status) {
    updated_adf4351_status->r0 &= ~R0_RF_INT_MASK;
    updated_adf4351_status->r0 |= (updated_adf4351_status->INT << R0_RF_INT_POS);

    int ret = adf4351_write_register(updated_adf4351_status->r0);
    if (ret < 0) {
        ESP_LOGE(TAG, "Error updating rf frequency");
        return -1;
    }
    return 0;
}


int adf4351_sweep_frequencies(sweep_config_t sweep_config) {
    uint32_t current_frequency = sweep_config.start_frequency;
    
    int ret;
    while (current_frequency <= sweep_config.stop_frequency) {
        if (current_frequency > MAX_FREQ){
            current_frequency = MAX_FREQ;
        } 
        else if (current_frequency < MIN_FREQ) {
            current_frequency = MIN_FREQ;
        }

        ret = adf4351_set_frequency(current_frequency);
        if (ret < 0) {
            ESP_LOGE(TAG, "Frequency sweep failed");
            return -1;
        }

        current_frequency += sweep_config.step_size;
         vTaskDelay(pdMS_TO_TICKS(sweep_config.hold_ms)); //manual delay at each freq, hold for lock, band select seperate
    }
    ESP_LOGI(TAG, "Frequency sweep complete");

    return 0;
}

