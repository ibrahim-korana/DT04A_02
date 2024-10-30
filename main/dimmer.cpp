#include "dimmer.h"

#include <rom/ets_sys.h>
#include "esp_timer.h"
#include "math.h"
#include "esp_log.h"
#include "bootloader_random.h"
#include <sys/random.h>


const char *DIMMER_TAG = "DIMMER"; 

ESP_EVENT_DEFINE_BASE(DALI_EVENTS);

void Dimmer::dali_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
  printf("DALI EVENT OLUSTU\n");
}

void Dimmer::Initialize(gpio_num_t zeropin, gpio_num_t outpin, gpio_num_t fade_led)
{
            _zero_pin = zeropin;
            _out_pin = outpin;
            _fade_led = fade_led;
            char *nm = (char *)malloc(20);
            sprintf(nm,"Stor%d",_out_pin);
            ESP_ERROR_CHECK(nvs_open(nm, NVS_READWRITE, &_disk));
            free(nm);
            physical_min_level = 35;
            device_type = 4;
            nvm_variable_process(NM_INIT);
            bank0[0]=0xff; //Son erişilebilir adres
            bank1[0]=0xff; //Son erişilebilir adres
            bank1[1]=0x01; //Indicator byte
            bank0[2]=0x01; //Son bank adresi
            bank0[9] =0x01; //Firmware major
            bank0[10] =0x01; //firmware minor
            bank0[13] =0x01; //Hardware major
            bank0[14] =0x01; //Hardware minor
           
            gpio_config_t io_conf = {};
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pin_bit_mask = (1ULL<<_out_pin) | (1ULL << _fade_led);
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE; 
            gpio_config(&io_conf);
            gpio_set_level(_out_pin,0);
            gpio_set_level(_fade_led,0);

            gpio_config_t intConfig;
            intConfig.pin_bit_mask = (1ULL<<_zero_pin);
            intConfig.mode         = GPIO_MODE_INPUT;
            intConfig.pull_up_en   = GPIO_PULLUP_ENABLE;
            intConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
            intConfig.intr_type    = GPIO_INTR_DISABLE;
            gpio_config(&intConfig);
            
            gpio_set_intr_type(_zero_pin, GPIO_INTR_NEGEDGE);
	        gpio_isr_handler_add(_zero_pin, int_handler, (void *)this);
            gpio_intr_disable(_zero_pin);

           // scene[0] = 50;
           // scene[1] = 254;
            
           // printf("Min=%d Max=%d\n",min_level,max_level);
 
            esp_timer_create_args_t zero_args = {};
            zero_args.callback = &zero_callback,
            zero_args.arg = (void*) this,
            zero_args.name = "one-shot";
            ESP_ERROR_CHECK(esp_timer_create(&zero_args, &zero_timer)); 

            esp_event_handler_instance_register(DALI_EVENTS,ESP_EVENT_ANY_ID,dali_handler,this,NULL);

            xTaskCreate(sem_task, "task_02", 2048, (void *)this, 2, NULL);
}
void Dimmer::sem_task(void *args)
{
    Dimmer *self = (Dimmer *)args;   
    for (;;) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        self->power_cycle_seen = true;
    }
}


 void Dimmer::zero_callback(void *arg)
{
    Dimmer *self = (Dimmer *)arg; 
    self->power_cycle_seen = false;
    if (self->lamp_on)
        {  
            gpio_set_level(self->_out_pin, 1);
            ets_delay_us(50);
            gpio_set_level(self->_out_pin, 0);
        }
}

void IRAM_ATTR  Dimmer::int_handler(void *args)
{
    Dimmer *self = (Dimmer *)args;    
    esp_timer_stop(self->zero_timer);
    esp_timer_start_once(self->zero_timer, self->raw_pwm_time-20);
    if (gpio_get_level(self->_zero_pin)==0) 
        gpio_set_intr_type(self->_zero_pin, GPIO_INTR_POSEDGE);
    else
        gpio_set_intr_type(self->_zero_pin, GPIO_INTR_NEGEDGE);    
}

void Dimmer::change_actual_level(uint8_t level)
{
    last_actual_level = actual_level;
    actual_level = level;
    ESP_ERROR_CHECK(nvm("actual",&actual_level,NM_WRITE,(uint8_t)0));

}

void Dimmer::start(void)
{    
    uint8_t lev = power_on_level;
    printf("Power level %d Actual Lev %d\n", power_on_level, actual_level);
    if (power_on_level==255) lev=actual_level;
    raw_pwm_time = raw_level(lev);
    actual_level = lev;
    ESP_ERROR_CHECK(nvm("actual",&actual_level,NM_WRITE,(uint8_t)0));
    if (raw_pwm_time<=10000) lamp_on = true;
    gpio_intr_enable(_zero_pin);
    ESP_LOGW(DIMMER_TAG,"Dimmer Start");
}

void Dimmer::fade_task(void* arg)
{
     Dimmer *self = (Dimmer *)arg;
     if (!self->fade_running)
     {
        self->fade_running = true;
        if (self->_fade_led!=GPIO_NUM_NC) gpio_set_level(self->_fade_led,1);
        ESP_LOGI(DIMMER_TAG,"Fade Start");
        //printf("Fade Start time=%d start=%d stop=%d step=%d\n",self->fade_param.step_time,self->fade_param.step_start,self->fade_param.step_stop,self->fade_param.step) ; 
        bool sart = true; 
        uint16_t tmp = self->fade_param.step_start;
        uint16_t tim_ms = self->fade_param.step_time / 1000;
        uint16_t tim_us = self->fade_param.step_time % 1000;
        //printf("ms %d us %d step time %d adim %d\n", tim_ms, tim_us, self->fade_param.step_time, self->fade_param.step_start-self->fade_param.step_stop);
        if (tim_ms>10) tim_ms-= 10; //Executedaki komutlar nedeniyle Total surede dongu basına 10ms bir artış sozkonusu. Bu artış düşürülüyor. 
        //uint32_t ttt = esp_timer_get_time();
        while (sart)
        {
            self->raw_pwm_time = tmp;
            //taskYIELD();
            ets_delay_us(tim_us); //Bekleme microsaniye cinsinden olacak
            vTaskDelay(tim_ms/portTICK_PERIOD_MS); //Bekleme milisaniye cinsinden olacak
            if (self->fade_param.direction==DIR_UP) {
                //Step up
                tmp -= self->fade_param.step;
                sart = (tmp>self->fade_param.step_stop);
            }
            if (self->fade_param.direction==DIR_DOWN) {
                //Step down
                tmp += self->fade_param.step;
                sart = (tmp<self->fade_param.step_stop);
            }   
        };

        //uint32_t rrr = esp_timer_get_time();
        self->change_actual_level(self->fade_param.destination); 
        self->fade_running = false;
        if (self->_fade_led!=GPIO_NUM_NC) gpio_set_level(self->_fade_led,0);
        ESP_LOGI(DIMMER_TAG,"Fade Stop");
        //printf("Fade Stop %d\n",rrr-ttt);
     }
     vTaskDelete(NULL);
};

void Dimmer::default_fade_param(uint8_t level, fade_time_calc_t calc_type, direction_t dir)
{
     uint8_t level0 = level;
     if (calc_type==CALC_200_MS)
      {
        uint8_t des = ((get_raw_fade_rate()/1000.0)*200.0); //200ms deki adim sayısı
        if (dir==DIR_UP) {
                des= actual_level + des;
                if (des>=max_level) des = max_level;
            } else {
                des= actual_level - des;
                if (des<=min_level) des = min_level;
            }
        level0 = des;
      }

    fade_param.actual      = actual_level;
    fade_param.destination = level0;
    fade_param.step_start  = raw_level(actual_level);
    fade_param.step_stop   = raw_level(level0);
    fade_param.step        = 1;
    fade_param.direction   = dir; 
    fade_param.total_step  = abs(fade_param.step_stop-fade_param.step_start);
    fade_param.step_time   = 1;
    uint8_t adim_sayisi = abs(level0-actual_level);
    if (calc_type==CALC_FADE_TIME)
      {
         uint32_t tm = 0;
         if (fade_time==0) {
            //fade time degeri microsaniye olarak
            tm = (get_multiplayer() * extended_fade_time_base) * 1000;
         } else {
            tm = get_raw_fade_time()*1000; 
         }
         if (adim_sayisi>0) 
         {
            fade_param.step_time   = tm/adim_sayisi;
            fade_param.step = fade_param.total_step / adim_sayisi;
         }
      }
    if (calc_type==CALC_200_MS) 
      {
        fade_param.step_time = (200.0/(adim_sayisi)) * 1000;
        fade_param.step = fade_param.total_step / adim_sayisi;
      }
    if (calc_type==CALC_FADE_RATE) 
      {  
          // abs(level0-actual_level)/get_raw_fade_rate() saniye cinsinden geçiş süresini hesaplar bunu microsaniyeye çevirip
          // adım sayısına bölersek fade işleminin her bir adımında beklenmesi gereken süreyi microsaniye cinsinden buluruz
          // Her bir adımda (power level) ne kadar raw level atlayacagını da step parametresine koyarız.  
          fade_param.step_time = ((adim_sayisi/get_raw_fade_rate())*1000.0*1000.0)/adim_sayisi;
          fade_param.step = fade_param.total_step / adim_sayisi;
      }    
}


bool Dimmer::direct_arc_power(uint16_t level)
{
    //Actual_level degerini Min-Max level arasında direkt değiştirmek için kullanılır. 
    //Actual_level değişimi fade ile olur. Fade süresi fade_time da belirlenen süredir.     
    if (!fade_running)
     { 
        if (level>=min_level && level<=max_level)
            {
                int action_level = actual_level - level;
                lamp_on = true;
                direction_t dr = DIR_UP;
                if (action_level>0) {dr   = DIR_DOWN;}
                default_fade_param(level, CALC_FADE_TIME, dr);              
                xTaskCreate(fade_task, "task_03", 2048, (void *)this, 2, NULL); 
            } else {
              limit_error = true;
              return false;
            }
            return true;
     }
    return false;
};

bool Dimmer::command_off(void)
{   
    //Lambayı dim olmadan kapatır. Actual_level olduğu gibi kalır. 
    if (lamp_on)
    { 
        cmd_val = 0x00;
        lamp_on = false;
        last_actual_level = actual_level; 
        return true;
    }
    return false;
};

bool Dimmer::command_up(uint8_t *action)
{
    //Bulunulan nokta (actual_level) fade_rate dikkate alınarak yukarıya doğru fade ile ışık şiddetine artırır.
    //Lamba kapalı ise açılmayacaktır. Toplam fade suresi 200ms dir.
    //Eger Actual_level max_level a esitse komut etkisizdir.

    //fade rate 1 sn deki adım sayısını belirler. Komut 200ms bir artış öngörür. Bu nedenle
    //öncelikle 200ms kaç adım atacagımızı buluyoruz. (des = ((get_raw_fade_rate()/1000.0)*200.0))
    //adım sayısını actual_level a ekleyerek hedef nokta tespit ediliyor.  
    //default_fade_param bu degerlere göre kaç raw step olduğunu hesaplıyarak total_step içine koyuyor
    //toplam step 200ms atılacagından  200/fade_param.total_step ile her adımdan sonra ne kadar belkenmesi gerektiği
    //bulunuyor. 
    

    if (lamp_on && actual_level<max_level && !fade_running)
    {
        default_fade_param(0, CALC_200_MS, DIR_UP); 
        limit_error=false;
        if (fade_param.destination>max_level) {fade_param.destination = max_level;limit_error=true;}
        if (*action>0) xTaskCreate(fade_task, "task_03", 2048, (void *)this, tskIDLE_PRIORITY, NULL); 
        
         *action = fade_param.destination; 
         return true;
    };
    return false;
}

bool Dimmer::command_down(uint8_t *action)
{
    //Bulunulan nokta (actual_level) fade_rate dikkate alınarak aşağıya doğru fade ile ışık şiddetine azaltır.
    //Lamba kapalı ise açılmayacaktır. Toplam fade suresi 200ms dir.
    //Eger Actual_level min_level a esitse komut etkisizdir. (Bkz UP)
    
    if (lamp_on && actual_level>min_level && !fade_running)
    {
       limit_error=false;
       default_fade_param(0, CALC_200_MS, DIR_DOWN); 
       if (fade_param.destination<min_level) {fade_param.destination = min_level;limit_error=true;}
       if (*action>0) xTaskCreate(fade_task, "task_03", 2048, (void *)this, tskIDLE_PRIORITY, NULL); 
       *action = fade_param.destination; 
       return true;
    };
    return false;
}

bool Dimmer::command_step_up(uint8_t *action)
{
    if ((actual_level+1)>max_level) limit_error=true;
    if (lamp_on && (actual_level+1)<=max_level)
    {
       if (*action>0) {
            raw_pwm_time = raw_level(actual_level+1);
            change_actual_level(actual_level+1);
            limit_error=false;
       } 
       *action = actual_level+1;
       return true;
    }
    return false;
}

bool Dimmer::command_step_down(uint8_t *action)
{
    if ((actual_level-1)<min_level) limit_error=true;
    if (lamp_on && (actual_level-1)>=min_level)
    {
       if (*action>0) { 
       raw_pwm_time = raw_level(actual_level-1);
       change_actual_level(actual_level-1);
       limit_error=false;
       }
       *action = actual_level-1;
    }
    return true;
}

bool Dimmer::command_recall_max_level(void)
{
    limit_error=false;
    if (!lamp_on) lamp_on=true;
    raw_pwm_time = raw_level(max_level);
    change_actual_level(max_level);
    return true;
}
bool Dimmer::command_recall_min_level(void)
{
    limit_error=false;
    if (!lamp_on) lamp_on=true;
    raw_pwm_time = raw_level(min_level);
    change_actual_level(min_level);
    return true;
}

bool Dimmer::command_step_down_and_off(uint8_t *action)
{
     if ((actual_level-1)<min_level) limit_error=true;
    if ((actual_level-1)>=min_level)
    {
       if (*action>0)
       { 
       raw_pwm_time = raw_level(actual_level-1);
       change_actual_level(actual_level-1);
       if (actual_level==min_level) lamp_on = false;
       }
       *action = actual_level-1;
       limit_error=false;
       return true;
    } 
    return false;
}
bool Dimmer::command_on_and_step_up(uint8_t *action)
{
    if ((actual_level+1)>=min_level && (actual_level+1)<=max_level)
    {
       if (*action>0)
       { 
        raw_pwm_time = raw_level(actual_level+1);
        change_actual_level(actual_level+1);
        lamp_on = true;
       }
       *action = actual_level+1;
       limit_error = false;
       return true;
    } else limit_error = true;
    return false;
}

bool Dimmer::command_goto_last_active_level(uint8_t *action)
{
    if(!fade_running)
    { 
        if (last_actual_level>0)
        {
            if (*action>0)
            {
            if (!lamp_on) lamp_on = true;
            direction_t dir = DIR_UP;
            if (actual_level>last_actual_level) dir = DIR_DOWN;
            default_fade_param(last_actual_level,CALC_FADE_TIME,dir);
            xTaskCreate(fade_task, "task_03", 2048, (void *)this, tskIDLE_PRIORITY, NULL); 
            }
            *action = last_actual_level;
            return true;
        } 
    }
    return false;
}

bool Dimmer::command_continuous_up(uint8_t *action)
{
    if(!fade_running && lamp_on)
    { 
        if (actual_level<max_level)
        {
            if (*action>0)
            {
                default_fade_param(max_level,CALC_FADE_RATE,DIR_UP);
                xTaskCreate(fade_task, "task_03", 2048, (void *)this, tskIDLE_PRIORITY, NULL); 
            }
            *action=max_level;
            limit_error=false;
            return true;
        } else limit_error=true;
    }
    return false;
}
bool Dimmer::command_continuous_down(uint8_t *action)
{
   if(!fade_running && lamp_on)
   { 
        if (actual_level>min_level)
        {
            if (*action>0)
            {
                default_fade_param(min_level,CALC_FADE_RATE,DIR_DOWN);
                xTaskCreate(fade_task, "task_03", 2048, (void *)this, tskIDLE_PRIORITY, NULL); 
            }
            *action = min_level;
            limit_error=false;
            return true;
        } else limit_error=true;
   }
    return false;
}

bool Dimmer::command_goto_scene(uint8_t scn, uint8_t *action)
{
   if(!fade_running)
   { 
        uint8_t lvl = scene[scn];
        
        if (lvl<255)
        {
            limit_error=false;
            if (lvl<min_level) {lvl=min_level; limit_error=true;}
            if (lvl>max_level) {lvl=max_level; limit_error=true;}
            if (*action>0)
              {
                if (!lamp_on) lamp_on = true;
                direction_t dir = DIR_UP;
                if (actual_level>lvl) dir = DIR_DOWN;            
                default_fade_param(lvl,CALC_FADE_TIME,dir);
                xTaskCreate(fade_task, "task_03", 2048, (void *)this, tskIDLE_PRIORITY, NULL); 
              }            
            *action = lvl;
        };
        return true;
   }
    return false;
}

uint8_t Dimmer::command_query(uint8_t comm)
{
    switch(comm)
    {
        case 0x90 : {
            //Query Status
            uint8_t aa = 0x00;
            if (control_Gear_Failure) aa = aa|0x01;
            if (lamp_failure) aa = aa|0x02;
            if (lamp_on) aa = aa|0x04;
            if (limit_error) aa = aa|0x08;
            if (fade_running) aa = aa|0x10;
            if (reset_state) aa = aa|0x20;
            if (short_address==255) aa = aa|0x40;
            if (power_cycle_seen) aa = aa|0x80;
            return aa;
        }
        break;
        case 0x91: {
            if (control_Gear_Failure) return 0xFF; else return 0x00;
        }
        break;
        case 0x92: {
            if (lamp_failure) return 0xFF; else return 0x00;
        }
        break;
        case 0x93:{
            if (lamp_on) return 0xFF; else return 0x00;
        }
        case 0x94:{
            if (limit_error) return 0xFF; else return 0x00;
        }
        case 0x95:{
            if (reset_state) return 0xFF; else return 0x00;
        }
        case 0x96:{
            if (short_address==255) return 0xFF; else return 0x00;
        }
        case 0x97: {
            return version;
        }
        case 0x98: {
            return DTR0;
        }
        case 0x99: {
            return 4; //IncandescentLamp
        }
        case 0x9A: {
            return physical_min_level;
        }
        case 0x9B: {
            return power_cycle_seen;
        }
        case 0x9C: {
            return DTR1;
        }
        case 0x9D: {
            return DTR2;
        }
        case 0x9E: {
            return operating_mode;
        }
        case 0x9F: {
            return Light_sourge_type;
        }
        case 0xA0 : {
            return actual_level;
        }
        case 0xA1 : {
            return max_level;
        }
        case 0xA2 : {
            return min_level;
        }
        case 0xA3 : {
            return power_on_level;
        }
        case 0xA4 : {
            return system_failure_level;
        }
        case 0xA5 : {
            return fade_time<<4 | fade_rate;
        }
        case 0xA6 : {
            //Boş
            return 0;
        }
        case 0xA7 : {
            //Boş
            return 0;
        }
        case 0xA8 : {
            return extended_fade_time_multiplier<<4 | extended_fade_time_base;
        }
        case 0xAA : {
            return control_Gear_Failure;
        }
        case 0xB0 ... 0xBF : {
            uint8_t aa = comm & 0x0F;
            return scene[aa];
        }
        case 0xC0 : {
            return gear_groups[0];
        }
        case 0xC1 : {
            return gear_groups[1];
        }
        case 0xC2 : {
            return random_address[0];
        }
        case 0xC3 : {
            return random_address[1];
        }
        case 0xC4 : {
            return random_address[2];
        }
        case 0xC5 : {
            return bank1[DTR0];
        }

        default: {return 0;} break;
    }
}

void Dimmer::convert_long(long_addr_t *s)
{
    s->random.data.data0 = random_address[2];
    s->random.data.data1 = random_address[1];
    s->random.data.data2 = random_address[0];

    s->search.data.data0 = search_address[2];
    s->search.data.data1 = search_address[1];
    s->search.data.data2 = search_address[0];
}

uint8_t Dimmer::special_command(uint8_t comm, uint8_t dat)
{
    uint8_t ret = 0x00;
    switch(comm) {
        case 0: break;
        case 1: {DTR0 = dat; } break;
        case 3: {
                  getrandom((uint8_t *)random_address,4,0); 
                  printf("%02X%02X%02X\n",random_address[0],random_address[1],random_address[2]); 
                };
                break;
        case 4: {
                     long_addr_t s;
                     convert_long(&s); 
                     //printf("compare %08X<=%08X\n",(unsigned int)s.random.long_num, (unsigned int)s.search.long_num);
                     if (s.is_less_than_equal()) ret = (uint8_t)0xFF;
                }
                break; 
        case 0x5: {
                    long_addr_t s;
                    convert_long(&s); 
                    if (s.is_equal()) ret = (uint8_t)0xFF;
                  } break;               
        case 0x8: {search_address[0]=dat;}; break;
        case 0x9: {search_address[1]=dat;}; break;
        case 0xA: {search_address[2]=dat;}; break;
        case 0xB: {
                    long_addr_t s;
                    convert_long(&s); 
                    //printf("set %08X<=%08X\n",(unsigned int)s.random.long_num, (unsigned int)s.search.long_num);
                    if (s.is_equal()) {
                         short_address = (dat>>1) & 0x3F;
                         printf("ATAMA Shot ADD = %02X\n",short_address);
                         ESP_ERROR_CHECK(nvm("short_address",&short_address,NM_WRITE,(uint8_t)255));
                       }
                  } break;
        case 0x0C : {
                     long_addr_t s;
                     convert_long(&s); 
                     if (short_address == ((dat>>1) & 0x3F) && s.is_equal())
                        ret = (uint8_t)0xFF;
                    } break;          
        case 0x0D : return short_address;
        default : break;
    }
    //printf("DTR 0 = %d\n",DTR0);
    return ret;
}

uint8_t Dimmer::command_config(uint8_t comm, bool reply)
{   
     uint8_t ret = 0x00;
    if (reply)
    {
        if (first_special==0xFF)
        {
                //İlk komut geldi Hafızaya al
                first_special = comm;
                active_special = 0xFF;
                if (esp_timer_is_active(special_timer)) esp_timer_stop(special_timer);
                esp_timer_start_once(special_timer,150000);
        } else {
            if (first_special==comm) {
                //İkinci komut ile ilki eşit işlem yap
                active_special=comm;
                first_special = 0xFF;
                if (esp_timer_is_active(special_timer)) esp_timer_stop(special_timer);
                ESP_LOGI("SPECIAL_COMMAND","Command Special %02x",active_special);
            } else {
                //eşit degil işlem yapma
                first_special =0xFF;
                active_special=0xFF;
                if (esp_timer_is_active(special_timer)) esp_timer_stop(special_timer);
            }
        }
    } else {active_special = comm;first_special=0xFF;}
    

    switch(comm) {
        case 0x20 : {
            //Reset
            actual_level = 254;
            last_actual_level = 0;
            physical_min_level = 35;
            device_type = 4;
            control_Gear_Failure = false;
            lamp_failure = false;
            lamp_on = false;
            limit_error = false;
            fade_running = false;
            reset_state = true;
            power_cycle_seen = false;
            nvm_variable_process(NM_RESET);
        }
        break;
        case 0x21:{DTR0 = actual_level;} break;
        case 0x22:{nvm_variable_process(NM_WRITE);}break;
        case 0x23:{operating_mode = DTR0;}break;
        case 0x24:break;//{dali2}break;
        case 0x25:break;
        case 0x2A:{max_level=DTR0;ESP_ERROR_CHECK(nvm("max_level",&max_level,NM_WRITE,(uint8_t)0));}break;
        case 0x2B:{min_level=DTR0;ESP_ERROR_CHECK(nvm("min_level",&min_level,NM_WRITE,(uint8_t)0));}break;
        case 0x2C:{system_failure_level=DTR0;ESP_ERROR_CHECK(nvm("system_failure",&system_failure_level,NM_WRITE,(uint8_t)0));}break;
        case 0x2D:{power_on_level=DTR0;ESP_ERROR_CHECK(nvm("power_on_level",&power_on_level,NM_WRITE,(uint8_t)0));}break;
        case 0x2E:{fade_time=DTR0;ESP_ERROR_CHECK(nvm("fade_time",&fade_time,NM_WRITE,(uint8_t)0));}break;
        case 0x2F:{fade_rate=DTR0;ESP_ERROR_CHECK(nvm("fade_rate",&fade_rate,NM_WRITE,(uint8_t)0));}break;
        case 0x30:{extended_fade_time_base=DTR0&0x0F;
                   ESP_ERROR_CHECK(nvm("time_base",&extended_fade_time_base,NM_WRITE,(uint8_t)0));
                   extended_fade_time_multiplier = DTR0>>4;
                   ESP_ERROR_CHECK(nvm("time_multip",&extended_fade_time_multiplier,NM_WRITE,(uint8_t)0));
                   }break;
        case 0x40 ... 0x4F: {
            uint8_t aa = comm & 0x0F;
            char *mm = (char*)malloc(10);
            sprintf(mm,"scene%d",aa);
            scene[aa]=DTR0;
            ESP_ERROR_CHECK(nvm(mm,&scene[aa],NM_WRITE,(uint8_t)255));
            free(mm);
            break;
        }        
        case 0x50 ... 0x5F : {
            uint8_t aa = comm & 0x0F;
            char *mm = (char*)malloc(10);
            sprintf(mm,"scene%d",aa);
            scene[aa]=255;
            ESP_ERROR_CHECK(nvm(mm,&scene[aa],NM_WRITE,(uint8_t)255));
            free(mm);
            break;
        }
        case 0x60 ... 0x6F: {
            uint8_t aa=comm&0x0F;
            if ((aa&0x08)!=0x08) {
                uint8_t bb = pow(2,aa);
                gear_groups[0] = gear_groups[0] | bb;
                ESP_ERROR_CHECK(nvm("gear0",&gear_groups[0],NM_WRITE,(uint8_t)0));
            } else {
                uint8_t bb = pow(2,(aa&0x07));
                gear_groups[1] = gear_groups[1] | bb;
                ESP_ERROR_CHECK(nvm("gear1",&gear_groups[1],NM_WRITE,(uint8_t)0));
            }
        } break;
        case 0x70 ... 0x7F: {
            uint8_t aa=comm&0x0F;
            if ((aa&0x08)!=0x08) {
                uint8_t bb = 0xFF - pow(2,aa);
                gear_groups[0] = gear_groups[0] & bb;
                ESP_ERROR_CHECK(nvm("gear0",&gear_groups[0],NM_WRITE,(uint8_t)0));
            } else {
                uint8_t bb = 0xFF - pow(2,(aa&0x07));
                gear_groups[1] = gear_groups[1] & bb;
                ESP_ERROR_CHECK(nvm("gear1",&gear_groups[1],NM_WRITE,(uint8_t)0));
            }
        } break;
        case 0x80: {
            short_address = DTR0; ESP_ERROR_CHECK(nvm("short_address",&short_address,NM_WRITE,(uint8_t)255));
        } break;
        case 0x81: {
            enable_write_memory = true;
        } break;

        default: return 0; break;
    }
    return 0;
}

void Dimmer::nvm_variable_process(rw_status_t stat)
{
    ESP_ERROR_CHECK(nvm("actual",&actual_level,stat,(uint8_t)254));
    ESP_ERROR_CHECK(nvm("power_on_level",&power_on_level,stat,(uint8_t)254));
    ESP_ERROR_CHECK(nvm("system_failure",&system_failure_level,stat,(uint8_t)254));
    ESP_ERROR_CHECK(nvm("min_level",&min_level,stat,physical_min_level));
    ESP_ERROR_CHECK(nvm("max_level",&max_level,stat,(uint8_t)254));
    ESP_ERROR_CHECK(nvm("fade_rate",&fade_rate,stat,(uint8_t)7));
    ESP_ERROR_CHECK(nvm("fade_time",&fade_time,stat,(uint8_t)0));
    ESP_ERROR_CHECK(nvm("time_base",&extended_fade_time_base,stat,(uint8_t)1));
    ESP_ERROR_CHECK(nvm("time_multip",&extended_fade_time_multiplier,stat,(uint8_t)0));
    if (stat!=NM_RESET) ESP_ERROR_CHECK(nvm("short_address",&short_address,stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("search0",&search_address[0],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("search1",&search_address[1],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("search2",&search_address[2],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("random0",&random_address[0],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("random1",&random_address[1],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("random2",&random_address[2],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("gear0",&gear_groups[0],stat,(uint8_t)0));
    ESP_ERROR_CHECK(nvm("gear1",&gear_groups[1],stat,(uint8_t)0));
    ESP_ERROR_CHECK(nvm("scene0",&scene[0],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("scene1",&scene[1],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("scene2",&scene[2],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("scene3",&scene[3],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("scene4",&scene[4],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("scene5",&scene[5],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("scene6",&scene[6],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("scene7",&scene[7],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("scene8",&scene[8],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("scene9",&scene[8],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("scene10",&scene[10],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("scene11",&scene[11],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("scene12",&scene[12],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("scene13",&scene[13],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("scene14",&scene[14],stat,(uint8_t)255));
    ESP_ERROR_CHECK(nvm("scene15",&scene[15],stat,(uint8_t)255));
}