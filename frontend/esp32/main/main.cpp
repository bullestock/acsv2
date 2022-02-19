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

#include "lcd_com.h"
#include "lcd_lib.h"
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

#define MAX_LEN 3
#define	XPT_START	0x80
#define XPT_XPOS	0x50
#define XPT_YPOS	0x10
#define XPT_8BIT  0x80
#define XPT_SER		0x04
#define XPT_DEF		0x03


int xptGetit(spi_device_handle_t xpt_handle, int cmd){
	char rbuf[MAX_LEN];
	char wbuf[MAX_LEN];

	memset(wbuf, 0, sizeof(rbuf));
	memset(rbuf, 0, sizeof(rbuf));
	wbuf[0] = cmd;
	spi_transaction_t SPITransaction;
	esp_err_t ret;

	memset(&SPITransaction, 0, sizeof(spi_transaction_t));
	SPITransaction.length = MAX_LEN * 8;
	SPITransaction.tx_buffer = wbuf;
	SPITransaction.rx_buffer = rbuf;
#if 1
	ret = spi_device_transmit(xpt_handle, &SPITransaction);
#else
	ret = spi_device_polling_transmit(xpt_handle, &SPITransaction);
#endif
	assert(ret==ESP_OK); 
	ESP_LOGD(TAG, "rbuf[0]=%02x rbuf[1]=%02x rbuf[2]=%02x", rbuf[0], rbuf[1], rbuf[2]);
	// 12bit Conversion
	//int pos = (rbuf[1]<<8)+rbuf[2];
	int pos = (rbuf[1]<<4)+(rbuf[2]>>4);
	return(pos);
}

void xptGetxy(spi_device_handle_t xpt_handle, int *xp, int *yp){
#if 0
	*xp = xptGetit(xpt_handle, (XPT_START | XPT_XPOS));
	*yp = xptGetit(xpt_handle, (XPT_START | XPT_YPOS));
#endif
#if 1
	*xp = xptGetit(xpt_handle, (XPT_START | XPT_XPOS | XPT_SER));
	*yp = xptGetit(xpt_handle, (XPT_START | XPT_YPOS | XPT_SER));
#endif
}

auto XPT_IRQ = (gpio_num_t) 33;

void TouchPosition(spi_device_handle_t xpt_handle, TickType_t timeout) {
	ESP_LOGW(TAG, "Start TouchPosition");
	TickType_t lastTouched = xTaskGetTickCount();

	// Enable XPT2046
  int _xp;
  int _yp;
  xptGetxy(xpt_handle, &_xp, &_yp);

	bool isRunning = true;
	while(isRunning) {
		int level = gpio_get_level(XPT_IRQ);
		ESP_LOGD(TAG, "gpio_get_level=%d", level);
		if (level == 0) {
			xptGetxy(xpt_handle, &_xp, &_yp);
			ESP_LOGI(TAG, "TouchPosition _xp=%d _yp=%d", _xp, _yp);
			lastTouched = xTaskGetTickCount();
		} else {
			TickType_t current = xTaskGetTickCount();
			if (current - lastTouched > timeout) {
				isRunning = false;
			}
		}
        vTaskDelay(10 / portTICK_PERIOD_MS);
	} // end while
	ESP_LOGW(TAG, "End TouchPosition");
}

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

	TFT_t dev;
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
	ESP_LOGI(TAG, "XPT_IRQ=%d",XPT_IRQ);
	gpio_config_t io_conf = {};
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.pin_bit_mask = (1ULL<<XPT_IRQ);
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
	gpio_config(&io_conf);
	//gpio_reset_pin(XPT_IRQ);
	//gpio_set_direction(XPT_IRQ, GPIO_MODE_DEF_INPUT);

	spi_device_interface_config_t xpt_devcfg = {
		.clock_speed_hz = XPT_Frequency,
		.spics_io_num = XPT_CS,
		.flags = SPI_DEVICE_NO_DUMMY,
		.queue_size = 7,
	};

	spi_device_handle_t xpt_handle;
	ret = spi_bus_add_device(LCD_HOST, &xpt_devcfg, &xpt_handle);
	ESP_LOGD(TAG, "spi_bus_add_device=%d",ret);
	assert(ret == ESP_OK);
#if 0    
    while (1)
    {
        TouchPosition(xpt_handle, 1000);
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
#endif
    xTaskCreate(console_task, "console_task", 4*1024, NULL, 5, NULL);
}
