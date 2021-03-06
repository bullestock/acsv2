#ifndef   __LCD_COM_H__
#define   __LCD_COM_H__

#include "i2s_lcd_driver.h"

#include "lcd_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TFTLCD_DELAY	0xFFFF
#define TFTLCD_DELAY16	0xFFFF
#define TFTLCD_DELAY8	0x7F

#define INTERFACE_I2S  1
#define INTERFACE_GPIO 2
#define INTERFACE_REG  3

#define GPIO_PORT_NUM  0

void gpio_digital_write(int GPIO_PIN, uint8_t data);
void gpio_lcd_write_data(int dummy1, unsigned char *data, size_t size);
void reg_lcd_write_data(int dummy1, unsigned char *data, size_t size);

void lcd_write_table(TFT_t * dev, const void *table, int16_t size);
void lcd_write_table16(TFT_t * dev, const void *table, int16_t size);
void lcd_write_comm_byte(TFT_t * dev, uint8_t cmd);
void lcd_write_comm_word(TFT_t * dev, uint16_t cmd);
void lcd_write_data_byte(TFT_t * dev, uint8_t data);
void lcd_write_data_word(TFT_t * dev, uint16_t data);
void lcd_write_addr(TFT_t * dev, uint16_t addr1, uint16_t addr2);
void lcd_write_color(TFT_t * dev, uint16_t color, uint16_t size);
void lcd_write_colors(TFT_t * dev, uint16_t * colors, uint16_t size);
void lcd_delay_ms(int delay_time);
void lcd_write_register_word(TFT_t * dev, uint16_t addr, uint16_t data);
void lcd_write_register_byte(TFT_t * dev, uint8_t addr, uint16_t data);
esp_err_t lcd_interface_cfg(TFT_t * dev, int interface);

#ifdef __cplusplus
}
#endif

#endif
