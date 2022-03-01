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
static const int line_height_large = 30;
static const int max_x_small = 39;
static const int max_y_small = 11;
static const int max_x_large = 19;
static const int max_y_large = 8;

// x,y,col,text
bool text(bool large, bool erase, const std::string s)
{
   std::vector<std::string> elems;
   split(s, ",", elems);
   if (elems.size() < 4)
   {
       printf("ERROR: Missing parameters\n");
       return false;
   }
   auto text = elems[3];
   for (size_t i = 4; i < elems.size(); ++i)
   {
       text += ",";
       text += elems[i];
   }
    const int max_x = large ? max_x_large : max_x_small;
    const int max_y = large ? max_y_large : max_y_small;
    int x, y, col;
    if (elems[0].empty())
        // No X specified, center
        x = (CONFIG_HEIGHT - text.size())/2;
    else if (!from_string(elems[0], x) || x < 0 || x > max_x)
    {
        printf("ERROR: Invalid X\n");
        return false;
    }
    if (!from_string(elems[1], y) || y < 0 || y > max_y)
    {
        printf("ERROR: Invalid Y\n");
        return false;
    }
    if (!from_string(elems[2], col) ||
        col < 0 || col >= sizeof(colours)/sizeof(colours[0]))
    {
        printf("ERROR: Invalid colour\n");
        return false;
    }
    const int line_height = large ? line_height_large : line_height_small;
    const int pix_y = CONFIG_WIDTH - y * line_height;
    const int erase_offset = 0;
    if (erase)
        lcdDrawFillRect(&dev, pix_y + erase_offset, 0, pix_y + erase_offset + line_height - 1, CONFIG_HEIGHT - 1, BLACK);
    printf("At %d, %d in %d: %s\n", y, x, col, text.c_str());
    lcdDrawString(&dev,
                  large ? &fx_large : &fx_small,
                  pix_y,
                  x * (large ? char_width_large : char_width_small),
                  reinterpret_cast<const uint8_t*>(text.c_str()), colours[col]);
    return true;
}

// x1,y1,x2,y2,col
bool rect(const std::string s)
{
   std::vector<std::string> elems;
   split(s, ",", elems);
   if (elems.size() != 5)
   {
       printf("ERROR: Missing parameters\n");
       return false;
   }
   int x1, y1, x2, y2, col;
    if (!from_string(elems[0], x1) || x1 < 0 || x1 > CONFIG_HEIGHT)
    {
        printf("ERROR: Invalid X1\n");
        return false;
    }
    if (!from_string(elems[1], y1) || y1 < 0 || y1 > CONFIG_WIDTH)
    {
        printf("ERROR: Invalid Y1\n");
        return false;
    }
    if (!from_string(elems[2], x2) || x2 < 0 || x2 > CONFIG_HEIGHT)
    {
        printf("ERROR: Invalid X2\n");
        return false;
    }
    if (!from_string(elems[3], y2) || y2 < 0 || y2 > CONFIG_WIDTH)
    {
        printf("ERROR: Invalid Y2\n");
        return false;
    }
    if (!from_string(elems[4], col) || col < 0 || col > sizeof(colours)/sizeof(colours[0]))
    {
        printf("ERROR: Invalid colour\n");
        return false;
    }
    printf("rect %d, %d, %d, %d, %d\n", y1, x1, y2, x2, col);
    lcdDrawRect(&dev, y1, x1, y2, x2, colours[col]);
    return true;
}


static int last_touch_x = 0;
static int last_touch_y = 0;
static TickType_t last_touch_tick = 0;

void check_touch()
{
    int level = gpio_get_level(XPT_IRQ);
    if (level)
        return;
    last_touch_x = xptGetit(xpt_handle, XPT_START | XPT_XPOS | XPT_SER);
    last_touch_y = xptGetit(xpt_handle, XPT_START | XPT_YPOS | XPT_SER);
    last_touch_tick = xTaskGetTickCount();
}

void touch()
{
    if (xTaskGetTickCount() - last_touch_tick > 1000 / portTICK_RATE_MS)
    {
        printf("T\n");
        return;
    }
    printf("T%d %d\n", last_touch_x, last_touch_y);
}

bool check_erase(std::string& s)
{
    if (s.empty() || (s[0] != 'e' && s[0] != 'E'))
        return false;
    s = s.substr(1);
    return true;
}

bool idle = true;

void handle_line(const std::string& line)
{
    if (line.empty())
        return;
    auto ch = line[0];
    auto rest = strip(line.substr(1));
    bool ok = true;
    bool erase = false;
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
    case 'r':
        ok = rect(rest);
        break;
    case 't':
        // t[e][x],y,col,text
        erase = check_erase(rest);
        ok = text(false, erase, rest);
        break;
    case 'T':
        // T[E][x],y,col,text
        erase = check_erase(rest);
        ok = text(true, erase, rest);
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
        check_touch();

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
