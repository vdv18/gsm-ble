#include "modem.h"
#include "app_timer.h"
#include "uart.h"

#define POWER_KEY_ON()  {NRF_P0->OUTSET = 1<<5;}
#define POWER_KEY_OFF() {NRF_P0->OUTCLR = 1<<5;}
#define GSM_ON()        {NRF_P0->OUTCLR = 1<<6;}
#define GSM_OFF()       {NRF_P0->OUTSET = 1<<6;}
#define GSM_STATUS()    ((NRF_P0->IN & 1<<4) > 0)

static void default_handler(modem_state_t state);
static modem_handler_t handler = default_handler;
static app_timer_t timer;
static app_timer_id_t timer_id = &timer;

enum modem_state_int_e {
  STATE_PREPARE           =0xFE,
  STATE_INIT              =0x00,
  STATE_PWR_KEY_ON        =0x01,
  STATE_PWR_KEY_OFF       =0x02,
  STATE_CHECK_STATUS      =0x03,
  STATE_CHECK_AT_RSP      =0x04,
  STATE_WORK              =0x05,
  STATE_NO_WORK           =0xFF,
};
static enum modem_state_int_e modem_state = STATE_INIT;

enum modem_request_state_e {
  REQUEST_IDLE            =0x00,
  REQUEST_SEND            =0x01,
  REQUEST_RESPONSE        =0x02,
};
static enum modem_request_state_e modem_request_state = REQUEST_IDLE;

struct modem_request_s {
  uint8_t *message;
  uint16_t*message_len;
};


void uart_cb_init(uart_state_t state, uint8_t *data, int len);
void uart_cb_work(uart_state_t state, uint8_t *data, int len);


void uart_cb_init(uart_state_t state, uint8_t *data, int len)
{
  switch(state)
  {
    case UART_RECV_MSG:
      if(strstr(data,"OK")!=0)
      {
        modem_state = STATE_WORK;
        app_timer_stop(timer_id);
        app_timer_start(timer_id,APP_TIMER_TICKS(100),NULL);
        uart_cb(uart_cb_work);
      }
      break;
    case UART_SEND_MSG_COMPLETE:
      break;
  }
}
void uart_cb_work(uart_state_t state, uint8_t *data, int len)
{
  switch(state)
  {
    case UART_RECV_MSG:
      if(strstr(data,"OK")!=0)
      {
        memset(data,0,strlen(data));
        modem_request_state = REQUEST_RESPONSE;
      }
      break;
    case UART_SEND_MSG_COMPLETE:
      break;
  }
}
static void modem_handler(void * p_context)
{
  switch(modem_state)
  {
    case STATE_CHECK_AT_RSP:
    case STATE_WORK:
      if(0)if(GSM_STATUS() == 0)
      {
        modem_state = STATE_INIT;
        GSM_OFF();
        POWER_KEY_OFF();
        app_timer_start(timer_id,APP_TIMER_TICKS(1000),NULL);
        return;
      }
      break;
  }
  switch(modem_state)
  {
    case STATE_PREPARE:
      modem_state = STATE_INIT;
      GSM_OFF();
      POWER_KEY_OFF();
      app_timer_start(timer_id,APP_TIMER_TICKS(1000),NULL);
      break;
    case STATE_INIT:app_timer_start(timer_id,APP_TIMER_TICKS(1000),NULL);
      modem_state=STATE_PWR_KEY_ON;
      GSM_ON();
      break;
    case STATE_PWR_KEY_ON:app_timer_start(timer_id,APP_TIMER_TICKS(2000),NULL);
      modem_state=STATE_PWR_KEY_OFF;
      GSM_ON();
      POWER_KEY_ON();
      break;
    case STATE_PWR_KEY_OFF:app_timer_start(timer_id,APP_TIMER_TICKS(2500),NULL);
      modem_state=STATE_CHECK_STATUS;
      POWER_KEY_OFF();
      break;
    case STATE_CHECK_STATUS:
      static int timeout_gsm = 50;
      
      if(timeout_gsm) timeout_gsm--;
      else
      {
        timeout_gsm = 50;
        handler(MODEM_DISABLED);
        modem_state = STATE_NO_WORK;
        app_timer_start(timer_id,APP_TIMER_TICKS(100),NULL);
        break;
      }
      if(GSM_STATUS() > 0)
      {
        modem_state = STATE_CHECK_AT_RSP;
      }
      app_timer_start(timer_id,APP_TIMER_TICKS(100),NULL);
      break;
    case STATE_CHECK_AT_RSP:
      {
        int ok = 0;
        uart_send("AT\r\n",4);
        app_timer_start(timer_id,APP_TIMER_TICKS(5000),NULL);
      }
      break;
    case STATE_WORK:
      {
        static uint8_t send = 0;
        static uint8_t message[0x400];
        // Work
        switch(modem_request_state){
          case REQUEST_IDLE:{
              if(send)
              {
                send = 0;
                sprintf(&message[strlen(message)],"\r\n");
                uart_send(message,strlen(message));
                modem_request_state = REQUEST_SEND;
              }
            }break;
          case REQUEST_SEND:{
            }break;
          case REQUEST_RESPONSE:{
              memset(message,0,sizeof(message));
              modem_request_state = REQUEST_IDLE;
            }break;
        };
        app_timer_start(timer_id,APP_TIMER_TICKS(10),NULL);
      }
      break;
    case STATE_NO_WORK:
      {
        // Work
        app_timer_start(timer_id,APP_TIMER_TICKS(10),NULL);
      }
      break;
  }
  
}


void modem_init(modem_handler_t _handler)
{
  POWER_KEY_OFF();
  GSM_OFF();
  NRF_P0->PIN_CNF[4] = ((uint32_t)GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_INPUT_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_PULL_Pos)
                             | ((uint32_t)GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_SENSE_Pos);
  NRF_P0->PIN_CNF[5] = ((uint32_t)GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_INPUT_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_PULL_Pos)
                             | ((uint32_t)GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_SENSE_Pos);
  NRF_P0->PIN_CNF[6] = ((uint32_t)GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_INPUT_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_PULL_Pos)
                             | ((uint32_t)GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_SENSE_Pos);
  POWER_KEY_OFF();
  GSM_OFF();
  
  if(NRF_SUCCESS != app_timer_create(&timer_id,APP_TIMER_MODE_SINGLE_SHOT,modem_handler))
  {
    while(1);
  }
  app_timer_start(timer_id,APP_TIMER_TICKS(1000),NULL);
  handler = _handler;
  uart_init();
  uart_cb(uart_cb_init);
}
void modem_deinit(void)
{
  handler = default_handler;
  uart_deinit();
}


static void default_handler(modem_state_t state)
{
}