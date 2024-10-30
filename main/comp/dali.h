#ifndef _DALI_H
#define _DALI_H

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_event.h"

#include "esp_timer.h"
#include "dali_global.h"


#define BUS_IDLE 1
#define DEFAULT_VREF    1100

ESP_EVENT_DEFINE_BASE(DALI_EVENTS);


enum {
    DALI_HAT_ERROR,
    DALI_HAT_NORMAL,
};



union flag_t {
    struct {
        uint8_t send_active:1;
        uint8_t receive_active:1;
        uint8_t busy:1;
    } data;
    uint8_t package;
};

typedef void (*dali_callback_t)(package_t *data, backword_t *backword);

class Dali  {
    public:
        Dali(){};
        Dali(gpio_num_t tx, gpio_num_t rx, adc_channel_t chan, dali_callback_t cb,gpio_num_t ld)
        {
            initialize(tx,rx,chan,cb, ld);
        }
        ~Dali() {};
        void initialize(gpio_num_t tx, gpio_num_t rx,  adc_channel_t chan, dali_callback_t cb, gpio_num_t ld);
        void send(package_t *package);
        void backword_yesno(backword_t data);
    private:
        gpio_num_t _tx;
        gpio_num_t _rx;
        void _send_one(void);
        void _send_zero(void);
        void _send_stop(void);
        uint64_t period_us = 0;
        volatile bool start_bit = false;
        package_t gelen;
        flag_t flag;
        gpio_num_t Led = GPIO_NUM_NC;

        static void IRAM_ATTR int_handler(void *args);
        static void sem_task(void* arg);
        static void end_clock(void* arg);
        void _send_byte(uint8_t data);
        int Adc_Olcum(void);
        bool Olcum_Ready = true;

        SemaphoreHandle_t semaphore;
        esp_timer_handle_t clock_timer = NULL;
        bool cont = false;
        dali_callback_t callback=NULL;

        adc_channel_t channel = ADC_CHANNEL_0;

        adc_oneshot_unit_handle_t adc1_handle;
        adc_cali_handle_t adc1_cali_chan0_handle = NULL;
        bool do_calibration1_chan0;

        /*
        esp_adc_cal_characteristics_t *adc_chars;
        esp_adc_cal_value_t val_type;
        static const adc_bitwidth_t width = ADC_BITWIDTH_12;
        static const adc_atten_t atten = ADC_ATTEN_DB_11;
        */
        static void adc_task(void* arg);
        bool hat_error = false;        
        bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
        
    protected: 
       

};

#endif