
#ifndef _TRIAC_DIM_H_
#define _TRIAC_DIM_H_

#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "storage.h"
#include <math.h>
#include "dali_global.h"




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


ESP_EVENT_DEFINE_BASE(DIMMER_EVENTS);

enum {
    DIMMER_BUSY_START,
    DIMMER_BUSY_END,
};

typedef struct {
        uint8_t actual_level;
        uint8_t last_actual_level;
        uint8_t power_on_level;
        uint8_t system_failure_level;
        uint8_t min_level;
        uint8_t max_level;
        uint8_t fade_time ;
        uint8_t fade_rate;
        uint8_t extended_fade_time_base ; //1-16
        uint8_t extended_fade_time_multiplier ; //0-4
        uint8_t scene[16] ; //Senaryo degerlerini tutar. 
        uint8_t gear_groups[2]; //Bit bazlı çalışır   
        uint8_t short_address;
        uint8_t search_address[3];
        uint8_t random_address[3];
        uint8_t DTR0;
        uint8_t DTR1;
        uint8_t DTR2;
        uint8_t operating_mode;
        uint8_t Light_sourge_type;

        //---------
        uint8_t up_on;
        uint8_t up_off;
        uint8_t down_on;
        uint8_t down_off;
        uint8_t enable_dt7;

        uint8_t version;
        uint8_t physical_min_level ;
        uint8_t device_type;
        bool enable_write_memory ;
        uint8_t bank0[255];
        uint8_t bank1[255];
        uint8_t lamp_on;

        uint8_t def;
} variable_t;

typedef enum {
    DIR_UP = 0,
    DIR_DOWN,
} direction_t;

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

class TriacDimmer {
    public:
        TriacDimmer() {};
        ~TriacDimmer() {};
        void init(gpio_num_t zeropin, gpio_num_t outpin, gpio_num_t ld, Storage *dsk);
        void start(void);

        uint8_t command_query(uint8_t comm);

        bool direct_arc_power(uint16_t level);
        bool direct_power(uint16_t level);
        bool command_off(void);
        bool command_up(void);
        bool command_down(void);
        bool command_step_up(void);
        bool command_step_down(void);


        /*
        uint8_t get_level();
        esp_err_t set_level(uint8_t level,uint16_t fade);
        esp_err_t set_color(uint16_t color);
        uint16_t get_color(void) {return config.color;}
        esp_err_t toggle_level(void);
        void without_fade_change(uint8_t level);
        void without_fade_change_nowrite(uint8_t level);

        bool is_on() {return var.lamp_on;}

        

        bool command_goto_last_active_level_with_adr(uint8_t *act, address_t adr);
        bool command_goto_last_active_level(uint8_t *act);

        bool command_recall_max_level_with_adr(address_t adr);
        bool command_recall_max_level(void);

        bool command_recall_min_level_with_adr(address_t adr);
        bool command_recall_min_level(void);

        bool command_step_down_and_off_with_adr(uint8_t *act, address_t adr);
        bool command_step_down_and_off(uint8_t *act);

        bool command_on_and_step_up_with_adr(uint8_t *act, address_t adr);
        bool command_on_and_step_up(uint8_t *act);

        bool command_goto_scene_with_adr(uint8_t scn, uint8_t *act, address_t adr);
        bool command_goto_scene(uint8_t scn, uint8_t *act);

        uint8_t command_query_with_adr(uint8_t comm, address_t adr, uint8_t *ret);
        uint8_t command_query(uint8_t comm);


        uint8_t special_command(uint8_t comm, uint8_t dat, bool reply=false);

        uint8_t command_config_with_adr(uint8_t comm, address_t adr, bool reply=true);
        uint8_t command_config(uint8_t comm, bool reply=true);
        */
        uint8_t get_actual_level(void) {return var.actual_level;}
        uint8_t get_max_level(void) {return var.max_level;}
        uint8_t get_min_level(void) {return var.min_level;}
        uint8_t get_power_on_level(void) {return var.power_on_level;}
        uint8_t get_failure_level(void) {return var.system_failure_level;}
       

        bool get_random(long_addr_t *rd) {
           rd->random.data.data0 = var.random_address[2];
           rd->random.data.data1 = var.random_address[1];
           rd->random.data.data2 = var.random_address[0];
           return true;
        }
        bool set_random(long_addr_t *rd) {
           var.random_address[2] = rd->random.data.data0;
           var.random_address[1] = rd->random.data.data1;
           var.random_address[0] = rd->random.data.data2;
           return true;
        }

        bool is_fade_running(void) {return fade_running;}
        bool is_open(void) {return var.lamp_on;}

        bool is_short_address(uint8_t addr) {
            if (var.short_address>64) return false;
            if (var.short_address==0xFF) return false;
            if (var.short_address==addr) return true;
            return false;
            }
        uint8_t get_short_address(void) {return var.short_address;}   
        void set_short_address(uint8_t addr) {var.short_address=addr;disk->write_file(file_name,&var,sizeof(variable_t),0);} 
        void clear_short_address(void) {var.short_address=0xFF;disk->write_file(file_name,&var,sizeof(variable_t),0); } 
        bool is_group_address(uint8_t addr) {
            if (addr>15) return false;
            if (var.gear_groups[0]==0xFF && var.gear_groups[1]==0xFF) return false;
            if (((uint8_t)pow(2,addr)&var.gear_groups[0])>0x00) return true;
            if (addr>8) {
                uint8_t tmp=addr-8;
                if (((uint8_t)pow(2,tmp)&var.gear_groups[1])>0x00) return true;
            }
            return false;
        } 

        uint8_t type = 4;

    protected:
    //Türetilenden erişilir

        gpio_num_t _zero_pin;
        gpio_num_t _out_pin;
        gpio_num_t Led = GPIO_NUM_NC;
        Storage *disk;
        variable_t var={};        
        char *file_name;

        SemaphoreHandle_t kilit;
        
        
        bool control_Gear_Failure = false;
        bool lamp_failure = false;
        bool limit_error = false;
        bool fade_running = false;
        bool reset_state = false;
        bool power_cycle_seen = false;

        uint32_t get_raw_fade_time(void);
        uint16_t get_raw_fade_rate(void);
        uint16_t get_multiplayer(void);
       
       // void Reset(void);
       // void convert_long(long_addr_t *s);
        void led_on(void) 
        {
            if (Led!=GPIO_NUM_NC) gpio_set_level(Led,1);
        };
        void led_off(void) 
        {
            if (Led!=GPIO_NUM_NC) gpio_set_level(Led,0);
        };

       // void virtual_dim(uint8_t start, uint8_t stop, uint16_t time);

        esp_timer_handle_t special_timer;
        esp_timer_handle_t config_timer;
        uint8_t first_special=99, active_special=99;
        uint8_t first_config=99, active_config=99;

        volatile uint16_t raw_pwm_time=0;  
        uint64_t start_time = 0 ;
        uint16_t raw_min = 9500;
        uint16_t raw_max = 500;
        float power_step = (raw_min-raw_max) / 253.0;
        esp_timer_handle_t zero_timer;

        uint16_t raw_level(uint8_t level) 
        {
            return raw_max + (raw_min-(level * power_step));
        };

        void fade_calc(uint8_t level, fade_time_calc_t calc_type, direction_t dir); 
        static void fade_task(void* arg);
      

    private:
    //    static void virtual_fade_task(void *arg);
        fade_param_t fade_param;
        static void IRAM_ATTR int_handler(void *args);
        static void zero_callback(void *arg); 
        static void special_timer_callback(void *arg);
        static void config_timer_callback(void *arg);

        

};



#endif