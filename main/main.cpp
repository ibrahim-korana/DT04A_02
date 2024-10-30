#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "core.h"
#include "comp/dali.h"
#include "iot_button.h"

#include "event.h"
#include "esp_https_ota.h"
#include "dali_global.h"
#include "comp/storage.h"
#include "comp/triac_dim.h"
#include "esp_log.h"


const char *TAG ="MAIN";


#define ZERO GPIO_NUM_14
#define DALI_TX GPIO_NUM_16
#define DALI_RX GPIO_NUM_4
#define LED GPIO_NUM_5
#define ADC GPIO_NUM_35
#define BUTTON1 GPIO_NUM_18
#define BUTTON2 GPIO_NUM_19
#define BUTTON3 GPIO_NUM_21
#define BUTTON4 GPIO_NUM_17

#define DIMMER_OUT GPIO_NUM_13

ESP_EVENT_DECLARE_BASE(DALI_EVENTS);

#define FATAL_MSG(a, str)                          \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        abort();                                         \
    }

#define GLOBAL_FILE "/config/config.bin"
#define NETWORK_FILE "/config/network.bin"


TriacDimmer dimmer;
Dali dali;
button_handle_t btn1, btn2, btn3, btn4;
Storage disk;
config_t GlobalConfig;
home_network_config_t NetworkConfig = {};

bool initialize_mod = false;
bool have_random=false;
bool dali_hat_status=true;
bool dali_hat_yk=false;
bool Net_Connect = false;
bool busy = true;

void dali_callback(package_t *data, backword_t *backword);
void btn_callback(void *arg, void *data);

#include "tool/config.cpp"

void dimmer_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    uint8_t *kk = (uint8_t *)event_data;
    uint8_t zz = 99;
    if (kk!=NULL) zz=*kk;
    if (id==DIMMER_BUSY_END) {ESP_LOGW(TAG,"%d Fade STOP",zz); busy=false;gpio_set_level(LED,0);}
    if (id==DIMMER_BUSY_START) {ESP_LOGW(TAG,"%d Fade START",zz);busy=true;gpio_set_level(LED,1);}
}

void dali_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    if (id==DALI_HAT_ERROR) {
        if (!Net_Connect)
            {
                ESP_LOGE(TAG,"DALI Hat KOPTU");
                dimmer.direct_power(dimmer.get_failure_level());
                dali_hat_status=false;
                dali_hat_yk = true;
            }
    }
    if (id==DALI_HAT_NORMAL) {
        if (!Net_Connect)
            {
                ESP_LOGW(TAG,"DALI Hat NORMAL");
                dali_hat_status=true;
                dali_hat_yk = false;
            }
        }
}


extern "C" {
#include "bootloader_random.h"
}

void dali_out_test(void)
{
    package_t pk = {};
    address_t bb = {};  
    bb.broadcast_adr = true;
    bb.arc_power = true;
    bb.data = 0xFF;
    pk.data.data0 = package_address(bb); 
    pk.data.data1 = 254; 
    pk.data.type = BLOCK_16;

    while(1)
    {
       // dali.busy_is_wait();
        dali.send(&pk);
        printf("ADR %02X DATA %02X\n",pk.data.data0,pk.data.data1);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

extern "C" void app_main()
{
    bootloader_random_enable();
    
    esp_log_level_set("gpio", ESP_LOG_NONE);

    ESP_LOGI(TAG, "INITIALIZING...");
    config();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(DIMMER_EVENTS, ESP_EVENT_ANY_ID, dimmer_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(DALI_EVENTS, ESP_EVENT_ANY_ID, dali_handler, NULL, NULL));


    dali.initialize(DALI_TX,DALI_RX,ADC_CHANNEL_7,dali_callback,LED);
    dimmer.init(ZERO, DIMMER_OUT, LED, &disk);
    dimmer.start();
    

ESP_LOGI(TAG, "Init Stop..");
ESP_LOGI(TAG, "Short ADDR = %02X",dimmer.get_short_address());

   // dali_out_test();



    while(true)
    {
           
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}


#define LOG0
//#define LOG1

void dali_callback(package_t *data, backword_t *backword)
{
    gpio_set_level(LED,1);
    backword->backword = BACK_NONE;
    #ifdef LOG0
       ESP_LOGI("DALI", "Gelen << 0=%02x 1=%02X 2=%02X %d", data->data.data0, data->data.data1 , data->data.data2,  data->data.type);
    #endif   

 
    address_t adres = {};
   unpackage_address(data->data.data0,&adres);
   
   #ifdef LOG1
   if (!adres.error)
   {
       
        printf("arc power %d\n", adres.arc_power);
        printf("short adr %d\n", adres.short_adr);
        printf("group adr %d\n", adres.group_adr);
        printf("broadcast %d\n", adres.broadcast_adr);
        printf("special %d\n", adres.special);
        printf("data %02X\n", adres.data);
        printf("error %d\n", adres.error);
        printf("COMMAND %02X %02X\n", data->data.data0,data->data.data1);
        
   }
   #endif

    if (!adres.error)
    {
        bool my_package = false;
        if (adres.short_adr || adres.group_adr) {
            if (dimmer.is_short_address(adres.data)) my_package=true;
            if (dimmer.is_group_address(adres.data)) my_package=true;       
        }
        if (adres.arc_power && (adres.broadcast_adr || my_package) && !initialize_mod) {
                //direct arc power geldi
                ESP_LOGI("DALI","Arc Power 0x%02X\n",data->data.data1);
                dimmer.direct_arc_power(data->data.data1);
                }

        if (!adres.arc_power && !adres.special && !initialize_mod && (adres.broadcast_adr || my_package))
            {
                //Normal Komut geldi
                printf("Command 0x%02X\n",data->data.data1);
                if (data->data.data1==0x00) dimmer.command_off();
                /*
                if (data->data.data1==0x01) dimmer.command_up(&action);
                if (data->data.data1==0x02) dimmer.command_down(&action);  
                if (data->data.data1==0x03) dimmer.command_step_up(&action);
                if (data->data.data1==0x04) dimmer.command_step_down(&action);
                if (data->data.data1==0x05) dimmer.command_recall_max_level();
                if (data->data.data1==0x06) dimmer.command_recall_min_level(); 
                if (data->data.data1==0x07) dimmer.command_step_down_and_off(&action); 
                if (data->data.data1==0x08) dimmer.command_on_and_step_up(&action); 
                if (data->data.data1==0x0A) dimmer.command_goto_last_active_level(&action);
                if (data->data.data1==0x0B) dimmer.command_continuous_up(&action);
                if (data->data.data1==0x0C) dimmer.command_continuous_down(&action);
                if (data->data.data1>=0x10 && data->data.data1<=0x1F) dimmer.command_goto_scene(data->data.data1&0x0F,&action); 

                if (
                    (data->data.data1>0x1F && data->data.data1<0x90) ||  //DT6 command
                    (data->data.data1>0xE0 && data->data.data1<0xF0)     //DT7 command
                   ) {
                    dimmer.command_config_with_adr(data->data.data1,adres); 
                }


                if (data->data.data1>0x1F && data->data.data1<0x90) dimmer.command_config(data->data.data1); 
                if (data->data.data1>=0x90 && data->data.data1<=0xC6) {
                    uint8_t bck = dimmer.command_query(data->data.data1);
                    backword->backword = BACK_DATA;
                    backword->data = bck;
                }
                */

            }

        if (adres.special) 
            {
                /*
               // printf("Special 0x%02X 0x%02X\n",adres.data,data->data.data1);
                if (adres.data==0x00) {
                    initialize_mod=false;
                    have_random=false; 
                    printf("INITIALIZE EXIT\n");
                }
            
                if (adres.data==0x02 && !initialize_mod) {
                    if (data->data.data1==0x00)
                    {
                        initialize_mod=true;
                        printf("ALL DEV INITIALIZE MODE\n");  
                    }   
                    if (data->data.data1==0xFF)
                    {
                        if (dimmer.get_short_address()==0xFF)
                        {
                            initialize_mod=true;
                            printf("NOT ADDR DEV INITIALIZE MODE\n");  
                        }
                    }
                    if (!initialize_mod)
                    {
                       if ((data->data.data1>>1)==dimmer.get_short_address())
                       {
                            initialize_mod=true;
                            printf("%02x ADDR DEV INITIALIZE MODE\n",data->data.data0>>1);  
                       }
                    }   
                }

                if (initialize_mod)
                {
                    if (adres.data==0x03 && !have_random) {
                        dimmer.special_command(adres.data,data->data.data0);
                        have_random=true; 
                        printf("RANDOM\n");                    
                    }

                    if (have_random)
                    {
                        if (adres.data==0x04) {
                            uint8_t ret = dimmer.special_command(adres.data,data->data.data1);
                            if (ret==0xFF) backword->backword = BACK_YES; 
                               // printf("COMPARE\n");                   
                        }
                        if (adres.data==0x08) {
                            dimmer.special_command(adres.data,data->data.data1);
                            //printf("ADDR H %02x\n",data->data.data1);                  
                        }
                        if (adres.data==0x09) {
                            dimmer.special_command(adres.data,data->data.data1);
                           //printf("ADDR M %02x\n",data->data.data1);                
                        }
                        if (adres.data==0x0A) {
                            dimmer.special_command(adres.data,data->data.data1);
                            //printf("ADDR L %02x\n",data->data.data1);                    
                        }
                        if (adres.data==0x0B) {
                            dimmer.special_command(adres.data,data->data.data1);
                            
                        }
                        if(adres.data==0x0C) {
                            uint8_t ret = dimmer.special_command(adres.data,data->data.data1);
                            if (ret==0xFF) backword->backword = BACK_YES; //else *backword = BACK_NO;
                        }
                        if (adres.data==0x05) {
                            uint8_t ret = dimmer.special_command(adres.data,data->data.data1);
                            if (ret==0xFF) {
                                initialize_mod=false;
                                have_random=false; 
                                printf("WITHDRAW INITIALIZE EXIT\n");
                            }                       
                    }
                    }                    
                }
                */
            }   
    }
   
    vTaskDelay(10 / portTICK_PERIOD_MS);
    gpio_set_level(LED,0);

}

void btn_callback(void *arg, void *data)
{
                            
}


