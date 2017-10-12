#include "uart.h"
#include "nrf.h"
#include "app_timer.h"

static uint8_t buffer_tx[0xFF];
static uint8_t buffer_rx[0xFF];
static app_timer_t timer;
static app_timer_id_t timer_id = &timer;

void default_handler(uart_state_t state, uint8_t *data, int len);
static uart_cb_t handler = default_handler;

void UARTE0_UART0_IRQHandler()
{
  if(NRF_UARTE0->EVENTS_ERROR){
    NRF_UARTE0->EVENTS_ERROR = 0;
    (void)NRF_UARTE0->ERRORSRC;
  }
  if(NRF_UARTE0->EVENTS_TXDRDY){
    NRF_UARTE0->RXD.PTR = (uint32_t)buffer_rx;
    NRF_UARTE0->RXD.MAXCNT = sizeof(buffer_rx);
    NRF_UARTE0->EVENTS_TXDRDY = 0;
    NRF_UARTE0->TASKS_STARTRX = 1;
  }
  if(NRF_UARTE0->EVENTS_RXTO){
    handler(UART_RECV_MSG,buffer_rx,NRF_UARTE0->RXD.AMOUNT);
    NRF_UARTE0->RXD.PTR = (uint32_t)buffer_rx;
    NRF_UARTE0->RXD.MAXCNT = sizeof(buffer_rx);
    NRF_UARTE0->EVENTS_RXTO = 0;
    NRF_UARTE0->TASKS_STARTRX = 1;
  }
  if(NRF_UARTE0->EVENTS_ENDTX){
    NRF_UARTE0->EVENTS_ENDTX = 0;
    handler(UART_RECV_MSG,buffer_rx,NRF_UARTE0->RXD.AMOUNT);
    NRF_UARTE0->RXD.PTR = (uint32_t)buffer_rx;
    NRF_UARTE0->RXD.MAXCNT = sizeof(buffer_rx);
    NRF_UARTE0->EVENTS_RXDRDY = 0;
    NRF_UARTE0->TASKS_STARTRX = 1;
  }
  if(NRF_UARTE0->EVENTS_ENDRX){
    NRF_UARTE0->EVENTS_ENDRX = 0;
    handler(UART_RECV_MSG,buffer_rx,NRF_UARTE0->RXD.AMOUNT);
    NRF_UARTE0->RXD.PTR = (uint32_t)buffer_rx;
    NRF_UARTE0->RXD.MAXCNT = sizeof(buffer_rx);
    NRF_UARTE0->EVENTS_RXDRDY = 0;
    NRF_UARTE0->TASKS_STARTRX = 1;
  }
  NRF_UARTE0->EVENTS_RXDRDY = 0;
  NRF_UARTE0->EVENTS_TXDRDY = 0;
  NRF_UARTE0->EVENTS_TXSTOPPED = 0;
}
//typedef struct {
//  __IO uint32_t  PTR;                               /*!< Data pointer                                                          */
//  __IO uint32_t  MAXCNT;                            /*!< Maximum number of bytes in receive buffer                             */
//  __I  uint32_t  AMOUNT;                            /*!< Number of bytes transferred in the last transaction                   */
//} UARTE_RXD_Type;
//
//typedef struct {
//  __IO uint32_t  PTR;                               /*!< Data pointer                                                          */
//  __IO uint32_t  MAXCNT;                            /*!< Maximum number of bytes in transmit buffer                            */
//  __I  uint32_t  AMOUNT;                            /*!< Number of bytes transferred in the last transaction                   */
//} UARTE_TXD_Type;

void uart_send(uint8_t *data, int len)
{
  //NRF_UARTE0->TASKS_STARTRX = 1;
  NRF_UARTE0->TASKS_STOPRX = 1;
  memcpy(buffer_tx,data,len);
  NRF_UARTE0->TXD.PTR = (uint32_t)buffer_tx;
  NRF_UARTE0->TXD.MAXCNT = len;
  NRF_UARTE0->TASKS_STARTTX = 1;
}

void uart_cb(uart_cb_t cb)
{
  if(cb)
    handler = cb;
}

void uart_init()
{
  
  NRF_UARTE0->BAUDRATE = UART_BAUDRATE_BAUDRATE_Baud9600; // 115200
  NRF_UARTE0->CONFIG  = 0x00; // No parity No HW flow control
  NRF_UARTE0->PSEL.RXD = 3; // 0x02
  NRF_UARTE0->PSEL.TXD = 2; // 0x03
  //NRF_UARTE0->INTENSET = (1<<4) | (1<<8) | (1<<9) | (1<<17);
  NRF_UARTE0->INTENSET = UARTE_INTENSET_ENDTX_Msk | UARTE_INTENSET_ENDRX_Msk | 
                         UARTE_INTENSET_RXTO_Msk | UARTE_INTENSET_ERROR_Msk |
                         UARTE_INTENSET_RXDRDY_Msk | UARTE_INTENSET_TXDRDY_Msk;
  NRF_UARTE0->ENABLE = 8; // Enable UART
  NRF_UARTE0->TASKS_FLUSHRX = 1;
  NVIC_EnableIRQ(UARTE0_UART0_IRQn);
}

void uart_deinit()
{
}

void default_handler(uart_state_t state, uint8_t *data, int len){};