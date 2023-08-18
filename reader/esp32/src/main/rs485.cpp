// https://github.com/espressif/esp-idf/tree/master/examples/peripherals/uart/uart_echo_rs485

#include "defines.h"

#include "driver/uart.h"

constexpr const int BUF_SIZE = 127;
constexpr const int READ_TIMEOUT = 3;
constexpr const int PACKET_READ_TICS = 100 / portTICK_PERIOD_MS;

void init_rs485()
{
    uart_config_t uart_config = {
        .baud_rate = RS485_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(RS485_UART_PORT, BUF_SIZE * 2,
                                        0, 0, NULL, 0));

    ESP_ERROR_CHECK(uart_param_config(RS485_UART_PORT, &uart_config));

    ESP_ERROR_CHECK(uart_set_pin(RS485_UART_PORT, RS485_TXD, RS485_RXD,
                                 RS485_RTS, UART_PIN_NO_CHANGE));

    ESP_ERROR_CHECK(uart_set_mode(RS485_UART_PORT, UART_MODE_RS485_HALF_DUPLEX));

    ESP_ERROR_CHECK(uart_set_rx_timeout(RS485_UART_PORT, READ_TIMEOUT));
}

int read_rs485(char* buf, size_t buf_size)
{
    return uart_read_bytes(RS485_UART_PORT,
                           reinterpret_cast<uint8_t*>(buf), buf_size,
                           PACKET_READ_TICS);
}

void write_rs485(const char* data, size_t size)
{
    const auto wrote = uart_write_bytes(RS485_UART_PORT, data, size);
    assert(wrote == size);
}
