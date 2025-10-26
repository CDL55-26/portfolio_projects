#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h> 
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/pwm.h> 
#include <zephyr/sys/atomic.h>
#include <zephyr/smf.h> 
#include "adf4351.h"


LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

//Events
K_EVENT_DEFINE(events);

#define START_SWEEP_EVENT BIT(0)

#define EVENTS_MASK (START_SWEEP_EVENT)

//GPIO
const struct gpio_dt_spec sweep_led = GPIO_DT_SPEC_GET(DT_ALIAS(sweepled), gpios); //HB led

const struct gpio_dt_spec sweep_button = GPIO_DT_SPEC_GET(DT_ALIAS(startsweep), gpios);
static struct gpio_callback sweep_button_cb;

//Callbacks
void sweep_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);


//State machine setup
enum event_state {INIT, SWEEP, IDLE};
struct s_object {
    struct smf_ctx ctx;
} s_obj;

static void init_run(void *o);

static void idle_entry(void *o);
static void idle_run(void *o);
static void idle_exit(void *o);

static void sweep_entry(void *o);
static void sweep_run(void *o);
static void sweep_exit(void *o);

static const struct smf_state event_states[] = {
    [INIT]  = SMF_CREATE_STATE(NULL, init_run, NULL, NULL, NULL),
    [IDLE]  = SMF_CREATE_STATE(idle_entry, idle_run, idle_exit, NULL, NULL),
    [SWEEP] = SMF_CREATE_STATE(sweep_entry, sweep_run, sweep_exit, NULL, NULL),
};

//State definitions
static void init_run(void *o) {
    int err = gpio_pin_configure_dt(&sweep_led, GPIO_OUTPUT_INACTIVE); //led for sweeping
    if (err < 0) { LOG_ERR("init failure"); return; }

    err = gpio_pin_configure_dt(&sweep_button, GPIO_INPUT);
    if (err < 0) { LOG_ERR("init failure"); return; }

    gpio_init_callback(&sweep_button_cb, sweep_button_callback, BIT(sweep_button.pin));
    err = gpio_add_callback_dt(&sweep_button, &sweep_button_cb);
    if (err < 0) { LOG_ERR("init failure"); return; }
    err = gpio_pin_interrupt_configure_dt(&sweep_button, GPIO_INT_EDGE_TO_ACTIVE);
    if (err < 0) { LOG_ERR("init failure"); return; }

    err = adf4351_init();
    if (err < 0) {
        LOG_ERR("adf4351 init failed");
        return; 
    }

    LOG_INF("Init Successful");
    smf_set_initial(SMF_CTX(&s_obj), &event_states[IDLE]);
}



static void idle_entry(void *o) {
    LOG_INF("Entered Idle");
    gpio_pin_set_dt(&sweep_led, 0);
}
static void idle_run(void *o) {
    uint32_t posted = k_event_wait(&events, EVENTS_MASK, true, K_FOREVER);
    if(posted & START_SWEEP_EVENT){
        smf_set_state(SMF_CTX(&s_obj), &event_states[SWEEP]);
    }   
}
static void idle_exit(void *o) {
    LOG_INF("Exiting Idle -> Sweep");
}



static void sweep_entry(void *o) {
    LOG_INF("Entered Sweep");
    gpio_pin_set_dt(&sweep_led, 1);
}
static void sweep_run(void *o) {
    sweep_configt_t test_configs ={
        .start_frequency = 130000,
        .stop_frequency = 250000,
        .step_size = 2000,
        .hold_ms = 100
    };

    int ret;
    ret = adf4351_sweep_frequencies(test_configs);
    if (ret < 0) {
        LOG_ERR("sweep frequency failed");
        return;
    }
    
    smf_set_state(SMF_CTX(&s_obj), &event_states[IDLE]);
}
static void sweep_exit(void *o) {
    LOG_INF("Exiting Sweep -> Idle");
}




int main(void) {
    smf_set_initial(SMF_CTX(&s_obj), &event_states[INIT]);
    int ret;
    while (1) {
        ret = smf_run_state(SMF_CTX(&s_obj));
        if (ret) {
            smf_set_terminate(SMF_CTX(&s_obj), ret);
            break;
        }
        k_msleep(10);
    }
    return 0;
}


//Callback defs
void sweep_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    k_event_post(&events, START_SWEEP_EVENT);
}