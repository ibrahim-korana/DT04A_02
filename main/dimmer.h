#ifndef _DIMMER_H
#define _DIMMER_H

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_event.h"


typedef enum {
    DIR_UP = 0,
    DIR_DOWN,
} direction_t;

typedef enum {
    NM_READ=0,
    NM_WRITE,
    NM_INIT,
    NM_RESET,
} rw_status_t;

typedef enum {
    CALC_FADE_TIME = 0,
    CALC_FADE_RATE,
    CALC_200_MS,
    CALC_STATIC
} fade_time_calc_t;


typedef struct _fade_param {
    uint8_t actual;
    uint8_t destination;
    uint32_t step_time;
    uint16_t step_start;
    uint16_t step_stop;
    direction_t direction;
    int step;
    uint16_t total_step;
} fade_param_t;

union long_number_t {
     struct {
        uint8_t data0;      
        uint8_t data1;
        uint8_t data2;
        uint8_t data3;
     } data;  
     uint32_t long_num;
};

typedef struct {
   long_number_t search;
   long_number_t random; 
   bool is_equal(void) {if (random.long_num==search.long_num) return true; else return false;}
   bool is_less_than_equal(void) {if (random.long_num<=search.long_num) return true; else return false;}
} long_addr_t;

class Dimmer  {
    public:
        Dimmer() {};
        Dimmer(gpio_num_t zeropin, gpio_num_t outpin, gpio_num_t f_led)
        {
            Initialize(zeropin, outpin, f_led);
        };

        void Initialize(gpio_num_t zeropin, gpio_num_t outpin, gpio_num_t fade_led);

        ~Dimmer() {};
        void start(void);
        void set_tim(uint16_t tm) {
            raw_pwm_time = tm;
        };

        uint16_t get_tim(void) { return raw_pwm_time;}


        void set_power_cycle_seen(bool stat) {power_cycle_seen = stat;}

        bool direct_arc_power(uint16_t level);
        bool command_off(void);
        bool command_up(uint8_t *action);
        bool command_down(uint8_t *action);
        bool command_step_up(uint8_t *action);
        bool command_step_down(uint8_t *action);
        bool command_recall_max_level(void);
        bool command_recall_min_level(void);
        bool command_step_down_and_off(uint8_t *action);
        bool command_on_and_step_up(uint8_t *action);
        bool command_goto_last_active_level(uint8_t *action);
        bool command_continuous_up(uint8_t *action);
        bool command_continuous_down(uint8_t *action);
        bool command_goto_scene(uint8_t scn, uint8_t *action);

        uint8_t command_query(uint8_t comm);

        //uint8_t command_config(uint8_t comm);
        uint8_t command_config(uint8_t comm, bool reply=true);
        uint8_t special_command(uint8_t comm, uint8_t dat);


        uint8_t get_actual_level(void) {return actual_level;}
        uint8_t get_max_level(void) {return max_level;}
        uint8_t get_min_level(void) {return min_level;}
        bool is_fade_running(void) {return fade_running;}
        bool is_open(void) {return lamp_on;}

        bool is_short_address(uint8_t addr) {
            if (short_address>64) return false;
            if (short_address==0xFF) return false;
            if (short_address==addr) return true;
            return false;
            }
        uint8_t get_short_address(void) {return short_address;}    
        bool is_group_address(uint8_t addr) {
            if (addr>15) return false;
            if (gear_groups[0]==0xFF && gear_groups[1]==0xFF) return false;
            if (((uint8_t)pow(2,addr)&gear_groups[0])>0x00) return true;
            if (addr>8) {
                uint8_t tmp=addr-8;
                if (((uint8_t)pow(2,tmp)&gear_groups[1])>0x00) return true;
            }
            return false;
        }    

        uint8_t hedef_level;
       
    private:
        gpio_num_t _zero_pin;
        gpio_num_t _out_pin;
        gpio_num_t _fade_led;
        nvs_handle_t _disk;

        fade_param_t fade_param;
        esp_timer_handle_t zero_timer;

        static void IRAM_ATTR int_handler(void *args);
        static void sem_task(void* arg);
        static void fade_task(void* arg);
        static void zero_callback(void *arg); 
        static void dali_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);
        void convert_long(long_addr_t *s);
        

        volatile uint16_t raw_pwm_time=0;  
        uint64_t start_time = 0 ;
        uint16_t raw_min = 9500;
        uint16_t raw_max = 500;
        float power_step = (raw_min-raw_max) / 253.0;
        uint8_t cmd_val;

        uint8_t actual_level  = 0;
        uint8_t last_actual_level  = 0;
        uint8_t power_on_level  = 35;
        uint8_t system_failure_level = 35;
        uint8_t min_level  = 35;
        uint8_t max_level  = 254;
        uint8_t fade_time = 6;
        uint8_t fade_rate = 7;
        uint8_t extended_fade_time_base = 1; //1-16
        uint8_t extended_fade_time_multiplier = 1; //0-4
        uint8_t scene[16] ; //Senaryo degerlerini tutar. 
        uint8_t gear_groups[2]; //Bit bazlı çalışır   
        uint8_t short_address = 255;
        uint8_t search_address[3];
        uint8_t random_address[3];
        uint8_t DTR0=0;
        uint8_t DTR1=0;
        uint8_t DTR2=0;
        uint8_t operating_mode=0;
        uint8_t Light_sourge_type=4;


        bool control_Gear_Failure = false;
        bool lamp_failure = false;
        bool lamp_on = false;
        bool limit_error = false;
        volatile bool fade_running = false;
        bool reset_state = false;
        bool power_cycle_seen = false;

        uint8_t version = 1;
        uint8_t physical_min_level = 35;
        uint8_t device_type = 4;
        bool enable_write_memory = false;
        uint8_t bank0[255];
        uint8_t bank1[255];


        void nvm_variable_process(rw_status_t stat);

        uint16_t raw_level(uint8_t level) 
        {
            return raw_max + (raw_min-(level * power_step));
        };

        void change_actual_level(uint8_t level);
        
        uint32_t get_raw_fade_time(void)
        {
            //Fade süresini belirler milisaniye cinsinden 
            switch( fade_time ) 
            { 
                case 1: return 700;
                case 2: return 1000;
                case 3: return 1400;
                case 4: return 2000;
                case 5: return 2800;
                case 6: return 4000; //def
                case 7: return 5700;
                case 8: return 8000;
                case 9: return 11300;
                case 10: return 16000;
                case 11: return 22600;
                case 12: return 32000;
                case 13: return 45300;
                case 14: return 64000;
                case 15: return 90500;
            };
            return 0;
        };  

        uint16_t get_raw_fade_rate(void)
        {
            //dönen deger step cinsindendir. Fade hızını belirler step/sn
            switch(fade_rate)
            {
                case  1 : return 357;
                case  2 : return 253;
                case  3 : return 178;
                case  4 : return 126;
                case  5 : return 89;
                case  6 : return 63;
                case  7 : return 45; //def
                case  8 : return 32;
                case  9 : return 22;
                case  10 : return 16;
                case  11 : return 11;
                case  12 : return 8;
                case  13 : return 6;
                case  14 : return 4;
                case  15 : return 3;
            }
            return 0;
        };

        uint16_t get_multiplayer(void)
        {
            //ms olarak döndürür
            switch(extended_fade_time_multiplier) 
            {
                case 0 : return 0;
                case 1 : return 100;
                case 2 : return 1000;
                case 3 : return 10000;
                case 4 : return 60000;
            }
            return 0;
        } 

        esp_err_t nvm(const char* name, uint8_t* adr, rw_status_t stat, uint8_t def)
        {
            esp_err_t ret = ESP_OK;

            if (stat==NM_INIT)
              {
                 ret = nvs_get_u8(_disk,name,adr);
                 if (ret!=ESP_OK) {
                    *adr = def;
                    ret=nvs_set_u8(_disk,name,*adr);
                    nvs_commit(_disk);
                 }
              }
            if (stat==NM_READ) ret = nvs_get_u8(_disk,name,adr);
            if (stat==NM_WRITE) {ret = nvs_set_u8(_disk,name,*adr); nvs_commit(_disk);}
            if (stat==NM_RESET) {
                *adr = def;
                ret=nvs_set_u8(_disk,name,*adr);
                nvs_commit(_disk);
                ret = nvs_get_u8(_disk,name,adr);
            }
            return ret;
        }  


    protected: 
        void default_fade_param(uint8_t level, fade_time_calc_t calc_type, direction_t dir);       

};

#endif