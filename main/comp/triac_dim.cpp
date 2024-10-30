
#include "triac_dim.h"
#include <rom/ets_sys.h>

const char *DIMM_TAG = "DIMMER";

void TriacDimmer::zero_callback(void *arg)
{
    //
    TriacDimmer *self = (TriacDimmer *)arg; 
    if (self->var.lamp_on)
        {  
            gpio_set_level(self->_out_pin, 1);
            ets_delay_us(50);
            gpio_set_level(self->_out_pin, 0);
        }
}
/*
    Interrupt zero cross kenarlarını takip eder. 
    Int her oluştugunda sinus dalgasının başlangıç kenarı
    aktif olmuştur. Bu nedenle dime baglı olarak (zero timer)
    triac tetiklenir. Triac sinüs dalgası düşerken kendisi 
    kapanacaktır. Bu nedenle dalga başlangıcından başlayarak 
    X süre sonra triac zero_timer tarafından tetiklenir.
*/
void IRAM_ATTR  TriacDimmer::int_handler(void *args)
{
    TriacDimmer *self = (TriacDimmer *)args;    
    esp_timer_stop(self->zero_timer);
    esp_timer_start_once(self->zero_timer, self->raw_pwm_time-20);
    if (gpio_get_level(self->_zero_pin)==0) 
        gpio_set_intr_type(self->_zero_pin, GPIO_INTR_POSEDGE);
    else
        gpio_set_intr_type(self->_zero_pin, GPIO_INTR_NEGEDGE);    
}

void TriacDimmer::init(gpio_num_t zeropin, gpio_num_t outpin, gpio_num_t ld, Storage *dsk)
{
    Led = ld;
    disk = dsk;
    _zero_pin = zeropin;
    _out_pin = outpin;

    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL<<_out_pin) | (1ULL << Led);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE; 
    gpio_config(&io_conf);
    gpio_set_level(_out_pin,0);
    gpio_set_level(Led,0);

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
        
    file_name = (char*)calloc(1,20);
    sprintf(file_name,"/config/cfg%d.bin",_out_pin);     
    int zz = disk->file_size(file_name);

    if (zz==0) {
        memset(&var,0,sizeof(var));
        var.actual_level  = 0;
        var.last_actual_level  = 0;
        var.power_on_level  = 128;
        var.system_failure_level = 35;
        var.min_level  = 35;
        var.max_level  = 254;
        var.fade_time = 6;
        var.fade_rate = 7;
        var.extended_fade_time_base = 1; //1-16
        var.extended_fade_time_multiplier = 1; //0-4
        //var.scene[16] ; //Senaryo degerlerini tutar. 
        //var.gear_groups[2]; //Bit bazlı çalışır   
        var.short_address = 255;
        //uint8_t search_address[3];
        //uint8_t random_address[3];
        var.DTR0=0;
        var.DTR1=0;
        var.DTR2=0;
        var.operating_mode=0;
        var.Light_sourge_type=4;

        var.version = 1;
        var.physical_min_level = 35;
        var.device_type = 4;
        var.enable_write_memory = false;
        var.def = 1;

        disk->file_control(file_name);
        disk->write_file(file_name,&var,sizeof(variable_t),0);
        printf("default yazıldı %d\n",var.short_address);
    }
    disk->read_file(file_name,&var,sizeof(variable_t),0);
    fade_running = false;

    esp_timer_create_args_t arg0 = {};
    arg0.callback = special_timer_callback;
    arg0.arg = (void*) this;
    arg0.name = "sptm";
    ESP_ERROR_CHECK(esp_timer_create(&arg0, &special_timer));

    esp_timer_create_args_t arg1 = {};
    arg1.callback = config_timer_callback;
    arg1.arg = (void*) this;
    arg1.name = "cfgtm";
    ESP_ERROR_CHECK(esp_timer_create(&arg1, &config_timer));

    esp_timer_create_args_t zero_args = {};
    zero_args.callback = &zero_callback,
    zero_args.arg = (void*) this,
    zero_args.name = "one-shot";
    ESP_ERROR_CHECK(esp_timer_create(&zero_args, &zero_timer)); 
}


void TriacDimmer::start(void)
{    
    uint8_t lev = var.power_on_level;
    ESP_LOGI(DIMM_TAG,"Power level %d Actual Lev %d", var.power_on_level, var.actual_level);
    if (var.power_on_level==255) lev=var.actual_level;
    raw_pwm_time = raw_level(lev);
    var.actual_level = lev;
    if (raw_pwm_time<=10000) var.lamp_on = true;
    gpio_intr_enable(_zero_pin);
    ESP_LOGW(DIMM_TAG,"Triac Dimmer Start");
}

void TriacDimmer::special_timer_callback(void *arg)
{
    TriacDimmer *ths = (TriacDimmer *)arg;
    ths->first_special =0xFF;
    ths->active_special=0xFF;
    printf("special clear\n");
}

void TriacDimmer::config_timer_callback(void *arg)
{
    TriacDimmer *ths = (TriacDimmer *)arg;
    ths->first_config =0xFF;
    ths->active_config=0xFF;
    printf("config clear\n");
}

void TriacDimmer::fade_calc(uint8_t level, fade_time_calc_t calc_type, direction_t dir)
{
     uint8_t level0 = level;
     if (calc_type==CALC_200_MS)
      {
        uint8_t des = ((get_raw_fade_rate()/1000.0)*200.0); //200ms deki adim sayısı
        if (dir==DIR_UP) {
                des= var.actual_level + des;
                if (des>=var.max_level) des = var.max_level;
            } else {
                des= var.actual_level - des;
                if (des<=var.min_level) des = var.min_level;
            }
        level0 = des;
      }

    fade_param.actual      = var.actual_level;
    fade_param.destination = level0;
    fade_param.step_start  = raw_level(var.actual_level);
    fade_param.step_stop   = raw_level(level0);
    fade_param.step        = 1;
    fade_param.direction   = dir; 
    fade_param.total_step  = abs(fade_param.step_stop-fade_param.step_start);
    fade_param.step_time   = 1;
    uint8_t adim_sayisi = abs(level0-var.actual_level);
    if (calc_type==CALC_FADE_TIME)
      {
         uint32_t tm = 0;
         if (var.fade_time==0) {
            //fade time degeri microsaniye olarak
            tm = (get_multiplayer() * var.extended_fade_time_base) * 1000;
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

void TriacDimmer::fade_task(void* arg)
{
     TriacDimmer *self = (TriacDimmer *)arg;
     if (!self->fade_running)
     {
        self->fade_running = true;
        self->led_on();
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
        self->fade_running = false;
        uint8_t kanal = 0;
        esp_event_post(DIMMER_EVENTS,DIMMER_BUSY_END,&kanal,sizeof(uint8_t),portMAX_DELAY);
        self->led_off();
     }
     vTaskDelete(NULL);
};


//------------------------------------------
bool TriacDimmer::direct_power(uint16_t level)
{ 
    led_on();
    if (level>=var.min_level && level<=var.max_level)
        {
            var.lamp_on = true;
            raw_pwm_time = raw_level(level);
            led_off();
            return true;              
        } else {
            limit_error = true;
            ESP_LOGE(DIMM_TAG,"Limit error level=%02x Min=%02x Max=%02x",level,var.min_level,var.max_level);
            led_off();
            return false;
        }
}

bool TriacDimmer::direct_arc_power(uint16_t level)
{
    //Actual_level degerini Min-Max level arasında direkt değiştirmek için kullanılır. 
    //Actual_level değişimi fade ile olur. Fade süresi fade_time da belirlenen süredir.    
    if (!fade_running)
     { 
        ESP_LOGI(DIMM_TAG,"ArcPower %02X",level);
        if (level<var.min_level) level=var.min_level;
        if (level>var.max_level) level=var.max_level;
        //var.actual_level = level;
        if (level>=var.min_level && level<=var.max_level)
            {
                var.lamp_on = true;
                int action_level = var.actual_level - level;
                direction_t dr = DIR_UP;
                if (action_level>0) {dr   = DIR_DOWN;}
                fade_calc(level, CALC_FADE_TIME, dr);  

                var.last_actual_level = var.actual_level;  
                var.actual_level = level;

                xTaskCreate(fade_task, "task_03", 2048, (void *)this, 2, NULL); 
                uint8_t kanal = 0;
                esp_event_post(DIMMER_EVENTS,DIMMER_BUSY_START,&kanal,sizeof(uint8_t),portMAX_DELAY);
                disk->write_file(file_name,&var,sizeof(variable_t),0);
                
            } else {
              limit_error = true;
              ESP_LOGE(DIMM_TAG,"Limit error level=%02x Min=%02x Max=%02x",level,var.min_level,var.max_level);
              return false;
            }
            return true;
     }
    //ESP_LOGE(TAG,"Fade running error"); 
    return false;
}

bool TriacDimmer::command_off(void)
{
    //Lambayı dim olmadan kapatır. Actual_level 0 olur.
    led_on(); 
    if (var.lamp_on)
    { 
        ESP_LOGI(DIMM_TAG,"Off Command");
        var.lamp_on = false;
        var.last_actual_level = var.actual_level; 
        var.actual_level = 0;
        disk->write_file(file_name,&var,sizeof(variable_t),0);
        led_off();
        return true;
    }
    led_off();
    return false;
}

bool TriacDimmer::command_up(void)
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

    if (!fade_running)
    {
      if (var.lamp_on)
      {
        if (var.actual_level<var.max_level)
        {
            fade_calc(0, CALC_200_MS, DIR_UP); 
            limit_error=false;
            if (fade_param.destination>var.max_level) {fade_param.destination = var.max_level;limit_error=true;}
            uint8_t kanal = 0;
            esp_event_post(DIMMER_EVENTS,DIMMER_BUSY_START,&kanal,sizeof(uint8_t),portMAX_DELAY);
            xTaskCreate(fade_task, "task_03", 2048, (void *)this, tskIDLE_PRIORITY, NULL); 
            var.last_actual_level = var.actual_level; 
            var.actual_level = fade_param.destination;
            limit_error=false;  
            disk->write_file(file_name,&var,sizeof(variable_t),0);   
            return true;
        } else {
            limit_error=true;  
            ESP_LOGE(DIMM_TAG,"Limit error Max=%02x",var.max_level);
            return false;
            };
      } else ESP_LOGE(DIMM_TAG,"Lamp OFF");
    }
    ESP_LOGE(DIMM_TAG,"Fade running error"); 
    return false;
}

bool TriacDimmer::command_down(void)
{
    //Bulunulan nokta (actual_level) fade_rate dikkate alınarak yukarıya doğru fade ile ışık şiddetine artırır.
    //Lamba kapalı ise açılmayacaktır. Toplam fade suresi 200ms dir.
    //Eger Actual_level max_level a esitse komut etkisizdir. 

    if (!fade_running)
    {
      if (var.lamp_on)
      {
        if (var.actual_level>var.min_level)
        {
            fade_calc(0, CALC_200_MS, DIR_DOWN); 
            limit_error=false;
            if (fade_param.destination<var.min_level) {fade_param.destination = var.min_level;limit_error=true;}
            uint8_t kanal = 0;
            esp_event_post(DIMMER_EVENTS,DIMMER_BUSY_START,&kanal,sizeof(uint8_t),portMAX_DELAY);
            xTaskCreate(fade_task, "task_03", 2048, (void *)this, tskIDLE_PRIORITY, NULL); 
            var.last_actual_level = var.actual_level; 
            var.actual_level = fade_param.destination;
            limit_error=false;  
            disk->write_file(file_name,&var,sizeof(variable_t),0);   
            return true;
        } else {
            limit_error=true;  
            ESP_LOGE(DIMM_TAG,"Limit error Max=%02x",var.min_level);
            return false;
            };
      } else ESP_LOGE(DIMM_TAG,"Lamp OFF");
    }
    ESP_LOGE(DIMM_TAG,"Fade running error"); 
    return false;
}


bool TriacDimmer::command_step_up(void)
{
    if ((var.actual_level+1)>var.max_level) {
        limit_error=true;
        ESP_LOGE(DIMM_TAG,"Limit error Max=%02x",var.max_level);
    }
    if (var.lamp_on && (var.actual_level+1)<=var.max_level)
    {
        raw_pwm_time = raw_level(actual_level+1);
        var.last_actual_level = var.actual_level;
        var.actual_level = var.actual_level + 1;
        limit_error=false;
        disk->write_file(file_name,&var,sizeof(variable_t),0);
       return true;
    }
    ESP_LOGE(TAG,"Lamp OFF");
    return false;
}

bool TriacDimmer::command_step_down(void)
{
    if ((var.actual_level-1)>var.min_level) {
        limit_error=true;
        ESP_LOGE(DIMM_TAG,"Limit error Min=%02x",var.min_level);
    }
    if (var.lamp_on && (var.actual_level-1)>=var.min_level)
    {
        raw_pwm_time = raw_level(actual_level-1);
        var.last_actual_level = var.actual_level;
        var.actual_level = var.actual_level - 1;
        limit_error=false;
        disk->write_file(file_name,&var,sizeof(variable_t),0);
       return true;
    }
    ESP_LOGE(TAG,"Lamp OFF");
    return false;
}


//--------------------------------------------------
uint8_t TriacDimmer::command_query(uint8_t comm)
{
    switch(comm)
    {
        case 0x90 : {
            //Query Status
            uint8_t aa = 0x00;
            if (control_Gear_Failure) aa = aa|0x01;
            if (lamp_failure) aa = aa|0x02;
            if (var.lamp_on) aa = aa|0x04;
            if (limit_error) aa = aa|0x08;
            if (fade_running) aa = aa|0x10;
            if (reset_state) aa = aa|0x20;
            if (var.short_address==255) aa = aa|0x40;
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
            if (var.lamp_on) return 0xFF; else return 0x00;
        }
        case 0x94:{
            if (limit_error) return 0xFF; else return 0x00;
        }
        case 0x95:{
            if (reset_state) return 0xFF; else return 0x00;
        }
        case 0x96:{
            if (var.short_address==255) return 0xFF; else return 0x00;
        }
        case 0x97: {
            return var.version;
        }
        case 0x98: {
            return var.DTR0;
        }
        case 0x99: {
            return type; //Relay
        }
        case 0x9A: {
            return var.physical_min_level;
        }
        case 0x9B: {
            return power_cycle_seen;
        }
        case 0x9C: {
            return var.DTR1;
        }
        case 0x9D: {
            return var.DTR2;
        }
        case 0x9E: {
            return var.operating_mode;
        }
        case 0x9F: {
            return var.Light_sourge_type;
        }
        case 0xA0 : {
            return var.actual_level;
        }
        case 0xA1 : {
            return var.max_level;
        }
        case 0xA2 : {
            return var.min_level;
        }
        case 0xA3 : {
            return var.power_on_level;
        }
        case 0xA4 : {
            return var.system_failure_level;
        }
        case 0xA5 : {
            return var.fade_time<<4 | var.fade_rate;
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
            return var.extended_fade_time_multiplier<<4 | var.extended_fade_time_base;
        }
        case 0xAA : {
            return control_Gear_Failure;
        }
        case 0xB0 ... 0xBF : {
            uint8_t aa = comm & 0x0F;
            return var.scene[aa];
        }
        case 0xC0 : {
            return var.gear_groups[0];
        }
        case 0xC1 : {
            return var.gear_groups[1];
        }
        case 0xC2 : {
            return var.random_address[0];
        }
        case 0xC3 : {
            return var.random_address[1];
        }
        case 0xC4 : {
            return var.random_address[2];
        }
        case 0xC5 : {
            return var.bank1[var.DTR0];
        }
        case 0XF0 : {
           //DT7 Query Features 
           return 0x0A;
        }
        case 0XF1 : {
           //DT7 Query Switch Status 
           return var.lamp_on;
        }
        case 0XF2 : {
           //DT7 Query Up Switch On Threshold  
           return var.up_on;
        }
        case 0XF3 : {
           //DT7 Query Up Switch Off Threshold  
           return var.up_off;
        }
        case 0XF4 : {
           //DT7 Query Down Switch On Threshold 
           return var.down_on;
        }
        case 0XF6 : {
           //DT7 Query Error Hold
           return 0;
        }
        case 0XF7 : {
           //DT7 Query Gear Type
           return type;
        }
        default: {return 0;} break;
    }
}


//--------------------------------------------------
uint32_t TriacDimmer::get_raw_fade_time(void)
        {
            //Fade süresini belirler milisaniye cinsinden 
            switch( var.fade_time ) 
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

uint16_t TriacDimmer::get_raw_fade_rate(void)
        {
            //dönen deger step cinsindendir. Fade hızını belirler step/sn
            switch(var.fade_rate)
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

uint16_t TriacDimmer::get_multiplayer(void)
        {
            //ms olarak döndürür
            switch(var.extended_fade_time_multiplier) 
            {
                case 0 : return 0;
                case 1 : return 100;
                case 2 : return 1000;
                case 3 : return 10000;
                case 4 : return 60000;
            }
            return 0;
        } 
