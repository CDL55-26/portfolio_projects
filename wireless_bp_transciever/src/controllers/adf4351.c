#include "adf4351.h"
#include <math.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/time_units.h>

LOG_MODULE_REGISTER(adf4351, LOG_LEVEL_DBG); //create log module called adf4351

//Macros
#define USER_NODE DT_PATH(zephyr_user)
#define HIGH 1
#define LOW 0
#define REF_OSC 25000
#define BCSD 80 //this might change if we change pfd BSCD should be fPFD / 300kHz

#define LOCK_HOLD_MS 50 //should be much less

#define R4_RF_DIVIDER_POS    (20) //starting bit pos of divider 
#define R4_RF_DIVIDER_MASK   (0b111 << R4_RF_DIVIDER_POS)
#define R1_RF_PRESCALER_POS  (27)
#define R1_RF_PRESCALER_MASK (0b1 << R1_RF_PRESCALER_POS)
#define R0_RF_INT_POS        (15)
#define R0_RF_INT_MASK       (((1<<16) - 1) << R0_RF_INT_POS)

#define MAX_FREQ 250000
#define MIN_FREQ 35000

//Hardware config
static const struct device* spi_dev = DEVICE_DT_GET(DT_ALIAS(adf4351spi));
static const struct gpio_dt_spec le_pin = GPIO_DT_SPEC_GET(USER_NODE, adf4351_le_gpios);
static const struct gpio_dt_spec ld_pin = GPIO_DT_SPEC_GET(USER_NODE, adf4351_ld_gpios);

static const struct spi_config spi_cfg = {
    .frequency = 1000000, //1Mhz -> can go up to 20Mhz i think 
    .operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
    .slave = 0, //not a slave device
    .cs = NULL, //not using cs, manual control of LE
};

//chip control struct
typedef struct {
    uint8_t div;
    uint8_t ps;
    uint16_t INT;
    uint16_t pfd;
    uint32_t current_freq_kHz;
    uint32_t r4;
    uint32_t r2;
    uint32_t r1;
    uint32_t r0;
}adf4351_status_t;
static adf4351_status_t adf4351_status;



//Helper functions 
int adf4351_write_register(uint32_t reg_value);
int update_rf_divider(adf4351_status_t* updated_adf4351_status);
int update_rf_prescaler(adf4351_status_t* updated_adf4351_status);
int update_rf_freq(adf4351_status_t* updated_adf4351_status);
int calc_adf4351_params(uint32_t frequency, adf4351_status_t* updated_adf4351_status);
void wait_band_select_us(void);
int wait_for_lock_ld(uint32_t timeout_us);

int adf4351_init(void) {
    if (!device_is_ready(spi_dev)) {
        LOG_ERR("SPI device not ready\n");
        return -1;
    }
    if(!gpio_is_ready_dt(&le_pin)) {
        LOG_ERR("LE pin not ready\n");
        return -1;
    }

    int ret = gpio_pin_configure_dt(&le_pin, GPIO_OUTPUT_INACTIVE); 
    if (ret) {
        LOG_ERR("Failed to configure LE pin\n");
        return -1;
    }
    ret = gpio_pin_configure_dt(&ld_pin, GPIO_INPUT);
    if (ret) {
        LOG_ERR("Failed to configure LE pin\n");
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
            LOG_ERR("Setup blast write error\n");
            return -1;
        }
    }

    wait_band_select_us();
    if (wait_for_lock_ld(LOCK_HOLD_MS) < 0) {
        LOG_ERR("Loop lock timed out");
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


    LOG_INF("adf4351 setup complete\n");

    return 0;
}

int adf4351_write_register(uint32_t reg_value) {
    uint8_t tx_buffer[4];

    tx_buffer[0] = (reg_value >> 24) & 0xFF; //MSB
    tx_buffer[1] = (reg_value >> 16) & 0xFF;
    tx_buffer[2] = (reg_value >> 8)  & 0xFF;
    tx_buffer[3] = (reg_value >> 0)  & 0xFF; //LB

    const struct spi_buf tx_buf = {.buf = tx_buffer, .len = sizeof(tx_buffer)}; //buffer of bytes to transmit
    const struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1}; //transmit just one buffer (all 4 bytes)

    gpio_pin_set_dt(&le_pin, HIGH);
    k_sleep(K_NSEC(100)); //make sure le is high for a short duration

    gpio_pin_set_dt(&le_pin, LOW); //pull LE low, signals start of 4byte transaction

    int ret = spi_write(spi_dev, &spi_cfg, &tx_bufs); //blocks until completion 
    if (ret < 0) {
        LOG_ERR("SPI write error");
        return -1;
    }

    gpio_pin_set_dt(&le_pin, HIGH); //latch transmit data

    return 0;

}

void wait_band_select_us(void) {
    uint32_t t_us = (uint32_t)((10.0 * BCSD * 1e6) /(double)REF_OSC);
    k_busy_wait(t_us); //blocking but whatever 
}

int wait_for_lock_ld(uint32_t timeout_ms) {
    uint64_t start = k_uptime_get_32(); 
    while (true) {
        int val = gpio_pin_get_dt(&ld_pin);
        if (val < 0) {
            LOG_ERR("LD pin read error");
            return -1;
        }
        if (val == 1) {
            return 0;  //locked
        }
        if ((k_uptime_get_32() - start) > timeout_ms) {
            return -1; //timed out
        }

        k_busy_wait(10);  //polling ~10us
    }   
}

int calc_adf4351_params(uint32_t frequency, adf4351_status_t* updated_adf4351_status) {
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
    else if (225000 <= frequency && frequency <= 250000) {
        div = 16;
        prescaler = 8;
    }
    else {
        LOG_ERR("Frequency out of bounds in adf4351_set_frequency()\n");
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
        LOG_ERR("Frequency out of bounds, adf4351_set_frequency()");
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
        LOG_ERR("Loop lock timed out");
        return -1;
    }

    LOG_INF("adf4351 frequency updated");
    return 0;

}


int update_rf_divider(adf4351_status_t* updated_adf4351_status) {
    uint8_t div_setting;
    switch (updated_adf4351_status->div) {
        case 16: div_setting = 0b100; break; //added div = 16
        case 32: div_setting = 0b101; break;
        case 64: div_setting = 0b110; break;
        default: return -1; // Invalid divider
    }

    updated_adf4351_status->r4 &= ~R4_RF_DIVIDER_MASK;
    updated_adf4351_status->r4 |= (div_setting << R4_RF_DIVIDER_POS);
    int ret = adf4351_write_register(updated_adf4351_status->r4);
    if (ret < 0) {
        LOG_ERR("Error updating rf divider");
        return -1;
    }
    return 0;
}

int update_rf_prescaler(adf4351_status_t* updated_adf4351_status) {
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
        LOG_ERR("Error updating rf prescaler");
        return -1;
    }
    return 0;
}

int update_rf_freq(adf4351_status_t* updated_adf4351_status) {
    updated_adf4351_status->r0 &= ~R0_RF_INT_MASK;
    updated_adf4351_status->r0 |= (updated_adf4351_status->INT << R0_RF_INT_POS);

    int ret = adf4351_write_register(updated_adf4351_status->r0);
    if (ret < 0) {
        LOG_ERR("Error updating rf frequency");
        return -1;
    }
    return 0;
}


int adf4351_sweep_frequencies(sweep_configt_t sweep_config) {
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
            LOG_ERR("Frequency sweep failed");
            return -1;
        }

        current_frequency += sweep_config.step_size;
        k_msleep(sweep_config.hold_ms); //manual delay at each freq, hold for lock, band select seperate
    }
    LOG_INF("Frequency sweep complete");

    return 0;
}
