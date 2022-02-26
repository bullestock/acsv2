#include "defines.h"
#include "util.h"

#include <cmath>
#include <stdio.h>
#include <string.h>
#include <string>
#include <utility>

#include <esp_system.h>
#include <esp_log.h>
#include <esp_console.h>
#include <esp_vfs_dev.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/spi_master.h>
#include <driver/uart.h>
#include <linenoise/linenoise.h>
#include <argtable3/argtable3.h>
#include <nvs.h>
#include <nvs_flash.h>

#include "lcd_api.h"
#include "fontx.h"

extern 	TFT_t dev;
extern 	spi_device_handle_t xpt_handle;

#define MAX_LEN 3
#define	XPT_START	0x80
#define XPT_XPOS	0x50
#define XPT_YPOS	0x10
#define XPT_8BIT  0x80
#define XPT_SER		0x04
#define XPT_DEF		0x03

bool get_int(const std::string& line, int& index, int& value)
{
    while (index < line.size() && line[index] == ' ')
        ++index;
    if (!isdigit(line[index]))
    {
        printf("Expected number, got %d at %d\n", int(line[index]), index);
        return false;
    }
    value = 0;
    while (index < line.size() && isdigit(line[index]))
    {
        value = value*10 + line[index] - '0';
        ++index;
    }
    return true;
}

static void version()
{
    printf("ACS display v " VERSION "\n");
}

void initialize_console()
{
    setvbuf(stdin, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_uart_port_set_rx_line_endings(0, ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_port_set_tx_line_endings(0, ESP_LINE_ENDINGS_CRLF);

    /* Configure UART. Note that REF_TICK is used so that the baud rate remains
     * correct while APB frequency is changing in light sleep mode.
     */
    uart_config_t uart_config;
    memset(&uart_config, 0, sizeof(uart_config));
    uart_config.baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.source_clk = UART_SCLK_REF_TICK;
    ESP_ERROR_CHECK(uart_param_config((uart_port_t) CONFIG_ESP_CONSOLE_UART_NUM, &uart_config));

    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK(uart_driver_install((uart_port_t) CONFIG_ESP_CONSOLE_UART_NUM,
                                         256, 0, 0, NULL, 0));

    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

    /* Initialize the console */
    esp_console_config_t console_config;
    memset(&console_config, 0, sizeof(console_config));
    console_config.max_cmdline_args = 8;
    console_config.max_cmdline_length = 256;
    ESP_ERROR_CHECK(esp_console_init(&console_config));

    linenoiseSetMultiLine(1);

    linenoiseHistorySetMaxLen(100);
    linenoiseSetDumbMode(1);
}

int xptGetit(spi_device_handle_t xpt_handle, int cmd)
{
	char rbuf[MAX_LEN];
	char wbuf[MAX_LEN];

	memset(wbuf, 0, sizeof(rbuf));
	memset(rbuf, 0, sizeof(rbuf));
	wbuf[0] = cmd;
	spi_transaction_t SPITransaction;

	memset(&SPITransaction, 0, sizeof(spi_transaction_t));
	SPITransaction.length = MAX_LEN * 8;
	SPITransaction.tx_buffer = wbuf;
	SPITransaction.rx_buffer = rbuf;
	esp_err_t ret = spi_device_transmit(xpt_handle, &SPITransaction);
	assert(ret == ESP_OK); 
	// 12bit Conversion
	int pos = (rbuf[1]<<4)+(rbuf[2]>>4);
	return(pos);
}

void clear()
{
    lcdFillScreen(&dev, BLACK);
}

void touch()
{
    int level = gpio_get_level(XPT_IRQ);
    if (level == 0)
    {
        int xp = xptGetit(xpt_handle, (XPT_START | XPT_XPOS | XPT_SER));
        int yp = xptGetit(xpt_handle, (XPT_START | XPT_YPOS | XPT_SER));
        printf("T%d %d\n", xp, yp);
    }
    else
        printf("T\n");
}

void handle_line(const std::string& line)
{
    if (line.empty())
        return;
    auto ch = line[0];
    auto rest = line.substr(1);
    bool ok = true;
    switch (ch)
    {
    case 'c':
        clear();
        break;
    case 't':
        touch();
        break;
    case 'v':
    case 'V':
        version();
        break;
    default:
        printf("ERROR: Unknown command: %s\n", line.c_str());
        break;
    }
    if (ok)
        printf("OK\n");
    else
        printf("ERROR\n");
}

extern "C" void console_task(void*)
{
    initialize_console();
    std::string line;
    bool idle = true;
    int count = 0;
    while (1)
    {
        vTaskDelay(10 / portTICK_RATE_MS);
        if (idle)
        {
            if (++count > 15)
            {
                count = 0;
                update_spinner(dev);
            }
        }
        //size_t bytes = 0;
        //const auto res = uart_get_buffered_data_len(0, &bytes);
        //int ch = fgetc(stdin);
        //if (ch != EOF)
        char ch;
        const auto bytes = uart_read_bytes(0, &ch, 1, 1);
        if (bytes > 0)
        {
            if (idle)
                clear();
            idle = false;
            if (ch == '\r' || ch == '\n')
            {
                handle_line(line);
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
    }
}
