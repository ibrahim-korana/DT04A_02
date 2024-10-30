#ifndef __CIHAZLAR_H__
#define __CIHAZLAR_H__

#include "lwip/err.h"
#include "lwip/ip.h"
#include "string.h"
#include "esp_log.h"
#include <lwip/etharp.h>

typedef struct device_register {
    uint8_t device_id;
    uint32_t ip;
    uint8_t active;
    uint8_t oldactive;
    char mac[14];
    uint8_t mac0[6];
    void *next;
} device_register_t;

class Cihazlar 
{
    public:
        Cihazlar(){};
        ~Cihazlar(){};
        device_register_t *get_handle(void) {return dev_handle;}
        device_register_t *cihaz_ekle(uint8_t *mac, uint8_t id);

        esp_err_t cihaz_sil(const char *mac);
        uint8_t cihaz_say(void);
        esp_err_t cihaz_list(void);
        esp_err_t cihaz_bosalt(void);
        esp_err_t update_ip(const char *mac,uint32_t ip);
        device_register_t *cihazbul(char *mac);
        device_register_t *cihazbul(uint8_t id);
        eth_addr *get_sta_mac(const uint32_t &ip, char *mac);
        void start(bool st) {status = st;}


        void init(void)
          {
            dev_handle = NULL;
            xTaskCreate(garbage, "garbage", 2048, (void *) this, 5,NULL);
            buf = (char *)calloc(1,14);
          }

        bool status=false;

    private:
        device_register_t *dev_handle = NULL;   
        device_register_t *_cihaz_sil(device_register_t *deletedNode,const char *mac);  
        static void garbage(void *arg);
        char *buf;
        char *mac_to_str(uint8_t *mc);

};

#endif