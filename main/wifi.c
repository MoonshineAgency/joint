#include "wifi.h"

#include <esp_netif.h>
#include <lwip/ip_addr.h>
#include "common.h"
#include "settings.h"
#include "bus.h"

static esp_netif_t *iface = NULL;
static wifi_mode_t wifi_mode;

static void log_ip_info()
{
    if (!iface) return;

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(iface, &ip_info);

    ESP_LOGI(TAG, "--------------------------------------------------");
    ESP_LOGI(TAG, "IP address: " IPSTR, IP2STR(&ip_info.ip));
    ESP_LOGI(TAG, "Netmask:    " IPSTR, IP2STR(&ip_info.netmask));
    ESP_LOGI(TAG, "Gateway:    " IPSTR, IP2STR(&ip_info.gw));
    ESP_LOGI(TAG, "--------------------------------------------------");

    esp_netif_dns_info_t dns;
    for (esp_netif_dns_type_t t = ESP_NETIF_DNS_MAIN; t < ESP_NETIF_DNS_MAX; t++)
    {
        esp_netif_get_dns_info(iface, t, &dns);
        ESP_LOGI(TAG, "DNS %d:      " IPSTR, t, IP2STR(&dns.ip.u_addr.ip4));
    }
    ESP_LOGI(TAG, "--------------------------------------------------");
}

static void set_ip_info()
{
    esp_err_t res;

    if (!settings.wifi.ip.dhcp || wifi_mode == WIFI_MODE_AP)
    {
        if (wifi_mode == WIFI_MODE_AP)
            esp_netif_dhcps_stop(iface);
        else
            esp_netif_dhcpc_stop(iface);

        esp_netif_ip_info_t ip_info;
        ip_info.ip.addr = ipaddr_addr(settings.wifi.ip.ip);
        ip_info.netmask.addr = ipaddr_addr(settings.wifi.ip.netmask);
        ip_info.gw.addr = ipaddr_addr(settings.wifi.ip.gateway);
        res = esp_netif_set_ip_info(iface, &ip_info);
        if (res != ESP_OK)
            ESP_LOGW(TAG, "Error setting IP address %d (%s)", res, esp_err_to_name(res));

        if (wifi_mode == WIFI_MODE_AP)
            esp_netif_dhcps_start(iface);
    }

    esp_netif_dns_info_t dns;
    dns.ip.type = IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = ipaddr_addr(settings.wifi.ip.dns);
    res = esp_netif_set_dns_info(iface,
        wifi_mode != WIFI_MODE_AP && settings.wifi.ip.dhcp
        ? ESP_NETIF_DNS_FALLBACK
        : ESP_NETIF_DNS_MAIN,
        &dns);
    if (res != ESP_OK)
        ESP_LOGW(TAG, "Error setting DNS address %d (%s)", res, esp_err_to_name(res));
}

static void wifi_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    esp_err_t res = ESP_OK;
    switch (event_id)
    {
        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "WiFi started in access point mode");
            set_ip_info();
            log_ip_info();
            bus_send_event(NETWORK_UP, NULL, 0);
            break;
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WiFi started in station mode, connecting...");
            if ((res = esp_wifi_connect()) != ESP_OK)
                ESP_LOGE(TAG, "WiFi error %d [%s]", res, esp_err_to_name(res));
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi connected to '%s'", settings.wifi.sta.ssid);
            set_ip_info();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "WiFi disconnected, reconnecting...");
            bus_send_event(NETWORK_DOWN, NULL, 0);
            if ((res = esp_wifi_connect()) != ESP_OK)
                ESP_LOGE(TAG, "WiFi error %d [%s]", res, esp_err_to_name(res));
            break;
        default:
            break;
    }
}

static void ip_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    esp_err_t res = ESP_OK;
    switch (event_id)
    {
        case IP_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "WiFi got IP address");
            log_ip_info();
            bus_send_event(NETWORK_UP, NULL, 0);
            break;
        case IP_EVENT_STA_LOST_IP:
            ESP_LOGI(TAG, "WiFi lost IP address, reconnecting");
            bus_send_event(NETWORK_DOWN, NULL, 0);
            if ((res = esp_wifi_connect()) != ESP_OK)
                ESP_LOGE(TAG, "WiFi error %d [%s]", res, esp_err_to_name(res));
            break;
        default:
            break;
    }
}

static esp_err_t init_ap()
{
    ESP_LOGI(TAG, "Starting WiFi in access point mode");

    wifi_mode = WIFI_MODE_AP;

    // create default interface
    iface = esp_netif_create_default_wifi_ap();
    CHECK(esp_netif_set_hostname(iface, APP_NAME));

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    CHECK(esp_wifi_init(&init_cfg));

    CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_handler, NULL, NULL));

    wifi_config_t wifi_cfg = { 0 };
    memcpy(wifi_cfg.ap.ssid, settings.wifi.ap.ssid, sizeof(wifi_cfg.ap.ssid));
    wifi_cfg.ap.ssid_len = strlen((char *) settings.wifi.ap.ssid);
    memcpy(wifi_cfg.ap.password, settings.wifi.ap.password, sizeof(wifi_cfg.ap.password));
    wifi_cfg.ap.max_connection = settings.wifi.ap.max_connection;
    wifi_cfg.ap.authmode = strlen((char *) settings.wifi.ap.password)
                           ? settings.wifi.ap.authmode
                           : WIFI_AUTH_OPEN;
    wifi_cfg.ap.channel = settings.wifi.ap.channel;

    ESP_LOGI(TAG, "WiFi access point settings:");
    ESP_LOGI(TAG, "--------------------------------------------------");
    ESP_LOGI(TAG, "SSID: %s", wifi_cfg.ap.ssid);
    ESP_LOGI(TAG, "Channel: %d", wifi_cfg.ap.channel);
    ESP_LOGI(TAG, "Auth mode: %s", wifi_cfg.ap.authmode == WIFI_AUTH_OPEN ? "Open" : "WPA2/PSK");
    ESP_LOGI(TAG, "--------------------------------------------------");

    CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg));

    return ESP_OK;
}

static esp_err_t init_sta()
{
    ESP_LOGI(TAG, "Starting WiFi in station mode, connecting to '%s'", settings.wifi.sta.ssid);

    wifi_mode = WIFI_MODE_STA;

    iface = esp_netif_create_default_wifi_sta();
    CHECK(esp_netif_set_hostname(iface, settings.system.name));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    CHECK(esp_wifi_init(&cfg));

    CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_handler, NULL, NULL));
    CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_handler, NULL, NULL));
    CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &ip_handler, NULL, NULL));

    wifi_config_t wifi_cfg = { 0 };
    memcpy(wifi_cfg.sta.ssid, settings.wifi.sta.ssid, strlen((char *)settings.wifi.sta.ssid));
    memcpy(wifi_cfg.sta.password, settings.wifi.sta.password, strlen((char *)settings.wifi.sta.password));
    wifi_cfg.sta.threshold.authmode = strlen((const char *) settings.wifi.sta.password)
                                      ? settings.wifi.sta.threshold.authmode
                                      : WIFI_AUTH_OPEN;

    ESP_LOGI(TAG, "WiFi station settings:");
    ESP_LOGI(TAG, "--------------------------------------------------");
    ESP_LOGI(TAG, "SSID: %s", wifi_cfg.sta.ssid);
    //ESP_LOGI(TAG, "Passwd: %s", wifi_cfg.sta.password);
    ESP_LOGI(TAG, "--------------------------------------------------");

    CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));

    return ESP_OK;
}

esp_err_t wifi_init()
{
    CHECK(esp_netif_init());
    CHECK(esp_event_loop_create_default());

    if (settings.system.safe_mode)
        CHECK(init_ap());
    else
        CHECK(init_sta());

    return esp_wifi_start();
}
