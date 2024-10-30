
#ifndef _CORE_H
#define _CORE_H

#include <stdio.h>
#include "esp_event.h"

typedef struct {
    uint8_t open_default;
    uint8_t atama_sirasi;
    uint8_t random_mac;
    uint8_t rawmac[4];
    uint8_t mqtt_server[64];
    uint8_t dali_server[64];
    uint16_t mqtt_keepalive;
    uint8_t device_id;
    uint8_t project_number;
    uint8_t http_start;
    uint8_t tcpserver_start;
    uint8_t daliserver_start;
    uint8_t comminication;
    uint8_t time_sync;
    uint8_t short_addr;
    uint8_t group;
    uint8_t power;
    uint8_t type1;
    uint8_t type2;
    uint8_t type3;
} config_t;

enum {
	LED_EVENTS_ETH,
	LED_EVENTS_WIFI,
	LED_EVENTS_MQTT,
    TCP_EVENTS_DATA,
    MQTT_EVENTS_DATA,
     SYSTEM_RESET,
    SYSTEM_DEFAULT_RESET,

};

typedef struct {
     char *data;
     int data_len;
     int socket;
     int port;
} tcp_events_data_t;

typedef enum {
    HOME_WIFI_DEFAULT = 0,
    HOME_WIFI_AP,     //Access point oluşturur
    HOME_WIFI_STA,    //Access station oluşturur
    HOME_WIFI_AUTO,   //Access station oluşturur. AP bağlantı hatası oluşursa AP oluşturur 
    HOME_ETHERNET,    //SPI ethernet
    HOME_NETWORK_DISABLE,//Wifi kapatır   
} home_wifi_type_t;

typedef enum {
    IP_DEFAULT = 0,
    STATIC_IP, 
    DYNAMIC_IP, 
} home_ipstat_type_t;

typedef enum {
    WAN_DEFAULT = 0,
    WAN_ETHERNET,
    WAN_WIFI,
} home_wan_type_t;

typedef struct {
    uint8_t home_default;
    home_wifi_type_t wifi_type;
    home_wan_type_t wan_type;
    uint8_t wifi_ssid[32];        //AP Station de kullanılacak SSID
    uint8_t wifi_pass[32];        //AP Station de kullanılacak Şifre 
    home_ipstat_type_t ipstat;
    uint8_t ip[17];               //Static ip 
    uint8_t netmask[17];          //Static netmask
    uint8_t gateway[17];          //Static gateway
    uint32_t home_ip;
    uint32_t home_netmask;
    uint32_t home_gateway;
    uint32_t home_broadcast;
    uint8_t mac[6];
    uint8_t channel;
    uint8_t dns[17]; 
    uint8_t backup_dns[17]; 
    uint8_t WIFI_MAXIMUM_RETRY;
} home_network_config_t;

typedef struct {
    uint8_t active;
    uint8_t on1saat;
    uint8_t on1dakika;
    uint8_t on2saat;
    uint8_t on2dakika;
    uint8_t off1saat;
    uint8_t off1dakika;
    uint8_t off2saat;
    uint8_t off2dakika;
    uint8_t power1; 
    uint8_t power2; 
} zaman_t;

typedef struct {
    uint8_t type; //1 grup 2 scene
    uint8_t command; //1 on 2 off
    uint8_t addr;
    uint8_t power;
} zaman_job_t;

/*
      EVENT
*/
typedef struct {
	uint8_t state;
} led_events_data_t;

ESP_EVENT_DECLARE_BASE(LED_EVENTS);
ESP_EVENT_DECLARE_BASE(SYSTEM_EVENTS);

typedef void (*web_callback_t)(home_network_config_t net, config_t glob);



#endif