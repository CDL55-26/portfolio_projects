#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "si5351.h"
#include "sweep_configs.h"
#include "oneshot_adc_driver.h"
#include "smf.h"
#include "wifi_transmitter.h"

static const char *TAG = "main";

#define SMART_SWEEP_WIDTH_KHZ 15000 //exactly 0.5 Hz sweep with 12500
sweep_config_t sweep_configs ={
  .start_frequency = 35000,
  .stop_frequency = 80000,
  .step_size = 1000,
  .hold_ms = 25
};

oneshot_adc_datapoint max_data_point ={
    .sample_frequency = 0, 
    .sample_data      = 0,
};

#define SAMPLING_PERIOD_MS 10

//gpio
#define ROUTINE_CONTROL_BUTTON 17 //change esp32c6

//tasks
void sweep_task(void *pvParameters);
void sample_task(void *pvParameters);
void wifi_task(void *pvParameters);

TaskHandle_t sweep_task_handle;
TaskHandle_t sample_task_handle;
TaskHandle_t wifi_task_handle;

//callbacks
static void routine_control_cb(void *arg);
void remote_command_callback(const char *cmd);

//events
EventGroupHandle_t smf_start_events;
EventGroupHandle_t smf_control_events;
EventGroupHandle_t smf_stop_events;

//should prob use a semaphore here, condense everything but whatevs
#define START_SAMPLE_EVENT BIT(0)
#define START_SWEEP_EVENT BIT(1)
#define START_WIFI_EVENT BIT(2)

#define TOGGLE_ROUTINE_EVENT  BIT(0)

#define STOP_SWEEP_EVENT BIT(0)
#define STOP_SAMPLE_EVENT BIT(1)
#define STOP_WIFI_EVENT   BIT(2)

//data queue
#define QUEUE_SIZE 256
QueueHandle_t data_queue;

//max frequency buffer 
#define RESONANT_FREQUENCY_BUFFER_SIZE 1024
resonant_frequency_tuple resonant_frequency_buffer[RESONANT_FREQUENCY_BUFFER_SIZE];
uint16_t resonant_frequency_buffer_sample_count = 0;

//state decs
static void init_run(void);

static void error_entry(void);
static void error_run(void);

static void idle_entry(void);
static void idle_run(void);
static void idle_exit(void);

static void collect_data_entry(void);
static void collect_data_run(void);

static void push_database_entry(void);
static void push_database_run(void);

smf_context_t smf_context;
typedef enum {
  INIT,
  IDLE,
  COLLECT_DATA,
  PUSH_DATABASE,
  ERROR,
}app_states_t;

const smf_state_t state_table[] = {
  [INIT]          = SMF_CREATE_STATE(NULL, init_run, NULL),
  [IDLE]          = SMF_CREATE_STATE(idle_entry, idle_run, idle_exit),
  [COLLECT_DATA]  = SMF_CREATE_STATE(collect_data_entry, collect_data_run, NULL),
  [PUSH_DATABASE] = SMF_CREATE_STATE(push_database_entry, push_database_run, NULL),
  [ERROR]         = SMF_CREATE_STATE(error_entry, error_run, NULL),
};

void app_main(void) {
  smf_init(&smf_context, state_table, INIT);
  ESP_LOGI(TAG, "smf init complete");

  while (1) {
    smf_run(&smf_context);
    vTaskDelay(pdMS_TO_TICKS(10)); //yield to scheduler
  }
  return;
}

//task defs
void sweep_task(void *pvParameters) {
  int ret;
  while (1) {
    xEventGroupWaitBits(smf_start_events, START_SWEEP_EVENT, pdTRUE, pdFALSE, portMAX_DELAY);
    sweep_configs.start_frequency = 35000;
    sweep_configs.stop_frequency  = 80000;

    ESP_LOGI(TAG, "Starting Frequency Sweep");
    while (1) {
      if (xEventGroupWaitBits(smf_stop_events, STOP_SWEEP_EVENT, pdTRUE, pdFALSE, 0)) { //no wait
        ESP_LOGI(TAG, "Sweep task stopped");
        break;
      }
      if (resonant_frequency_buffer_sample_count == 0) {
        ESP_LOGI(TAG, "Original configs");
        ret = si5351_sweep_frequencies(sweep_configs);
      }
      else {
        sweep_configs.start_frequency = max_data_point.sample_frequency - SMART_SWEEP_WIDTH_KHZ < 35000 ? 35000 : max_data_point.sample_frequency - SMART_SWEEP_WIDTH_KHZ ;
        sweep_configs.stop_frequency = max_data_point.sample_frequency + SMART_SWEEP_WIDTH_KHZ > 80000 ? 80000 : max_data_point.sample_frequency + SMART_SWEEP_WIDTH_KHZ ;

        ret = si5351_sweep_frequencies(sweep_configs);
      }
      if (ret != 0) {
        ESP_LOGE(TAG, "si5351 sweep failed");
        return;
      }
    }
  }
}

void sample_task(void *pvParameters) {
  int ret;
  oneshot_adc_datapoint data_point;
  oneshot_adc_datapoint max_datapoint;
  oneshot_adc_datapoint filtered_max_datapoint;

  //averaging stuff
  uint32_t previous_frequency = sweep_configs.start_frequency;
  uint32_t max_sample = 0;

  while (1) {
    xEventGroupWaitBits(smf_start_events, START_SAMPLE_EVENT, pdTRUE, pdFALSE, portMAX_DELAY);
    max_sample = 0;
    previous_frequency = sweep_configs.start_frequency;
    oneshot_adc_clear_filter(); //clear the moving average filter for next trial

    ESP_LOGI(TAG, "Starting Sampling");
    while (1) {
      if (xEventGroupWaitBits(smf_stop_events, STOP_SAMPLE_EVENT, pdTRUE, pdFALSE, 0)) { //no wait
        ESP_LOGI(TAG, "Sample task stopped");
        break;
      }
      ret = oneshot_adc_get_datapoint(&data_point);
      if (ret != 0) {
        ESP_LOGE(TAG, "ADC sample failed");
        return;
      }

      if (data_point.sample_frequency != previous_frequency) {
        max_datapoint.sample_data = max_sample;
        max_datapoint.sample_frequency = previous_frequency;
        
        /*
         *takes the max data point of a given freq bucket and passes to smoothing function
         function is array that holds previous samples. add up samples and average
         return average and add to the bufffer <-- might need to tweak
         */
        oneshot_adc_filter_datapoint(&max_datapoint, &filtered_max_datapoint);

        xQueueSend(data_queue, &filtered_max_datapoint, 0); //never wait. If full, just drop the sample
        //ESP_LOGI(TAG, "Filtered Data point: %df %d val", max_datapoint.sample_frequency, max_datapoint.sample_data);
        
        previous_frequency = data_point.sample_frequency;
        max_sample = data_point.sample_data; //reset for next freq bucket;
      }

      if (data_point.sample_data > max_sample) {
        max_sample = data_point.sample_data;
      }
      
      vTaskDelay(pdMS_TO_TICKS(SAMPLING_PERIOD_MS)); //delay for 10 ms between samples. Not super accurate, but dont care
    }
  }
}

/*
 Need to change up the peak detection. Just taking the max value wont work. really needs to be max value 
 per sweep

 should add a similar previous_frequency like adc task:
  when current_freq < prev_freq, send max value of previous sweep
*/


void wifi_task(void *pvParameters) { 
  oneshot_adc_datapoint data_point;
    
  //resonant buffer stuff
  resonant_frequency_tuple frequency_tuple;

  uint32_t previous_frequency = sweep_configs.start_frequency;

  while (1) {
    xEventGroupWaitBits(smf_start_events, START_WIFI_EVENT, pdTRUE, pdFALSE, portMAX_DELAY);
    previous_frequency = sweep_configs.start_frequency;
    max_data_point.sample_frequency = 0;
    max_data_point.sample_data = 0;
    resonant_frequency_buffer_sample_count = 0;
    ESP_LOGI(TAG, "Starting Wifi Event");

    while (1) {
      if (xEventGroupWaitBits(smf_stop_events, STOP_WIFI_EVENT, pdTRUE, pdFALSE, 0)) { //no wait
        max_data_point.sample_frequency = 0;
        max_data_point.sample_data = 0;
        ESP_LOGI(TAG, "Wifi task stopped");
        break; 
      }
      if (xQueueReceive(data_queue, &data_point, pdMS_TO_TICKS(100))) { //dont block indefinitely,
        
        if ((data_point.sample_frequency < previous_frequency) && data_point.sample_frequency != 0) { //when cur freq < prev freq, we've wrapped with the sweep
          wifi_ws_send_resonance(max_data_point.sample_frequency, max_data_point.sample_data); 
          max_data_point.sample_data = 0; //reset after each sweep

          // new res frequency buffer stuff
          frequency_tuple.frequency = max_data_point.sample_frequency; //add resonant frequency to buffer
          frequency_tuple.frequency_index = resonant_frequency_buffer_sample_count;
          if (resonant_frequency_buffer_sample_count > RESONANT_FREQUENCY_BUFFER_SIZE - 1) { //ring buffer, dont overflow
            resonant_frequency_buffer_sample_count = resonant_frequency_buffer_sample_count % RESONANT_FREQUENCY_BUFFER_SIZE;
          }
          resonant_frequency_buffer[resonant_frequency_buffer_sample_count] = frequency_tuple;
          resonant_frequency_buffer_sample_count++;
        }
        
        if (data_point.sample_data >= max_data_point.sample_data) {  //if we find new max, write to max_data_point
          max_data_point.sample_data = data_point.sample_data;
          max_data_point.sample_frequency = data_point.sample_frequency;

          //ESP_LOGI(TAG, "Max Frequency: %d, Max sample: %d", max_data_point.sample_frequency, max_data_point.sample_data);
        }

        wifi_ws_send_datapoint(data_point.sample_frequency, data_point.sample_data); //always send datapoint
        previous_frequency = data_point.sample_frequency;
        //ESP_LOGI(TAG, "Frequency: %d, sample: %d", data_point.sample_frequency, data_point.sample_data);
      }
    }
  }
}

//callback defs
static void routine_control_cb(void *arg) {
  xEventGroupSetBits(smf_control_events, TOGGLE_ROUTINE_EVENT);
}

void remote_command_callback(const char *cmd) {
  //check if cmd is TOGGLE
  if (strcmp(cmd, "TOGGLE") == 0) {
    ESP_LOGI(TAG, "Remote Toggle Received");
    xEventGroupSetBits(smf_control_events, TOGGLE_ROUTINE_EVENT);
  }
}

//state defs
static void init_run(void) {
  int err;
  err = oneshot_adc_init();
  if (err != 0) {
    ESP_LOGE(TAG,"ADC init failed");
    smf_set_state(&smf_context, ERROR);
    return;
  }
  //will need to go back and change these return values
  err = si5351_init();
  if (err != 0) {
    ESP_LOGE(TAG, "si5351 init failed");
    smf_set_state(&smf_context, ERROR);
    return;
  }
  if (wifi_init() != ESP_OK) {
    ESP_LOGE(TAG, "WIFI init failed");
    smf_set_state(&smf_context, ERROR);
  }
  wifi_register_cmd_callback(remote_command_callback);
  if (wifi_ws_start() != ESP_OK) {
    ESP_LOGE(TAG, "WS init failed");
    smf_set_state(&smf_context, ERROR);
  }

  gpio_config_t start_rountine_btn_config = {
    .pin_bit_mask = (1ULL << ROUTINE_CONTROL_BUTTON),
    .mode = GPIO_MODE_INPUT,
    .intr_type = GPIO_INTR_POSEDGE,
  };
  gpio_config(&start_rountine_btn_config);
  gpio_install_isr_service(0); //only do this once, here
  gpio_isr_handler_add(ROUTINE_CONTROL_BUTTON, routine_control_cb, NULL);

  smf_start_events = xEventGroupCreate();
  smf_stop_events = xEventGroupCreate();
  smf_control_events = xEventGroupCreate();

  data_queue = xQueueCreate(256, sizeof(oneshot_adc_datapoint));

  xTaskCreate(sample_task, "sample_task", 2048, NULL, 2, &sample_task_handle);
  xTaskCreate(sweep_task, "sweep_task", 2048, NULL, 2, &sweep_task_handle);
  xTaskCreate(wifi_task, "wifi_task", 2048, NULL, 7, &wifi_task_handle);

  ESP_LOGI(TAG,"Init completed");
  smf_set_state(&smf_context, IDLE);
}

static void idle_entry(void) {
  ESP_LOGI(TAG, "Entering Idle");
  vTaskDelay(pdMS_TO_TICKS(40)); //a little wait to debounce
  //turn button interrupts back on
  sweep_configs.start_frequency = 35000;
  sweep_configs.stop_frequency  = 80000;
  xEventGroupClearBits(smf_control_events, TOGGLE_ROUTINE_EVENT);
}
static void idle_run(void) {
  xEventGroupWaitBits(smf_control_events, TOGGLE_ROUTINE_EVENT, pdTRUE, pdFALSE, portMAX_DELAY);
  smf_set_state(&smf_context, COLLECT_DATA);
}
static void idle_exit(void) {
  ESP_LOGI(TAG, "Exiting Idle");
}

static void collect_data_entry(void) {
  ESP_LOGI(TAG, "Entering Collect Data");
  vTaskDelay(pdMS_TO_TICKS(80));
  //make sure all bits are cleared
  xEventGroupClearBits(smf_control_events, TOGGLE_ROUTINE_EVENT);
  xEventGroupClearBits(smf_stop_events, (STOP_WIFI_EVENT | STOP_SWEEP_EVENT | STOP_SAMPLE_EVENT));
}
static void collect_data_run(void) {
  xEventGroupSetBits(smf_start_events, (START_WIFI_EVENT | START_SAMPLE_EVENT | START_SWEEP_EVENT));
  xEventGroupWaitBits(smf_control_events, TOGGLE_ROUTINE_EVENT, pdTRUE, pdFALSE, portMAX_DELAY); //wait for toggle
  ESP_LOGI(TAG, "STOP routine");
  xEventGroupSetBits(smf_stop_events, (STOP_WIFI_EVENT | STOP_SWEEP_EVENT | STOP_SAMPLE_EVENT));

  smf_set_state(&smf_context, PUSH_DATABASE);
}

static void push_database_entry(void) {
  ESP_LOGI(TAG, "Entering Push Database");
}

static void push_database_run(void) {
  /*
    state needs to call function from wifi_transmitter.c
    essentially just pass the the buffer of peaks + indexes

    pass resonant_frequency_buffer[], resonant_frequency_buffer_sample_count
  */ 
  wifi_post_resonant_frequencies(resonant_frequency_buffer, resonant_frequency_buffer_sample_count);

  smf_set_state(&smf_context, IDLE);
}

static void error_entry(void) {
  ESP_LOGI(TAG, "Entered Error");
  vTaskSuspend(sweep_task_handle);
  vTaskSuspend(sample_task_handle);
  vTaskSuspend(wifi_task_handle);
}

static void error_run(void) {
  //wait 20 s, else force restart
  xEventGroupWaitBits(smf_control_events, TOGGLE_ROUTINE_EVENT, pdTRUE, pdFALSE, pdMS_TO_TICKS(20000));
  //use same button restart as toggle event
  esp_restart(); 
}

