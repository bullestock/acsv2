#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "RDM6300.h"
#include "buzzer.h"
#include "console.h"
#include "defines.h"
#include "rs485.h"

#include <mutex>
#include <string>

#define BUF_SIZE (128)
#define PIN_TXD (UART_PIN_NO_CHANGE)
#define PIN_RXD 18
#define PIN_RTS (UART_PIN_NO_CHANGE)
#define PIN_CTS (UART_PIN_NO_CHANGE)

extern "C" void console_task(void*);
extern "C" void led_task(void*);

static std::mutex last_cardid_mutex;
static RDM6300::Card_id last_cardid;

RDM6300::Card_id get_and_clear_last_cardid()
{
    std::lock_guard<std::mutex> g(last_cardid_mutex);
    const auto id = last_cardid;
    last_cardid = 0;
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

    ESP_ERROR_CHECK(uart_driver_install(CONSOLE_UART_PORT, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(CONSOLE_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(CONSOLE_UART_PORT, PIN_TXD, PIN_RXD, PIN_RTS, PIN_CTS));

    // Configure a temporary buffer for the incoming data
    uint8_t data[BUF_SIZE];

    RDM6300 decoder;

    auto last_beep = esp_timer_get_time();
    while (1)
    {
        int len = uart_read_bytes(CONSOLE_UART_PORT, data, BUF_SIZE, 20 / portTICK_PERIOD_MS);
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
                       //printf("last %ld\n", (long) last_cardid);
                  }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

extern "C"
void app_main(void)
{
    init_buzzer();
    init_rs485();
    
    xTaskCreate(rfid_task, "rfid_task", 10*1024, NULL, 5, NULL);
    xTaskCreate(console_task, "console_task", 4*1024, NULL, 5, NULL);
    xTaskCreate(led_task, "led_task", 4*1024, NULL, 5, NULL);

    std::string line;
    while (1)
    {
        char buf[32];
        int bytes = read_rs485(buf, sizeof(buf));
        for (int i = 0; i < bytes; ++i)
        {
            int ch = buf[i];
            if (ch == '\r' || ch == '\n')
            {
                const auto reply = handle_line(line);
                write_rs485(reply.c_str(), reply.size());
                line.clear();
                continue;
            }
            line += ch;
            if (line.size() > 1024)
            {
                printf("ERROR: Line too long\n");
                line.clear();
            }
        }
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}
