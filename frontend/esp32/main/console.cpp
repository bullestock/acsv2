#include "console.h"
#include "defs.h"
#include "hw.h"

#include <string>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"

#include <driver/uart.h>

#include <TFT_eSPI.h>

static constexpr const int GFXFF = 1;

#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"

static int toggle_relay(int, char**)
{
    for (int n = 0; n < 10; ++n)
    {
        vTaskDelay(500/portTICK_PERIOD_MS);
        set_relay(true);
        vTaskDelay(500/portTICK_PERIOD_MS);
        set_relay(false);
    }
    printf("done\n");
    return 0;
}

static unsigned int rainbow(int value)
{
  uint8_t red = 0; // Red is the top 5 bits of a 16 bit colour value
  uint8_t green = 0;// Green is the middle 6 bits
  uint8_t blue = 0; // Blue is the bottom 5 bits

  uint8_t quadrant = value / 32;

  if (quadrant == 0) {
    blue = 31;
    green = 2 * (value % 32);
    red = 0;
  }
  if (quadrant == 1) {
    blue = 31 - (value % 32);
    green = 63;
    red = 0;
  }
  if (quadrant == 2) {
    blue = 0;
    green = 63;
    red = value % 32;
  }
  if (quadrant == 3) {
    blue = 0;
    green = 63 - 2 * (value % 32);
    red = 31;
  }
  return (red << 11) + (green << 5) + blue;
}

static long map(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static int test_display(int, char**)
{
    printf("Running display test\n");

    TFT_eSPI tft;
    tft.init();
    tft.setRotation(1);
    
    tft.fillScreen(TFT_BLACK);
#if 1
    // Mandelbrot
    tft.startWrite();
    const int MAX_X = 320;
    const int MAX_Y = 480;
    for (int px = 1; px < MAX_X; px++)
    {
        for (int py = 0; py < MAX_Y; py++)
        {
            float x0 = (map(px, 0, MAX_X, -250000/2, -242500/2)) / 100000.0;
            float yy0 = (map(py, 0, MAX_Y, -75000/4, -61000/4)) / 100000.0; 
            float xx = 0.0;
            float yy = 0.0;
            int iteration = 0;
            int max_iteration = 128;
            while (((xx * xx + yy * yy) < 4) &&
                   (iteration < max_iteration))
            {
                float xtemp = xx * xx - yy * yy + x0;
                yy = 2 * xx * yy + yy0;
                xx = xtemp;
                iteration++;
            }
            int color = rainbow((3*iteration+64)%128);
            tft.drawPixel(px, py, color);
        }
    }
#else
    tft.setFreeFont(&FreeSansBold24pt7b);
    tft.drawString("42.1", 0, 0, GFXFF);
    tft.setFreeFont(&FreeSansBold18pt7b);
    tft.drawString("Compressor", 0, 50, GFXFF);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.drawString("Compressor", 0, 100, GFXFF);
#endif
    tft.endWrite();

    return 0;
}

static int reboot(int, char**)
{
    printf("Reboot...\n");
    esp_restart();
    return 0;
}

void initialize_console()
{
    /* Disable buffering on stdin */
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
#if CONFIG_LOG_COLORS
    console_config.hint_color = atoi(LOG_COLOR_CYAN);
#endif
    ESP_ERROR_CHECK(esp_console_init(&console_config));

    /* Configure linenoise line completion library */
    /* Enable multiline editing. If not set, long commands will scroll within
     * single line.
     */
    linenoiseSetMultiLine(1);

    /* Tell linenoise where to get command completions and hints */
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);

    /* Set command history size */
    linenoiseHistorySetMaxLen(100);
}

void run_console()
{
    initialize_console();

    esp_console_register_help_command();

    const esp_console_cmd_t toggle_relay_cmd = {
        .command = "relay",
        .help = "Toggle relay",
        .hint = nullptr,
        .func = &toggle_relay,
        .argtable = nullptr
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&toggle_relay_cmd));

    const esp_console_cmd_t test_display_cmd = {
        .command = "display",
        .help = "Test display",
        .hint = nullptr,
        .func = &test_display,
        .argtable = nullptr
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&test_display_cmd));

    const esp_console_cmd_t reboot_cmd = {
        .command = "reboot",
        .help = "Reboot",
        .hint = nullptr,
        .func = &reboot,
        .argtable = nullptr
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&reboot_cmd));
    
    const char* prompt = LOG_COLOR_I "acs> " LOG_RESET_COLOR;
    int probe_status = linenoiseProbe();
    if (probe_status)
    {
        printf("\n"
               "Your terminal application does not support escape sequences.\n"
               "Line editing and history features are disabled.\n"
               "On Windows, try using Putty instead.\n");
        linenoiseSetDumbMode(1);
#if CONFIG_LOG_COLORS
        /* Since the terminal doesn't support escape sequences,
         * don't use color codes in the prompt.
         */
        prompt = "esp32> ";
#endif //CONFIG_LOG_COLORS
    }

    while (true)
    {
        char* line = linenoise(prompt);
        if (!line)
            continue;

        linenoiseHistoryAdd(line);

        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND)
            printf("Unrecognized command\n");
        else if (err == ESP_ERR_INVALID_ARG)
            ; // command was empty
        else if (err == ESP_OK && ret != ESP_OK)
            printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(err));
        else if (err != ESP_OK)
            printf("Internal error: %s\n", esp_err_to_name(err));

        linenoiseFree(line);
    }
}

// Local Variables:
// compile-command: "(cd ..; idf.py build)"
// End:
