#include "uart.h"
#include "nrf.h"
#include "app_timer.h"
#include "string.h"

static uint8_t buffer_tx_temp[2048];
static uint8_t *buffer_tx;
static uint8_t buffer_rx[2048];
static uint16_t buffer_tx_cnt = 0;
static uint16_t buffer_rx_cnt = 0;
static uint16_t buffer_tx_len = 0;

static app_timer_t timer;
static app_timer_id_t timer_id = &timer;

static void default_handler(uart_state_t state, uint8_t *data, int len);
static uart_cb_t callback = default_handler;

void UARTE0_UART0_IRQHandler()
{
  if(NRF_UART0->EVENTS_ERROR){
    NRF_UART0->EVENTS_ERROR = 0;
    NRF_UART0->ERRORSRC = 0;
  }
  if(NRF_UART0->EVENTS_RXDRDY){
    NRF_UART0->EVENTS_RXDRDY = 0;
    buffer_rx[buffer_rx_cnt++] = NRF_UART0->RXD;
    app_timer_stop(timer_id);
    app_timer_start(timer_id,APP_TIMER_TICKS(3),NULL);
  }
  if(NRF_UART0->EVENTS_TXDRDY){
    NRF_UART0->EVENTS_TXDRDY = 0;
    if(buffer_tx_cnt < buffer_tx_len)
    {
      NRF_UART0->TXD = buffer_tx[buffer_tx_cnt++];
    }
    else
    {
      NRF_UART0->TASKS_STOPTX = 1;
      callback(UART_SEND_MSG_COMPLETE,buffer_tx,buffer_tx_cnt);
    }
  }
}

void uart_send_buffer(uint8_t *data, int len)
{
  if(!len)
    return;
  NRF_UART0->TASKS_STOPTX = 1;
  buffer_tx=data;
  buffer_tx_len = len;
  buffer_tx_cnt = 0;
  NRF_UART0->TXD = buffer_tx[buffer_tx_cnt++];
  NRF_UART0->TASKS_STARTTX = 1;
}
void uart_send(uint8_t *data, int len)
{
  if(!len)
    return;
  NRF_UART0->TASKS_STOPTX = 1;
  memcpy(buffer_tx_temp,data,len);
  buffer_tx_temp[len]=0;
  buffer_tx = buffer_tx_temp;
  buffer_tx_len = len;
  buffer_tx_cnt = 0;
  NRF_UART0->TXD = buffer_tx[buffer_tx_cnt++];
  NRF_UART0->TASKS_STARTTX = 1;
}

void uart_cb(uart_cb_t cb)
{
  if(cb)
    callback = cb;
}

static void uart_handler(void * p_context)
{
  callback(UART_RECV_MSG,buffer_rx,buffer_rx_cnt);
  memset(buffer_rx,0,sizeof(buffer_rx));
  buffer_rx_cnt = 0;
}

void uart_init()
{
  
  NRF_UART0->BAUDRATE = UART_BAUDRATE_BAUDRATE_Baud9600; // 115200
  NRF_UART0->CONFIG  = 0x00; // No parity No HW flow control
  NRF_UART0->PSELRXD = 3; // 0x02
  NRF_UART0->PSELTXD = 2; // 0x03
  NRF_UART0->INTENCLR = 0xFFFFFFFF;
  NRF_UART0->INTENSET = UART_INTENSET_TXDRDY_Msk | UART_INTENSET_RXDRDY_Msk | UART_INTENSET_ERROR_Msk;
  NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Enabled << UART_ENABLE_ENABLE_Pos; // Enable UART
  NRF_UART0->TASKS_STARTRX = 1;
  
  NVIC_ClearPendingIRQ(UARTE0_UART0_IRQn);
  NVIC_SetPriority(UARTE0_UART0_IRQn, 1);
  NVIC_EnableIRQ(UARTE0_UART0_IRQn);
  
  if(NRF_SUCCESS != app_timer_create(&timer_id,APP_TIMER_MODE_SINGLE_SHOT,uart_handler))
  {
    while(1);
  }
  
}

void uart_deinit()
{
  NRF_UART0->BAUDRATE = UART_BAUDRATE_BAUDRATE_Baud9600; // 115200
  NRF_UART0->CONFIG   = 0x00; // No parity No HW flow control
  NRF_UART0->PSELRXD = 0xFFFFFFFF; // 0x02
  NRF_UART0->PSELTXD = 0xFFFFFFFF; // 0x03
  //NRF_UARTE0->INTENSET = (1<<4) | (1<<8) | (1<<9) | (1<<17);
  NRF_UART0->INTENCLR = 0xFFFFFFFF;
  NRF_UART0->ENABLE   = 0x00000000; // Enable UART
  
  NRF_UART0->EVENTS_RXDRDY = 0;
  NRF_UART0->EVENTS_TXDRDY = 0;
  NRF_UART0->EVENTS_CTS   = 0;
  NRF_UART0->EVENTS_ERROR = 0;
  NRF_UART0->EVENTS_NCTS  = 0;
  NRF_UART0->EVENTS_RXTO  = 0;
  
  NVIC_DisableIRQ(UARTE0_UART0_IRQn);
}

void default_handler(uart_state_t state, uint8_t *data, int len){};