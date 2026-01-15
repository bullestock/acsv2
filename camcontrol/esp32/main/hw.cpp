#include "hw.h"
#include "defs.h"

#include <freertos/FreeRTOS.h>
#include <driver/i2c_master.h>
#include <driver/gpio.h>

const int I2C_MASTER_FREQ_HZ = 400000; // I2C clock of SSD1306 can run at 400 kHz max.

extern i2c_master_dev_handle_t dev_handle;

void init_hardware()
{
    // Relay outputs
    
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_RELAY1) |
        (1ULL << PIN_RELAY2);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // Button input
    
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_BUTTON) | (1ULL << PIN_OTA);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // I2C
    
    i2c_master_bus_config_t i2c_mst_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
    };
    i2c_mst_config.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t i2c_bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));

    i2c_device_config_t display_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DISPLAY_I2C_ADDRESS,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &display_cfg, &dev_handle));
}

void set_relay1(bool on)
{
    gpio_set_level(PIN_RELAY1, on);
}

void set_relay2(bool on)
{
    gpio_set_level(PIN_RELAY2, on);
}

bool read_button()
{
    return !gpio_get_level(PIN_BUTTON);
}
