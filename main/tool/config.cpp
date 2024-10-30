
void Button_Config(gpio_num_t btno, const char *num, button_handle_t *bt)
{

    button_config_t *cfg;
    cfg = (button_config_t *)calloc(1,sizeof(button_config_t));
    cfg->type = BUTTON_TYPE_GPIO;
    cfg->gpio_button_config = {
        .gpio_num = btno,
        .active_level=0  
    };
    char *bt_num = (char*)calloc(1,3); strcpy(bt_num,num);
    *bt = iot_button_create(cfg);
    iot_button_register_cb(*bt,BUTTON_ALL_EVENT,btn_callback, bt_num);
}

void default_config(void)
{
    ESP_LOGW(TAG,"DEFAULT CONFIG");
     GlobalConfig.open_default = 1;
     GlobalConfig.atama_sirasi = 1;
     GlobalConfig.random_mac = 0;

     strcpy((char *)GlobalConfig.mqtt_server,"mqtt.smartq.com.tr");
     GlobalConfig.mqtt_keepalive = 30;
     GlobalConfig.device_id = 1;
     GlobalConfig.project_number=1;
     GlobalConfig.http_start=1;
     GlobalConfig.tcpserver_start=1;
     GlobalConfig.daliserver_start=0;
     GlobalConfig.comminication=0;
     GlobalConfig.time_sync=0;
     GlobalConfig.short_addr = 0;
     GlobalConfig.group = 0;
     GlobalConfig.power = 0;
     GlobalConfig.type1 = 6;
     GlobalConfig.type2 = 6;
     GlobalConfig.type3 = 6;

     strcpy((char *)GlobalConfig.dali_server,"192.168.7.1");

     disk.file_control(GLOBAL_FILE);
     disk.write_file(GLOBAL_FILE,&GlobalConfig,sizeof(GlobalConfig),0);
}

void network_default_config(void)
{
    ESP_LOGW(TAG,"NETWORK DEFAULT CONFIG");
     NetworkConfig.home_default = 1;
     NetworkConfig.wifi_type = HOME_WIFI_AP;
     
        NetworkConfig.wan_type = WAN_ETHERNET;
        
     NetworkConfig.ipstat = DYNAMIC_IP;
     //strcpy((char*)NetworkConfig.wifi_ssid, "Lords Palace");
     strcpy((char*)NetworkConfig.wifi_pass, "");
     strcpy((char*)NetworkConfig.ip,"192.168.7.1");
     strcpy((char*)NetworkConfig.netmask,"255.255.255.0");
     strcpy((char*)NetworkConfig.gateway,"192.168.7.1");
     strcpy((char*)NetworkConfig.dns,"4.4.4.4");
     strcpy((char*)NetworkConfig.backup_dns,"8.8.8.8");
     NetworkConfig.channel = 1;
     NetworkConfig.WIFI_MAXIMUM_RETRY=5;
/*
    NetworkConfig.wifi_type = HOME_WIFI_STA;
    strcpy((char *)NetworkConfig.wifi_ssid,(char *)"Akdogan_2.4G");
    strcpy((char *)NetworkConfig.wifi_pass,(char *)"651434_2.4");

    NetworkConfig.wifi_type = HOME_WIFI_STA;
    strcpy((char *)NetworkConfig.wifi_ssid,(char *)"Baguette Modem");
    strcpy((char *)NetworkConfig.wifi_pass,(char *)"Baguette2024");
*/

/*
    NetworkConfig.wifi_type = HOME_WIFI_STA;
    strcpy((char *)NetworkConfig.wifi_ssid,(char *)"SMQ");
    strcpy((char *)NetworkConfig.wifi_pass,(char *)"12345678");
*/    
    NetworkConfig.wifi_type = HOME_WIFI_STA;
    strcpy((char *)NetworkConfig.wifi_ssid,(char *)"IMS_YAZILIM");
    strcpy((char *)NetworkConfig.wifi_pass,(char *)"mer6514a4c");
    
    disk.file_control(NETWORK_FILE);
    disk.write_file(NETWORK_FILE,&NetworkConfig,sizeof(NetworkConfig),0);
}


void config(void)
{
    gpio_config_t intConfig = {};
    intConfig.pin_bit_mask = (1ULL<<LED);
    intConfig.mode         = GPIO_MODE_OUTPUT;
    intConfig.pull_up_en   = GPIO_PULLUP_ENABLE;
    intConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    intConfig.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&intConfig);

    gpio_set_level(LED,0);
    bool kk=false;
    gpio_set_level(LED,kk);

    kk=!kk;
    for (int i=0;i<4;i++)
    {
       gpio_set_level(LED,kk); 
       kk=!kk;
       vTaskDelay(200 / portTICK_PERIOD_MS);
    }

    gpio_install_isr_service(0);
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    //NVS Initialize    
    //ESP_ERROR_CHECK(nvs_flash_erase());
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    ESP_LOGI(TAG,"Disc Init");
    //disk.format();
    ESP_ERROR_CHECK(!disk.init());

    disk.read_file(GLOBAL_FILE,&GlobalConfig,sizeof(GlobalConfig), 0);
	if (GlobalConfig.open_default==0 ) {
		//Global ayarlar diskte kayıtlı değil. Kaydet.
		 default_config();
		 disk.read_file(GLOBAL_FILE,&GlobalConfig,sizeof(GlobalConfig),0);
		 FATAL_MSG(GlobalConfig.open_default,"Global Initilalize File ERROR !...");
	}

    disk.read_file(NETWORK_FILE,&NetworkConfig,sizeof(NetworkConfig), 0);
	if (NetworkConfig.home_default==0 ) {
		//Network ayarları diskte kayıtlı değil. Kaydet.
		 network_default_config();
		 disk.read_file(NETWORK_FILE,&NetworkConfig,sizeof(NetworkConfig),0);
		 FATAL_MSG(NetworkConfig.home_default, "Network Initilalize File ERROR !...");
	}


    disk.list("/config/","*.*");

    Button_Config(BUTTON1,"3",&btn3);
    Button_Config(BUTTON2,"2",&btn2);
    Button_Config(BUTTON3,"1",&btn1);
    Button_Config(BUTTON4,"4",&btn4);

    

}