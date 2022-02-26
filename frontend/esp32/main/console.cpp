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
extern FontxFile fx_large;
extern FontxFile fx_small;

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

static uint16_t colours[] = {
    RED,    // 0xf800
    GREEN,  // 0x07e0
    BLUE,   // 0x001f
    WHITE,  // 0xffff
    GRAY,   // 0x8c51
    YELLOW, // 0xFFE0
    CYAN, // 0x07FF
    PURPLE, // 0xF81F
};

static const int char_width_small = 8;
static const int line_height_small = 18;
static const int char_width_large = 16;
static const int line_height_large = 25;
static const int max_x_small = 39;
static const int max_y_small = 12;
static const int max_x_large = 19;
static const int max_y_large = 8;

// x,y,col,text
bool text(bool large, const std::string s)
{
    const auto first_sep = s.find(",");
    if (first_sep == std::string::npos)
    {
        printf("Missing Y\n");
        return false;
    }
    const auto second_sep = s.find(",", first_sep + 1);
    if (second_sep == std::string::npos)
    {
        printf("Missing colour\n");
        return false;
    }
    const auto third_sep = s.find(",", second_sep + 1);
    if (third_sep == std::string::npos)
    {
        printf("Missing text\n");
        return false;
    }
    const int max_x = large ? max_x_large : max_x_small;
    const int max_y = large ? max_y_large : max_y_small;
    int x, y, col;
    if (!from_string(s.substr(0, first_sep), x) || x < 0 || x > max_x ||
        !from_string(s.substr(first_sep + 1, second_sep - first_sep), y) || y < 0 || y > max_y)
    {
        printf("Invalid coords\n");
        return false;
    }
    if (!from_string(s.substr(second_sep + 1, third_sep - second_sep), col) ||
        col < 0 || col >= sizeof(colours)/sizeof(colours[0]))
    {
        printf("Invalid colour\n");
        return false;
    }
    const auto text = s.substr(third_sep + 1);
    printf("At %d, %d in %d: %s\n", x, y, col, text.c_str());
    lcdDrawString(&dev,
                  large ? &fx_large : &fx_small,
                  (max_y - y) * (large ? line_height_large : line_height_small),
                  x * (large ? char_width_large : char_width_small),
                  reinterpret_cast<const uint8_t*>(text.c_str()), colours[col]);
    return true;
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

bool idle = true;

void handle_line(const std::string& line)
{
    if (line.empty())
        return;
    auto ch = line[0];
    auto rest = strip(line.substr(1));
    bool ok = true;
    switch (ch)
    {
    case 'c':
        clear();
        break;
    case 'i':
        idle = true;
        break;
    case 'p':
        touch();
        break;
    case 't':
        text(false, rest);
        break;
    case 'T':
        text(true, rest);
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
    printf("\nInit console done\n");
    vTaskDelay(1000 / portTICK_RATE_MS);
    printf("\n\n");
    version();

    std::string line;
    int count = 0;
    while (1)
    {
        vTaskDelay(10 / portTICK_RATE_MS);

        // Update idle animation if idle
        if (idle)
            if (++count > 25)
            {
                count = 0;
                update_spinner(dev);
            }

        char ch;
        const auto bytes = uart_read_bytes(0, &ch, 1, 1);
        if (bytes > 0)
        {
            // No longer idle, remove spinner
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
