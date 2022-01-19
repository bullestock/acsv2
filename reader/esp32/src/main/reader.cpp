#include <stdio.h>
#include <string.h>

#include "connect.h"
#include "defines.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "RDM6300.h"
#include "buzzer.h"

#include <mutex>
#include <string>

#define BUF_SIZE (128)
#define UART_PORT_NUM   1
#define PIN_TXD (UART_PIN_NO_CHANGE)
#define PIN_RXD 18
#define PIN_RTS (UART_PIN_NO_CHANGE)
#define PIN_CTS (UART_PIN_NO_CHANGE)

extern "C" void console_task(void*);
extern "C" void led_task(void*);

static std::mutex last_cardid_mutex;
static std::string last_cardid;

std::string get_and_clear_last_cardid()
{
    std::lock_guard<std::mutex> g(last_cardid_mutex);
    const auto id = last_cardid;
    last_cardid.clear();
    return id;
}

void rfid_task(void*)
{
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_APB,
    };
    int intr_alloc_flags = 0;
#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, PIN_TXD, PIN_RXD, PIN_RTS, PIN_CTS));

    // Configure a temporary buffer for the incoming data
    uint8_t data[BUF_SIZE];

    RDM6300 decoder;

    auto last_beep = esp_timer_get_time();
    while (1)
    {
        int len = uart_read_bytes(UART_PORT_NUM, data, BUF_SIZE, 20 / portTICK_RATE_MS);
        if (len)
        {
             for (int i = 0; i < len; ++i)
                  if (decoder.add_byte(data[i]))
                  {
                       std::lock_guard<std::mutex> g(last_cardid_mutex);
                       last_cardid = decoder.get_id();
                       const auto since_last_beep = esp_timer_get_time() - last_beep;
                       if (since_last_beep > 1000000)
                       {
                           beep(1000, 50);
                           last_beep = esp_timer_get_time();
                       }
                       //printf("%s\n", last_cardid.c_str());
                  }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

std::vector<std::pair<std::string, std::string>> get_wifi_credentials(char* buf)
{
    std::vector<std::pair<std::string, std::string>> v;
    bool is_ssid = true;
    std::string ssid;
    char* p = buf;
    while (1)
    {
        char* token = strsep(&p, ":");
        if (!token)
            break;
        if (is_ssid)
            ssid = std::string(token);
        else
            v.push_back(std::make_pair(ssid, std::string(token)));
        is_ssid = !is_ssid;
    }
    return v;
}

int8_t wifi_active = false;

extern "C"
void app_main(void)
{
    init_buzzer();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    nvs_get_i8(my_handle, WIFI_ON_KEY, &wifi_active);
    printf("WiFi active: %d\n", wifi_active);
    if (wifi_active)
    {
        char buf[256];
        auto buf_size = sizeof(buf);
        if (nvs_get_str(my_handle, WIFI_KEY, buf, &buf_size) == ESP_OK)
        {
            const auto creds = get_wifi_credentials(buf);
            connect(creds);
        }
    }
    nvs_close(my_handle);

    xTaskCreate(rfid_task, "rfid_task", 10*1024, NULL, 5, NULL);
    xTaskCreate(console_task, "console_task", 4*1024, NULL, 5, NULL);
    xTaskCreate(led_task, "led_task", 4*1024, NULL, 5, NULL);
}
