#include "console.h"

#include "buttons.h"
#include "cardcache.h"
#include "cardreader.h"
#include "defs.h"
#include "display.h"
#include "format.h"
#include "gateway.h"
#include "hw.h"
#include "logger.h"
#include "nvs.h"
#include "slack.h"

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

static int test_gateway(int, char**)
{
    printf("Running gateway test\n");

    auto status = cJSON_CreateObject();
    cJSON_wrapper jw(status);
    auto door = cJSON_CreateString("closed");
    cJSON_AddItemToObject(status, "door", door);
    auto space = cJSON_CreateString("open");
    cJSON_AddItemToObject(status, "space", space);
    auto lock = cJSON_CreateString("unlocked");
    cJSON_AddItemToObject(status, "lock status", lock);
    
    Gateway::instance().set_status(status);

    return 0;
}

static int test_slack(int argc, char**)
{
    printf("Running Slack test\n");

    if (argc > 1)
    {
        int i = 0;
        while (1)
        {
            printf("%d\n", i);
            Slack_writer::instance().send_message(format("Slack stress test #%d", i));
            ++i;
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    Slack_writer::instance().send_message(format("ESP frontend (%s) says hi",
                                                 get_identifier().c_str()));

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

    Logger::instance().set_log_to_gateway(true);

    if (argc > 1)
    {
        int i = 0;
        while (1)
        {
            printf("%d\n", i);
            Logger::instance().log(format("log stress test #%d", i));
            ++i;
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    Logger::instance().log("ESP test log: normal");
    Logger::instance().log_verbose("ESP test log: verbose");
    Logger::instance().log_backend(42, "ESP test log: backend");
    Logger::instance().log_unknown_card(0x12345678);

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
    struct arg_str* descriptor;
    struct arg_end* end;
} set_descriptor_args;

int set_descriptor(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &set_descriptor_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, set_descriptor_args.end, argv[0]);
        return 1;
    }
    const auto descriptor = set_descriptor_args.descriptor->sval[0];
    if (strlen(descriptor) < 2)
    {
        printf("ERROR: Invalid descriptor\n");
        return 1;
    }
    set_descriptor(descriptor);
    printf("OK: Descriptor set to %s\n", descriptor);
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
    struct arg_str* token;
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
    const auto token = set_gw_credentials_args.token->sval[0];
    if (strlen(token) < 32)
    {
        printf("ERROR: Invalid token\n");
        return 1;
    }
    set_gateway_token(token);
    printf("OK: Gateway token set to %s\n", token);
    return 0;
}

struct
{
    struct arg_str* token;
    struct arg_end* end;
} set_slack_credentials_args;

int set_slack_credentials(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &set_slack_credentials_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, set_slack_credentials_args.end, argv[0]);
        return 1;
    }
    const auto token = set_slack_credentials_args.token->sval[0];
    if (strlen(token) < 32)
    {
        printf("ERROR: Invalid token\n");
        return 1;
    }
    set_slack_token(token);
    printf("OK: Slack token set to %s\n", token);
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
    struct arg_str* username;
    struct arg_str* password;
    struct arg_end* end;
} set_foreninglet_credentials_args;

int set_foreninglet_credentials(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &set_foreninglet_credentials_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, set_foreninglet_credentials_args.end, argv[0]);
        return 1;
    }
    const auto username = set_foreninglet_credentials_args.username->sval[0];
    const auto password = set_foreninglet_credentials_args.password->sval[0];
    if (strlen(username) < 1)
    {
        printf("ERROR: Invalid username value\n");
        return 1;
    }
    set_foreninglet_username(username);
    set_foreninglet_password(password);
    printf("OK: ForeningLet credentials set to %s/%s\n", username, password);
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

    const esp_console_cmd_t test_gateway_cmd = {
        .command = "test_gateway",
        .help = "Test gateway",
        .hint = nullptr,
        .func = &test_gateway,
        .argtable = nullptr
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&test_gateway_cmd));

    const esp_console_cmd_t test_slack_cmd = {
        .command = "test_slack",
        .help = "Test Slack",
        .hint = nullptr,
        .func = &test_slack,
        .argtable = nullptr
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&test_slack_cmd));

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

    set_descriptor_args.descriptor = arg_str1(NULL, NULL, "<ident>", "Descriptor");
    set_descriptor_args.end = arg_end(2);
    const esp_console_cmd_t set_descriptor_cmd = {
        .command = "desc",
        .help = "Set descriptor",
        .hint = nullptr,
        .func = &set_descriptor,
        .argtable = &set_descriptor_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_descriptor_cmd));

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

    set_gw_credentials_args.token = arg_str1(NULL, NULL, "<token>", "Gateway token");
    set_gw_credentials_args.end = arg_end(2);
    const esp_console_cmd_t set_gw_credentials_cmd = {
        .command = "gw",
        .help = "Set gateway credentials",
        .hint = nullptr,
        .func = &set_gw_credentials,
        .argtable = &set_gw_credentials_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_gw_credentials_cmd));

    set_slack_credentials_args.token = arg_str1(NULL, NULL, "<token>", "Slack token");
    set_slack_credentials_args.end = arg_end(2);
    const esp_console_cmd_t set_slack_credentials_cmd = {
        .command = "slack",
        .help = "Set Slack credentials",
        .hint = nullptr,
        .func = &set_slack_credentials,
        .argtable = &set_slack_credentials_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_slack_credentials_cmd));

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

    set_foreninglet_credentials_args.username = arg_str1(NULL, NULL, "<username>", "Username");
    set_foreninglet_credentials_args.password = arg_strn(NULL, NULL, "<password>", 0, 1, "Password");
    set_foreninglet_credentials_args.end = arg_end(2);
    const esp_console_cmd_t set_foreninglet_credentials_cmd = {
        .command = "foreninglet",
        .help = "Set ForeningLet credentials",
        .hint = nullptr,
        .func = &set_foreninglet_credentials,
        .argtable = &set_foreninglet_credentials_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_foreninglet_credentials_cmd));

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
