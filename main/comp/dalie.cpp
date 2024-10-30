#include "dali.h"
#include "math.h"

#include <rom/ets_sys.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_event.h"

#define HALF_PERIOD 416
#define FULL_PERIOD 832
#define HP_SAPMA 42
#define FP_SAPMA 84

#define HALF_PERIOD_UP HALF_PERIOD+HP_SAPMA
#define HALF_PERIOD_DOWN HALF_PERIOD-HP_SAPMA
#define FULL_PERIOD_UP FULL_PERIOD+FP_SAPMA
#define FULL_PERIOD_DOWN FULL_PERIOD-FP_SAPMA

const char *DALI_TAG = "DALI_TAG"; 

ESP_EVENT_DEFINE_BASE(DALI_EVENTS);

int Dali::Adc_Olcum(void)
{
    int adc_raw, voltage;
    adc_oneshot_read(adc1_handle, ADC_CHANNEL_7, &adc_raw);   
    if (do_calibration1_chan0) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan0_handle, adc_raw, &voltage));
        return voltage;
    } 
    return 0;
};

void Dali::adc_task(void *args)
{
    Dali *self = (Dali *)args;   
    for (;;) {
        if (self->Olcum_Ready)
        {
            int adc_reading = self->Adc_Olcum();
           //ESP_LOGI(DALI_TAG,"Hat %d",adc_reading);
            if (adc_reading<500) {
                vTaskDelay(100 / portTICK_PERIOD_MS); adc_reading = self->Adc_Olcum();
                if (adc_reading<500) {vTaskDelay(100 / portTICK_PERIOD_MS); adc_reading = self->Adc_Olcum();}
                if (adc_reading<500) {
                    if (!self->hat_error) {
                        esp_event_post(DALI_EVENTS, DALI_HAT_ERROR, 0, NULL, 0);
                    }
                    self->hat_error = true; 
                    gpio_intr_disable(self->_rx);
                }
                //dali hattının voltajı gitti. lambanın systemFailureLevel seviyesinde yakılması gerekiyor.
            }else {
                if (self->hat_error) {
                    esp_event_post(DALI_EVENTS, DALI_HAT_NORMAL, 0, NULL, 0);
                }
                self->hat_error = false;
                gpio_intr_enable(self->_rx);
            }
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);         
        }
}

bool Dali::adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

    if (!calibrated) {
        ESP_LOGI("DALI", "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {};
            cali_config.unit_id = unit;
            cali_config.atten = atten;
            cali_config.bitwidth = ADC_BITWIDTH_DEFAULT;
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI("DALI", "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW("DALI", "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE("DALI", "Invalid arg or no memory");
    }

    return calibrated;
}

void Dali::initialize(gpio_num_t tx, gpio_num_t rx,  adc_channel_t chan, dali_callback_t cb)
{
            _tx = tx;
            _rx = rx;
            callback = cb;
           // channel = chan;
            gpio_config_t io_conf = {};
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pin_bit_mask = (1ULL<<_tx) ;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE; 
            gpio_config(&io_conf);
            gpio_set_level(_tx, BUS_IDLE);
            gpio_set_drive_capability(_tx,GPIO_DRIVE_CAP_3);

            gpio_config_t intConfig;
            intConfig.pin_bit_mask = (1ULL<<_rx);
            intConfig.mode         = GPIO_MODE_INPUT;
            intConfig.pull_up_en   = GPIO_PULLUP_DISABLE;
            intConfig.pull_down_en = GPIO_PULLDOWN_ENABLE;
            intConfig.intr_type    = GPIO_INTR_ANYEDGE;
            gpio_config(&intConfig);
            
            gpio_set_intr_type(_rx, GPIO_INTR_ANYEDGE);
	         gpio_isr_handler_add(_rx, int_handler, (void *)this);
            gpio_intr_disable(_rx);

            esp_timer_create_args_t tim_arg0 = {};
                    tim_arg0.callback = &end_clock;
                    tim_arg0.arg = (void*) this;
                    tim_arg0.name = "clkend";
           
            adc_oneshot_unit_init_cfg_t init_config1 = {};
               init_config1.unit_id = ADC_UNIT_1;            
            ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle)); 

            adc_oneshot_chan_cfg_t config = {};
               config.bitwidth = ADC_BITWIDTH_DEFAULT;
               config.atten = ADC_ATTEN_DB_12;        
            ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_7, &config));  

            adc_cali_handle_t adc1_cali_chan0_handle = NULL;
            bool do_calibration1_chan0 = adc_calibration_init(ADC_UNIT_1, ADC_CHANNEL_7, ADC_ATTEN_DB_12, &adc1_cali_chan0_handle);
                                  
            ESP_ERROR_CHECK(esp_timer_create(&tim_arg0, &clock_timer));
            semaphore = xSemaphoreCreateBinary();
            xTaskCreate(sem_task, "task_02", 2048, (void *)this, 10, NULL);
            xTaskCreate(adc_task, "task_03", 2048, (void *)this, 10, NULL);
           gpio_intr_enable(_rx);
}


void Dali::_send_one(void)
{
    portDISABLE_INTERRUPTS();
     gpio_set_level(_tx, 0);
     ets_delay_us(HALF_PERIOD);
     gpio_set_level(_tx, 1);
     ets_delay_us(HALF_PERIOD);
    portENABLE_INTERRUPTS(); 
}

void Dali::_send_zero(void)
{
    portDISABLE_INTERRUPTS();
     gpio_set_level(_tx, 1);
     ets_delay_us(HALF_PERIOD);
     gpio_set_level(_tx, 0);
     ets_delay_us(HALF_PERIOD); 
    portENABLE_INTERRUPTS();  
}

void Dali::_send_stop(void)
{
    portDISABLE_INTERRUPTS();
     gpio_set_level(_tx, 1);
     ets_delay_us(FULL_PERIOD*6);  
    portENABLE_INTERRUPTS(); 
}

void Dali::_send_byte(uint8_t data)
{
   uint8_t tmp=data, ii=0xFF;
   portDISABLE_INTERRUPTS();
   while(ii>0)
   {
      if ((tmp&0x80)==0x80)
      {
         _send_one(); 
      }
      else
      {
         _send_zero();
      }
      tmp=tmp<<1;           
      ii=ii>>1;
   } 
   portENABLE_INTERRUPTS(); 
}
void Dali::send(package_t *package)
{
   period_us = esp_timer_get_time();
   gpio_intr_disable(_rx);
   printf("Dali send %02X %02X %02X\n", package->data.data0, package->data.data1, package->data.data2);

   //Start Bit
   _send_one(); 
   //Adress
   _send_byte(package->data.data0);
   if (package->data.type==BLOCK_24 || package->data.type==BLOCK_16) _send_byte(package->data.data1);
   if (package->data.type==BLOCK_24) _send_byte(package->data.data2);
   //Stop bits
   _send_stop();
   vTaskDelay(8/portTICK_PERIOD_MS);
    gpio_intr_enable(_rx);
}

void IRAM_ATTR  Dali::int_handler(void *args)
{
    Dali *self = (Dali *)args;
    uint8_t level = gpio_get_level(self->_rx);
    static bool start_bit_begin = false; 
    if (!self->start_bit)
    { 
      if (start_bit_begin && level==1) 
        {          
           self->start_bit = true;
           start_bit_begin=false;    
           gpio_intr_disable(self->_rx);  
           if (self->clock_timer!=NULL) (esp_timer_start_periodic(self->clock_timer, HALF_PERIOD));    
        } else {
             start_bit_begin=true;         
               }  
    } else self->cont = true;    
}

void Dali::backword_yesno(backword_t data)
{
   // period_us = esp_timer_get_time();
    uint8_t dat = 0x00;
    if (data.backword==BACK_DATA) dat=data.data; else dat=data.backword;
    gpio_intr_disable(_rx);
    portDISABLE_INTERRUPTS(); 
    //vTaskDelay(10/portTICK_PERIOD_MS); 
    _send_one(); 
    _send_byte(dat);
    _send_stop();
    ets_delay_us(FULL_PERIOD*6);
    portENABLE_INTERRUPTS(); 
    gpio_intr_enable(_rx);
    //printf("BACKWORD GONDERILDI %02X\n", data);
}


void Dali::sem_task(void *args)
{
    Dali *self = (Dali *)args;   
    for (;;) {
        if (xSemaphoreTake(self->semaphore, portMAX_DELAY)) {
            backword_t rt = {};
           rt.backword = BACK_NONE;
           if (self->callback!=NULL) self->callback(&self->gelen, &rt); 
           ets_delay_us(HALF_PERIOD * 6);
           if (rt.backword!=BACK_NONE) self->backword_yesno(rt);
            self->gelen.package = 0;                                                  
          }           
        }
}

#define CALIBRATION 50

void Dali::end_clock(void *args)
{
    Dali *self = (Dali *)args;
    static int bb = 0;  
    static uint32_t gelen = 0x000000;
    static uint8_t max=22;
    static uint8_t count=0;
    count++;
    if(++bb>1)
    {
      bb=0;
      ets_delay_us(CALIBRATION);
      uint8_t level = gpio_get_level(self->_rx);
      gelen = (gelen<<1) | level;  
    }

    if (count==18 || count==35 ) {
      gpio_intr_enable(self->_rx);
    }
    if (count==21 || count==38) {
      gpio_intr_disable(self->_rx);
      if (self->cont) {
         self->cont=false; 
         max=38;
         if (count==38) max=54;
         } 
    }

    if (count>max) {
      self->gelen.package =(gelen>>3); 
      self->gelen.data.type = BLOCK_24;
      //printf("Count %d  ",count);
      //printf("%08x %08x\n",(int)self->gelen.package,(int)gelen);

      if(count==23) {self->gelen.package = self->gelen.package<<16;self->gelen.data.type = BLOCK_8;}
      if(count==39) {self->gelen.package = self->gelen.package<<8;self->gelen.data.type = BLOCK_16;}
      xSemaphoreGive(self->semaphore);
      gelen = 0;
      esp_timer_stop(self->clock_timer); 
      bb=0; 
      count=0;
      max = 22;
      self->start_bit = false;  
      self->cont=false;
      gpio_intr_enable(self->_rx);      
      
    }

}

