#ifndef __UART_H__
#define __UART_H__

#include <stdint.h>

typedef enum uart_state_e{
  UART_RECV_MSG,
  UART_SEND_MSG_COMPLETE,
} uart_state_t;

typedef void (*uart_cb_t)(uart_state_t state, uint8_t *data, int len);

void uart_init();
void uart_cb(uart_cb_t cb);
void uart_send(uint8_t *data, int len);
void uart_send_buffer(uint8_t *data, int len);
void uart_deinit();


#endif//__UART_H__