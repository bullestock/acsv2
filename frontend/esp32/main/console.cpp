#include "console.h"

#include "buttons.h"
#include "cardcache.h"
#include "cardreader.h"
#include "defs.h"
#include "display.h"
#include "format.h"
#include "hw.h"
#include "mqtt.h"
#include "nvs.h"

#include <memory>
#include <string>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
#include <driver/uart_vfs.h>
#endif
#include <esp_console.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_vfs_dev.h>
#include <mbedtls/base64.h>
#include <nvs_flash.h>

static constexpr const int GFXFF = 1;
static Display* display = nullptr;

#include <linenoise/linenoise.h>
#include <argtable3/argtable3.h>

#ifdef HW_TEST

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

static int test_card_cache(int, char**)
{
    printf("Running card cache test\n");

    const auto result = Card_cache::instance().has_access(0x13006042CF);
    printf("Access: %d\n", static_cast<int>(result.access));
    printf("User:   %d\n", result.user_id);

    return 0;
}

static int test_display(int, char**)
{
    printf("Running display test\n");

    display->clear();
    display->set_status("Open", TFT_GREEN,
                        "Open: barndoor, woodshop", TFT_RED);

    return 0;
}

static int test_buttons(int, char**)
{
    printf("Running buttons test\n");

    Buttons b;
    for (int n = 0; n < 10; ++n)
    {
        vTaskDelay(500/portTICK_PERIOD_MS);
        const auto keys = b.read();
        printf("R %d W %d G %d L %d D %d\n",
               keys.red, keys.white, keys.green, keys.leave,
               get_door_open());
    }
    printf("done\n");
    
    return 0;
}

#endif

static int test_logger(int argc, char**)
{
    printf("Running logger test\n");

    if (argc > 1)
    {
        int i = 0;
        while (1)
        {
            printf("%d\n", i);
            Mqtt::instance().log(format("log stress test #%d", i));
            ++i;
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    Mqtt::instance().log("ESP test log: normal");
    Mqtt::instance().log_backend(42, "ESP test log: backend");
    Mqtt::instance().log_unknown_card(0x12345678);

    return 0;
}

static int test_reader(int, char**)
{
    printf("Running reader test\n");

    Card_reader::instance().set_sound(Card_reader::Sound::warning);
    const auto id = Card_reader::instance().get_and_clear_card_id();
    printf("Card ID " CARD_ID_FORMAT "\n", id);
    
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
    struct arg_str* identifier;
    struct arg_end* end;
} set_identifier_args;

int set_identifier(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &set_identifier_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, set_identifier_args.end, argv[0]);
        return 1;
    }
    const auto identifier = set_identifier_args.identifier->sval[0];
    if (strlen(identifier) < 2)
    {
        printf("ERROR: Invalid identifier\n");
        return 1;
    }
    set_identifier(identifier);
    printf("OK: Identifier set to %s\n", identifier);
    return 0;
}

struct
{
    struct arg_str* token;
    struct arg_end* end;
} set_acs_credentials_args;

int set_acs_credentials(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &set_acs_credentials_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, set_acs_credentials_args.end, argv[0]);
        return 1;
    }
    const auto token = set_acs_credentials_args.token->sval[0];
    if (strlen(token) < 32)
    {
        printf("ERROR: Invalid token\n");
        return 1;
    }
    set_acs_token(token);
    printf("OK: ACS token set to %s\n", token);
    return 0;
}

struct
{
    struct arg_str* address;
    struct arg_end* end;
} set_mqtt_params_args;

int set_mqtt_params(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &set_mqtt_params_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, set_mqtt_params_args.end, argv[0]);
        return 1;
    }
    const auto address = set_mqtt_params_args.address->sval[0];
    set_mqtt_address(address);
    printf("OK: MQTT address set to %s\n", address);
    return 0;
}

struct
{
    struct arg_str* key;
    struct arg_end* end;
} set_private_key_args;

int hex_string_to_bytes(const char* hex_str, uint8_t* bytes, size_t max_len)
{
    size_t len = strlen(hex_str);
    if (len % 2 != 0)
    {
        printf("ERROR: Hex string must have even number of characters\n");
        return -1;
    }
    
    size_t byte_len = len / 2;
    if (byte_len > max_len)
    {
        printf("ERROR: Hex string too long (max %zu bytes)\n", max_len);
        return -1;
    }
    
    for (size_t i = 0; i < byte_len; ++i)
    {
        int result = sscanf(hex_str + 2 * i, "%2hhx", &bytes[i]);
        if (result != 1)
        {
            printf("ERROR: Invalid hex format\n");
            return -1;
        }
    }
    
    return byte_len;
}

int set_private_key_cmd(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &set_private_key_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, set_private_key_args.end, argv[0]);
        return 1;
    }
    const auto key_str = set_private_key_args.key->sval[0];
    if (strlen(key_str) != 2*SIGNING_KEY_SIZE)
    {
        printf("ERROR: Invalid private key\n");
        return 1;
    }
    
    uint8_t key_bytes[SIGNING_KEY_SIZE];
    int key_len = hex_string_to_bytes(key_str, key_bytes, sizeof(key_bytes));
    if (key_len < 0)
        return 1;
    
    set_private_key(key_bytes);
    printf("OK: Private key set (%d bytes)\n", key_len);
    return 0;
}

struct
{
    struct arg_int* is_main;
    struct arg_end* end;
} set_is_main_args;

int set_is_main_cmd(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &set_is_main_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, set_is_main_args.end, argv[0]);
        return 1;
    }
    int val = set_is_main_args.is_main->ival[0];
    if (val != 0 && val != 1)
    {
        printf("ERROR: Invalid value, must be 0 or 1\n");
        return 1;
    }
    set_is_main(val != 0);
    printf("OK: is_main set to %d\n", val);
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

void run_console(Display& display_arg)
{
    display = &display_arg;

    initialize_console();

    esp_console_register_help_command();

#ifdef HW_TEST
    
    const esp_console_cmd_t toggle_relay_cmd = {
        .command = "relay",
        .help = "Toggle relay",
        .hint = nullptr,
        .func = &toggle_relay,
        .argtable = nullptr
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&toggle_relay_cmd));

    const esp_console_cmd_t test_display_cmd = {
        .command = "test_display",
        .help = "Test display",
        .hint = nullptr,
        .func = &test_display,
        .argtable = nullptr
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&test_display_cmd));

    const esp_console_cmd_t test_card_cache_cmd = {
        .command = "test_card_cache",
        .help = "Test card cache",
        .hint = nullptr,
        .func = &test_card_cache,
        .argtable = nullptr
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&test_card_cache_cmd));

    const esp_console_cmd_t test_mqtt_cmd = {
        .command = "test_mqtt",
        .help = "Test MQTT",
        .hint = nullptr,
        .func = &test_mqtt,
        .argtable = nullptr
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&test_mqtt_cmd));

    const esp_console_cmd_t test_buttons_cmd = {
        .command = "test_buttons",
        .help = "Test buttons",
        .hint = nullptr,
        .func = &test_buttons,
        .argtable = nullptr
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&test_buttons_cmd));

#endif // HW_TEST

    const esp_console_cmd_t test_logger_cmd = {
        .command = "logger_test",
        .help = "Test logger",
        .hint = nullptr,
        .func = &test_logger,
        .argtable = nullptr
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&test_logger_cmd));

    const esp_console_cmd_t test_reader_cmd = {
        .command = "test_reader",
        .help = "Test card reader",
        .hint = nullptr,
        .func = &test_reader,
        .argtable = nullptr
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&test_reader_cmd));

    add_wifi_credentials_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID");
    add_wifi_credentials_args.password = arg_strn(NULL, NULL, "<password>", 0, 1, "Password");
    add_wifi_credentials_args.end = arg_end(2);
    const esp_console_cmd_t add_wifi_credentials_cmd = {
        .command = "wifi",
        .help = "Add WiFi credentials",
        .hint = nullptr,
        .func = &add_wifi_creds,
        .argtable = &add_wifi_credentials_args
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

    set_identifier_args.identifier = arg_str1(NULL, NULL, "<ident>", "Identifier");
    set_identifier_args.end = arg_end(2);
    const esp_console_cmd_t set_identifier_cmd = {
        .command = "ident",
        .help = "Set identifier",
        .hint = nullptr,
        .func = &set_identifier,
        .argtable = &set_identifier_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_identifier_cmd));

    set_acs_credentials_args.token = arg_str1(NULL, NULL, "<token>", "ACS token");
    set_acs_credentials_args.end = arg_end(2);
    const esp_console_cmd_t set_acs_credentials_cmd = {
        .command = "acs",
        .help = "Set ACS credentials",
        .hint = nullptr,
        .func = &set_acs_credentials,
        .argtable = &set_acs_credentials_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_acs_credentials_cmd));

    set_mqtt_params_args.address = arg_str1(NULL, NULL, "<address>", "MQTT address");
    set_mqtt_params_args.end = arg_end(2);
    const esp_console_cmd_t set_mqtt_params_cmd = {
        .command = "mqtt",
        .help = "Set MQTT host",
        .hint = nullptr,
        .func = &set_mqtt_params,
        .argtable = &set_mqtt_params_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_mqtt_params_cmd));

    set_private_key_args.key = arg_str1(NULL, NULL, "<key>", "Private key");
    set_private_key_args.end = arg_end(2);
    const esp_console_cmd_t set_private_key_cmd_reg = {
        .command = "set_private_key",
        .help = "Set private key",
        .hint = nullptr,
        .func = &set_private_key_cmd,
        .argtable = &set_private_key_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_private_key_cmd_reg));

    set_is_main_args.is_main = arg_int1(NULL, NULL, "<is_main>", "Is main (0/1)");
    set_is_main_args.end = arg_end(2);
    const esp_console_cmd_t set_is_main_cmd_reg = {
        .command = "set_is_main",
        .help = "Set is_main flag",
        .hint = nullptr,
        .func = &set_is_main_cmd,
        .argtable = &set_is_main_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_is_main_cmd_reg));

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
