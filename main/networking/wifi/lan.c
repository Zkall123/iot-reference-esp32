/* eth2ap (Ethernet to Wi-Fi AP packet forwarding) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
/* Ethernet Basic Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "ethernet_init.h"
#include "sdkconfig.h"
#include <freertos/event_groups.h>
#include "esp_mac.h"

#include "app_wifi.h"

#include "lan.h"
#include "extras/ledStrip.h"
//#include "sntpTime.h"

static const char *TAG = "Ethernet";

static const int ETH_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t eth_event_group;

uint8_t eth_port_cnt;
esp_eth_handle_t *eth_handles;

//ETH App callback
static eth_connect_event_callback_t EthConnectedEvent_cb;

//Boolean to show if its connectet
bool b_is_eth_connected = true;

uint8_t mac_addr[6] = {0};

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        b_is_eth_connected = false;
        RgbLedETHAppStarted();
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");


    b_is_eth_connected = true;
    if(EthConnectedEvent_cb)
    {
        EthCallCallback();
    }
    RgbLedETHConnected();
    xEventGroupSetBits(eth_event_group, ETH_CONNECTED_EVENT);
}

void EthSetCallback(eth_connect_event_callback_t cb)
{
    EthConnectedEvent_cb = cb;
    ESP_LOGI(TAG, "EthSetCallback!");
}

void EthCallCallback()
{
    EthConnectedEvent_cb();
    ESP_LOGI(TAG, "EthCallCallback!");
}

void start_ethernet(void)
{
    //RgbLedETHAppStarted();

    eth_event_group = xEventGroupCreate();

    // Initialize Ethernet driver
    eth_port_cnt = 0;
    ESP_ERROR_CHECK(example_eth_init(&eth_handles, &eth_port_cnt));

    // Initialize TCP/IP network interface aka the esp-netif (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background darf nicht wegen bits usw
    //ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create instance(s) of esp-netif for Ethernet(s)
    if (eth_port_cnt == 1) {
        // Use ESP_NETIF_DEFAULT_ETH when just one Ethernet interface is used and you don't need to modify
        // default esp-netif configuration parameters.
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        esp_netif_t *eth_netif = esp_netif_new(&cfg);
        // Attach Ethernet driver to TCP/IP stack
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));
    } else {
        // Use ESP_NETIF_INHERENT_DEFAULT_ETH when multiple Ethernet interfaces are used and so you need to modify
        // esp-netif configuration parameters for each interface (name, priority, etc.).
        esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
        esp_netif_config_t cfg_spi = {
            .base = &esp_netif_config,
            .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
        };
        char if_key_str[10];
        char if_desc_str[10];
        char num_str[3];
        for (int i = 0; i < eth_port_cnt; i++) {
            itoa(i, num_str, 10);
            strcat(strcpy(if_key_str, "ETH_"), num_str);
            strcat(strcpy(if_desc_str, "eth"), num_str);
            esp_netif_config.if_key = if_key_str;
            esp_netif_config.if_desc = if_desc_str;
            esp_netif_config.route_prio -= i*5;
            esp_netif_t *eth_netif = esp_netif_new(&cfg_spi);

            // Attach Ethernet driver to TCP/IP stack
            ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[i])));
        }
    }

    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    // Start Ethernet driver state machine
    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
    }

    vWaitOnETHConnected();
}

bool bIsConnected()
{
    return b_is_eth_connected;
}

void vWaitOnETHConnected(void)
{
    xEventGroupWaitBits( eth_event_group, ETH_CONNECTED_EVENT, false, true, portMAX_DELAY );
    ESP_LOGW(TAG, "ETH Connected from vWaitOnETHConnected");
}

void disconnect_ethernet(void)
{
    ESP_LOGI(TAG, "Disconnecting Ethernet...");
    
    // Set connection status to false
    b_is_eth_connected = false;

    // Unregister Ethernet event handlers
    ESP_ERROR_CHECK(esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler));

    // Stop Ethernet driver(s)
    for (int i = 0; i < eth_port_cnt; i++) {
        ESP_LOGI(TAG, "Sopping Ethernet!");
        ESP_ERROR_CHECK(esp_eth_stop(eth_handles[i]));
    }

    ESP_LOGI(TAG, "Ethernet Disconnected.");
}

void SwitchWifi(bool UseWifi)
{
    //ESP_LOGI(TAG, "Switching into Wifi-Mode!");
    //bUseWifi = UseWifi;
    //disconnect_ethernet();

    //app_wifi_init();
    //app_wifi_start( POP_TYPE_MAC );
}

char* LanPrintMac()
{
    // Statischer Speicher für die MAC-Adresse im Format "xx:xx:xx:xx:xx:xx"
    static char MacAdress[18];  // 17 Zeichen für die MAC-Adresse + 1 Zeichen für die Nullterminierung

    // Erstelle den MAC-Adress-String
    snprintf(MacAdress, sizeof(MacAdress), "%02X%02X%02X%02X%02X%02X", 
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

    return MacAdress;
}
