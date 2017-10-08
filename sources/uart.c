#include "uart.h"
#include "nrf.h"
void UARTE0_UART0_IRQHandler()
{
  if(NRF_UART0->EVENTS_TXDRDY){
    
  }
}


void uart_init()
{
  NRF_UART0->BAUDRATE = 0x01D7E000; // 115200
  NRF_UART0->CONFIG  = 0x00; // No parity No HW flow control
  NRF_UART0->PSELRXD = 2; // 0x02
  NRF_UART0->PSELTXD = 3; // 0x03
  NRF_UART0->INTENSET = (1<<4) | (1<<8) | (1<<9) | (1<<17);
  NRF_UART0->ENABLE = 4; // Enable UART
}

void uart_deinit()
{
}