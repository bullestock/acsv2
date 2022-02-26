#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"

#include <driver/spi_master.h>
#include <driver/gpio.h>

#include "defines.h"
#include "lcd_com.h"
#include "fontx.h"

#define LCD_HOST HSPI_HOST
static const int XPT_Frequency = 1*1000*1000;

#if CONFIG_INTERFACE_I2S
#define INTERFACE INTERFACE_I2S
#elif CONFIG_INTERFACE_GPIO
#define INTERFACE INTERFACE_GPIO
#elif CONFIG_INTERFACE_REG
#define INTERFACE INTERFACE_REG
#endif

#include "ili9341.h"
#define DRIVER "ILI9341"
#define INIT_FUNCTION(a, b, c, d, e) ili9341_lcdInit(a, b, c, d, e)

extern "C" void console_task(void*);

static const char *TAG = "MAIN";

FontxFile fx16G[2];

void disp_line(TFT_t& dev, uint8_t* text)
{
    static int ypos = -1;
    if (ypos < 0)
    {
        lcdFillScreen(&dev, BLACK);
        ypos = CONFIG_WIDTH - 16;
    }
    uint16_t color = WHITE;
    lcdDrawString(&dev, fx16G, ypos, 0, text, color);
    ypos -= 14;
}

void TouchPosition(spi_device_handle_t xpt_handle, TickType_t timeout) {
	ESP_LOGW(TAG, "Start TouchPosition");
	ESP_LOGW(TAG, "End TouchPosition");
}

TFT_t dev;
spi_device_handle_t xpt_handle;

extern "C"
void app_main(void)
{
	ESP_LOGI(TAG, "Initializing SPIFFS");
	esp_vfs_spiffs_conf_t conf = {
		.base_path = "/spiffs",
		.partition_label = NULL,
		.max_files = 10,
		.format_if_mount_failed =true
	};

	// Use settings defined above toinitialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is an all-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)",esp_err_to_name(ret));
		}
		return;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(NULL, &total,&used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG,"Failed to get SPIFFS partition information (%s)",esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG,"Partition size: total: %d, used: %d", total, used);
	}

	InitFontx(fx16G,"/spiffs/ILGH16XB.FNT",""); // 8x16Dot Gothic

	lcd_interface_cfg(&dev, INTERFACE);

	INIT_FUNCTION(&dev, CONFIG_WIDTH, CONFIG_HEIGHT, CONFIG_OFFSETX, CONFIG_OFFSETY);

    lcdSetFontDirection(&dev, 1);
    int n = 0;
    while (n < 10)
    {
        uint8_t ascii[40];
        sprintf((char *)ascii, "16Dot Gothic Font %d", n++);
        disp_line(dev, ascii);
        vTaskDelay(300/portTICK_PERIOD_MS);
        ++n;
    }

	int MISO_GPIO = 25;

    spi_bus_config_t buscfg = {
		.mosi_io_num = 32,
		.miso_io_num = MISO_GPIO,
		.sclk_io_num = 26,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1
	};

	ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
	ESP_LOGD(TAG, "spi_bus_initialize=%d",ret);
	assert(ret==ESP_OK);

    auto XPT_CS = (gpio_num_t) 27;
	gpio_reset_pin(XPT_CS);
	gpio_set_direction(XPT_CS, GPIO_MODE_OUTPUT);
	gpio_set_level(XPT_CS, 1);

	// set the IRQ as a input
	gpio_config_t io_conf = {};
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.pin_bit_mask = (1ULL<<XPT_IRQ);
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
	gpio_config(&io_conf);

	spi_device_interface_config_t xpt_devcfg = {
		.clock_speed_hz = XPT_Frequency,
		.spics_io_num = XPT_CS,
		.flags = SPI_DEVICE_NO_DUMMY,
		.queue_size = 7,
	};

	ret = spi_bus_add_device(LCD_HOST, &xpt_devcfg, &xpt_handle);
	ESP_LOGD(TAG, "spi_bus_add_device=%d",ret);
	assert(ret == ESP_OK);
#if 0
    TouchPosition(xpt_handle, 5000);
#endif
    xTaskCreate(console_task, "console_task", 4*1024, NULL, 5, NULL);
}
