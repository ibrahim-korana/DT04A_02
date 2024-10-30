
#include "now.h"

uint8_t broadcast_id = 255;
uint8_t dev_id;
        
const char *ENTAG = "TR_ESPNOW";
static uint8_t broadcast_mac[MAC_LEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static QueueHandle_t receive_queue;
static QueueHandle_t send_queue;
SemaphoreHandle_t ack_sem = NULL;
SemaphoreHandle_t ping_sem = NULL;
SemaphoreHandle_t pingack_sem = NULL;
bool send_ok = false;
bool ping_ok = false;
static uint8_t ping_active = 0;
volatile bool send_active = false;
now_callback_t main_callback = NULL;
now_callback_t broadcast_callback = NULL;
Cihazlar *cihaz;


static void receive_callback(const esp_now_recv_info_t * esp_now_info, const uint8_t *data, int len);
static void send_callback(const uint8_t *mac_addr, esp_now_send_status_t status);

void receive_task(void *arg)
{
   // EspNOW *mthis = (EspNOW *)arg;
    receive_cb_t *par;
    while(1)
    {
        xQueueReceive(receive_queue, &par, (TickType_t)portMAX_DELAY);
        //printf("GELEN Mac : %s\n", Adr.mac_to_string(par->mac)); 
        
        data_t *buf = (data_t *)par->data;
        bool normal=true;
        
        //buf->payload[buf->len] = 0;
        if (buf->type==DATA_PING)
        {
            /*
            if (led!=GPIO_NUM_NC) gpio_set_level(led,1);
            normal=false; 
            if (!send_active) _pingpong(buf->sender,DATA_PONG);             
            if (led!=GPIO_NUM_NC) {
                vTaskDelay(5/portTICK_PERIOD_MS);
                gpio_set_level(led,0);
            }
            */
        }
        if (buf->type==DATA_PONG)
        {
            normal=false;
            ping_ok = true;
            xSemaphoreGive(ping_sem);
        }  
        if (normal)      
        {
            if (buf->receiver==255)
                {
                    if (broadcast_callback!=NULL) 
                    broadcast_callback(buf->payload,buf->len,NULL);
                } else {
                    if (main_callback!=NULL) 
                        main_callback(buf->payload,buf->len,NULL);
                }  
            /*
            uint8_t pk = findpk(buf->sender);
            if (pk==255) pk=findfreepk(buf->sender, buf->total_len);
            if (pk==255) {
            ESP_LOGE(ENTAG, "BOS PAKET YOK"); 
            return; 
            }
            //printf("PK=%d\n",pk);
            memcpy(paket[pk]->data+((buf->current_pk-1)*NPAKET_SIZE),buf->payload,buf->len);
            paket[pk]->header = (1ULL<<buf->current_pk);        
            if(pow(2,buf->total_pk)==paket[pk]->header)
            {
                if (buf->receiver==255)
                {
                    if (broadcast_callback!=NULL) 
                    broadcast_callback(paket[pk]->data,buf->sender,TR_ESPNOW);
                } else {
                if (main_callback!=NULL) 
                    main_callback(paket[pk]->data,buf->sender,TR_ESPNOW);
                }
                //printf("TAMAMLANDI %s\n",paket[pk]->data);           
                free(paket[pk]->data);
                paket[pk]->data = NULL;
                paket[pk]->header = 0;
                paket[pk]->id = 0;           
            }
            */
        }
        free(par->data) ;  
        free(par) ;  
    }
    vTaskDelete(NULL);
}

void send_task(void *arg)
{
    send_cb_t *par;
    while(1)
    {
        xQueueReceive(send_queue, &par, (TickType_t)portMAX_DELAY);
        if (!ping_active) 
         {
            send_ok=false;
            
                if (par->status == ESP_NOW_SEND_SUCCESS) send_ok=true;
                xSemaphoreGive(ack_sem);
                                                
         } else xSemaphoreGive(pingack_sem);      
        free(par);
    }
    vTaskDelete(NULL);
}

void send_callback(const uint8_t *mac_addr, esp_now_send_status_t status)
{
        send_cb_t *par = (send_cb_t *)malloc(sizeof(send_cb_t));
        memcpy(par->mac,mac_addr,MAC_LEN);
        par->status = status;
        if (xQueueSend(send_queue, ( void * ) &par, pdMS_TO_TICKS(5000) ) != pdTRUE) {
                ESP_LOGW(ENTAG, "Send Send queue fail");
                free(par);
            } 
}

void add_peer(const uint8_t *mac, uint8_t id)
{
    esp_now_peer_info_t *peer = (esp_now_peer_info_t *)malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(ENTAG, "Malloc peer information fail");
        return ;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));    
    peer->channel = ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = false;
    memcpy(peer->peer_addr, mac, MAC_LEN);
    ESP_ERROR_CHECK( esp_now_add_peer(peer) );
        /* 
        char * mc =(char *)malloc(16);
        strcpy(mc,Adr.mac_to_string(mac));
        //printf("EKLENEN MAC %s\n",mc);
        device_register_t *cc = cihaz->cihaz_ekle(mc,TR_ESPNOW);
        cc->device_id = id;
        memcpy(cc->mac0, mac, MAC_LEN);
        free(mc);
        */

    free(peer);  
};

void receive_callback(const esp_now_recv_info_t * esp_now_info, const uint8_t *data, int len)
{
    uint16_t crc, crc_cal = 0;
    data_t *buf = (data_t *)data;
    crc = buf->crc;
    buf->crc = 0;
    crc_cal = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, len);
    if (crc_cal == crc)
    {
        if (!esp_now_is_peer_exist(esp_now_info->src_addr)) add_peer(esp_now_info->src_addr,buf->sender);
        receive_cb_t *par = (receive_cb_t *)malloc(sizeof(receive_cb_t));
        memcpy(par->mac,esp_now_info->src_addr,MAC_LEN);
        par->data = (uint8_t *)malloc(len);
        memcpy(par->data,data,len);       
        if (xQueueSend(receive_queue, ( void * ) &par, pdMS_TO_TICKS(5000) ) != pdTRUE) {
                ESP_LOGW(ENTAG, "Send receive queue fail");
                free(par->data);
                free(par);
            } 
    }   
}

void init_wifi(void)
{
    //ESP_ERROR_CHECK(esp_netif_init());
    //ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(ESPNOW_WIFI_MODE) );
    ESP_ERROR_CHECK( esp_wifi_start());
    ESP_ERROR_CHECK( esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK( esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
}

void init_espnow(void)
{
    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_send_cb(send_callback) );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(receive_callback) );
    ESP_ERROR_CHECK( esp_now_set_pmk((uint8_t *)"1234567890123456") );
    add_peer(broadcast_mac,broadcast_id);
    xTaskCreate(receive_task, "receive_task", 4096, NULL, 4, NULL);
    xTaskCreate(send_task, "send_task", 2048, NULL, 2, NULL);
}

void EspNOW_init(uint8_t id) 
{
        dev_id = id;               
        receive_queue = xQueueCreate(QUEUE_SIZE, sizeof(receive_cb_t *));
        send_queue = xQueueCreate(QUEUE_SIZE, sizeof(send_cb_t *));
        assert(receive_queue);
        ack_sem = xSemaphoreCreateBinary();
        assert(ack_sem);
        ping_sem = xSemaphoreCreateBinary();
        assert(ping_sem);
        pingack_sem = xSemaphoreCreateBinary();
        assert(pingack_sem);

        assert(ack_sem); 
        init_wifi();
        init_espnow();
}

void EspNOW_set_callback(now_callback_t cb)
{
    main_callback = cb;
}

void EspNOW_set_broadcast_callback(now_callback_t cb)
{
    broadcast_callback = cb;
}

uint8_t data_prepare(send_param_t *send_param, const char *data)
{
    data_t *buf = (data_t *)send_param->buffer;
    buf->type = send_param->type;
    buf->crc = 0;
    buf->sender = send_param->sender;
    buf->receiver = send_param->receiver;
    buf->len = strlen(data);
    memcpy(buf->payload,data, buf->len);
    uint8_t len = buf->len+10;
    buf->crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, len);
    return len;
}

esp_err_t _send(const char *data, uint8_t id)
{
    
    send_active = true;
    device_register_t *cln = cihaz->cihazbul(id);

    if (cln!=NULL)
      {
        //Paketler Hesaplanıyor 
        ESP_LOGI(ENTAG, "GIDEN >> [%d] %s",id,data);
        uint16_t size = strlen((char*)data);
        
        send_param_t *send_param = (send_param_t *)malloc(sizeof(send_param_t));
        memset(send_param, 0, sizeof(send_param_t));
            if (send_param == NULL) {
                ESP_LOGE(ENTAG, "Malloc send parameter fail");
                send_active=false;
                return ESP_FAIL;
            }
        send_param->type     = (id==255)? DATA_BROADCAST:DATA_UNICAST;
        send_param->sender   = dev_id;
        send_param->receiver = id;
        send_param->len  = size;
        send_param->buffer = (uint8_t *)malloc(200);
            
        uint8_t len = data_prepare(send_param,data);
        if (esp_now_send(cln->mac0, (uint8_t *)send_param->buffer, len) != ESP_OK) {
                    ESP_LOGE(ENTAG, "Send error");
                }
           
        
            free(send_param->buffer);
            free(send_param);
            
      } else {send_active=false;return ESP_FAIL;}
      
    send_active = false;   
    return ESP_OK;
}

void EspNOW_set_cihazlar(Cihazlar *chz)
{
    cihaz = chz;
}


esp_err_t EspNOW_Broadcast(const char *data)
{
    while (ping_active>0) vTaskDelay(20/ portTICK_PERIOD_MS);
    return _send(data,255);
}

