#include "console.h"

#include "defs.h"
#include "display.h"
#include "format.h"
#include "gateway.h"
#include "hw.h"
#include "mqtt.h"
#include "nvs.h"
#include "util.h"

#include <memory>
#include <string>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
#  include <driver/uart_vfs.h>
#endif
#include <esp_console.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_vfs_dev.h>
#include <mbedtls/base64.h>
#include <nvs_flash.h>

#include <linenoise/linenoise.h>
#include <argtable3/argtable3.h>

static int test_logger(int argc, char**)
{
    printf("Running logger test\n");

    if (argc > 1)
    {
        int i = 0;
        while (1)
        {
            printf("%d\n", i);
            log_mqtt(format("log stress test #%d", i));
            ++i;
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    log_mqtt("camctl test log");

    return 0;
}

struct
{
    struct arg_str* ssid;
    struct arg_str* password;
    struct arg_end* end;
} add_wifi_credentials_args;

int add_wifi_creds(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &add_wifi_credentials_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, add_wifi_credentials_args.end, argv[0]);
        return 1;
    }
    const auto ssid = add_wifi_credentials_args.ssid->sval[0];
    const auto password = add_wifi_credentials_args.password->sval[0];
    if (strlen(ssid) < 1)
    {
        printf("ERROR: Invalid SSID value\n");
        return 1;
    }
    add_wifi_credentials(ssid, password);
    printf("OK: Added WiFi credentials %s/%s\n", ssid, password);
    return 0;
}

int list_wifi_creds(int argc, char** argv)
{
    const auto creds = get_wifi_creds();
    for (const auto& c : creds)
    {
        printf("%-20s %s\n", c.first.c_str(),
               c.second.empty() ? "" : "***");
    }
    printf("OK: Listed %d WiFi credentials\n", static_cast<int>(creds.size()));
    return 0;
}

int clear_wifi_credentials(int, char**)
{
    clear_wifi_credentials();
    printf("OK: WiFi credentials cleared\n");
    return 0;
}

struct
{
    struct arg_str* gw_token;
    struct arg_end* end;
} set_gw_credentials_args;

int set_gw_credentials(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &set_gw_credentials_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, set_gw_credentials_args.end, argv[0]);
        return 1;
    }
    const auto gw_token = set_gw_credentials_args.gw_token->sval[0];
    if (strlen(gw_token) < 32)
    {
        printf("ERROR: Invalid token\n");
        return 1;
    }
    set_gateway_token(gw_token);
    printf("OK: Gateway token set to %s\n", gw_token);
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

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    uart_vfs_dev_port_set_rx_line_endings(0, ESP_LINE_ENDINGS_CR);
    uart_vfs_dev_port_set_tx_line_endings(0, ESP_LINE_ENDINGS_CRLF);
#else
    esp_vfs_dev_uart_port_set_rx_line_endings(0, ESP_LINE_ENDINGS_CR);
    esp_vfs_dev_uart_port_set_tx_line_endings(0, ESP_LINE_ENDINGS_CRLF);
#endif

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

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    uart_vfs_dev_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
#else
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
#endif   

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

void run_console(Display& display)
{
    initialize_console();

    esp_console_register_help_command();

    const esp_console_cmd_t test_logger_cmd = {
        .command = "logger_test",
        .help = "Test logger",
        .hint = nullptr,
        .func = &test_logger,
        .argtable = nullptr,
        .func_w_context = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&test_logger_cmd));

    add_wifi_credentials_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID");
    add_wifi_credentials_args.password = arg_strn(NULL, NULL, "<password>", 0, 1, "Password");
    add_wifi_credentials_args.end = arg_end(2);
    const esp_console_cmd_t add_wifi_credentials_cmd = {
        .command = "wifi",
        .help = "Add WiFi credentials",
        .hint = nullptr,
        .func = &add_wifi_creds,
        .argtable = &add_wifi_credentials_args,
        .func_w_context = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&add_wifi_credentials_cmd));

    const esp_console_cmd_t list_wifi_credentials_cmd = {
        .command = "list_wifi",
        .help = "List WiFi credentials",
        .hint = nullptr,
        .func = &list_wifi_creds,
        .argtable = nullptr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&list_wifi_credentials_cmd));

    const esp_console_cmd_t clear_wifi_credentials_cmd = {
        .command = "clearwifi",
        .help = "Clear WiFi credentials",
        .hint = nullptr,
        .func = &clear_wifi_credentials,
        .argtable = nullptr
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&clear_wifi_credentials_cmd));

    set_gw_credentials_args.gw_token = arg_str1(NULL, NULL, "<gw_token>", "Gateway token");
    set_gw_credentials_args.end = arg_end(1);
    const esp_console_cmd_t set_gw_credentials_cmd = {
        .command = "gw",
        .help = "Set gateway credentials",
        .hint = nullptr,
        .func = &set_gw_credentials,
        .argtable = &set_gw_credentials_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_gw_credentials_cmd));

    const esp_console_cmd_t reboot_cmd = {
        .command = "reboot",
        .help = "Reboot",
        .hint = nullptr,
        .func = &reboot,
        .argtable = nullptr
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&reboot_cmd));
    
    const char* prompt = LOG_COLOR_I "camctl> " LOG_RESET_COLOR;
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
            printf("Command returned non-zero error code: 0x%x (%s)\n",
                   ret, esp_err_to_name(err));
        else if (err != ESP_OK)
            printf("Internal error: %s\n", esp_err_to_name(err));

        linenoiseFree(line);
    }
}

// Local Variables:
// compile-command: "(cd ..; idf.py build)"
// End:
