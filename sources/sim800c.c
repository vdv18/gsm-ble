#include "sim800c.h"
#include "uart.h"
#include "app_timer.h"

#define POWER_KEY_ON()  {NRF_P0->OUTSET = 1<<5;}
#define POWER_KEY_OFF() {NRF_P0->OUTCLR = 1<<5;}
#define IS_GSM_ON()     ((NRF_P0->OUT & 1<<6) > 0)
#define GSM_ON()        {NRF_P0->OUTCLR = 1<<6;}
#define GSM_OFF()       {NRF_P0->OUTSET = 1<<6;}
#define GSM_STATUS()    ((NRF_P0->IN & 1<<4) > 0)

static app_timer_t timer;
static app_timer_id_t timer_id = &timer;

static void default_handler(sim800c_cmd_t cmd, sim800c_resp_t resp, char *msg, int len){};
static sim800c_callback_t callback = default_handler;
static int gsm_status = 0;

enum sim800c_state_int_e {
  SIM800C_STATE_PREPARE           =0xFE,
  SIM800C_STATE_INIT              =0x00,
  SIM800C_STATE_COMMAND_PROCESS   =0x01,
  SIM800C_STATE_PWR_KEY_OFF       =0x02,
  SIM800C_STATE_CHECK_STATUS      =0x03,
  SIM800C_STATE_CHECK_AT_RSP      =0x04,
  SIM800C_STATE_WORK              =0x05,
  SIM800C_STATE_NO_WORK           =0xFF,
};
static enum sim800c_state_int_e sim800c_state = SIM800C_STATE_INIT;



typedef struct sim800c_command_s {
  sim800c_cmd_t cmd;
  void *param;
  struct sim800c_command_s *next;
}sim800c_command_t;

#define MAX_COMMANDS_NUM SIM800C_MAX_LIST_CMD
static sim800c_command_t cmd_items[MAX_COMMANDS_NUM];
static sim800c_command_t cmd_current;
static sim800c_command_t *cmd_list = 0;

  
static int sim800c_cmd_pop(sim800c_command_t *cmd);
static int sim800c_cmd_push(sim800c_cmd_t cmd, void *param);


void uart_cb_work(uart_state_t state, uint8_t *data, int len);

static uint8_t uart_cb_download = 0;
static uint8_t uart_cb_http_action_data_resp = 0;
static uint8_t uart_cb_ok = 0;
static uint8_t uart_cb_call_rdy = 0;
static uint8_t uart_cb_sms_rdy = 0;
static uint8_t uart_cb_err = 0;

void uart_cb_work(uart_state_t state, uint8_t *data, int len)
{
  switch(state)
  {
    case UART_RECV_MSG:
      if(strstr(data,"OK") != 0)
      {
        uart_cb_ok = 1;
        callback(SIM800C_CMD_NOP, SIM800C_CALL_READY, NULL, 0);
      }
      if(strstr(data,"ERR") != 0)
      {
        uart_cb_err = 1;
        callback(SIM800C_CMD_NOP, SIM800C_CALL_READY, NULL, 0);
      }
      if(strstr(data,"Call Ready") != 0)
      {
        uart_cb_call_rdy = 1;
        callback(SIM800C_CMD_NOP, SIM800C_CALL_READY, NULL, 0);
      }
      if(strstr(data,"SMS Ready") != 0)
      {
        uart_cb_sms_rdy = 1;
        callback(SIM800C_CMD_NOP, SIM800C_SMS_READY, NULL, 0);
      }
      if(strstr(data,"DOWNLOAD") != 0)
      {
        uart_cb_download = 1;
        callback(cmd_current.cmd, SIM800C_RESP_HTTP_DOWNLOAD, NULL, 0);
      }
      if(strstr(data,"+HTTPACTION:") != 0)
      {
        uart_cb_http_action_data_resp = 1;
        callback(SIM800C_CMD_HTTPACTION, SIM800C_RESP_HTTP_ACTION_STATUS, NULL, 0);
      }
      break;
    case UART_SEND_MSG_COMPLETE:
      break;
  }
}

static void sim800c_command_handler(void *p_context);
static void sim800c_handler(void * p_context);
int sim800c_init()
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
                             | ((uint32_t)GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos)
                             | ((uint32_t)GPIO_PIN_CNF_DRIVE_H0D1 << GPIO_PIN_CNF_DRIVE_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_SENSE_Pos);
  POWER_KEY_OFF();
  GSM_OFF();
  callback = default_handler;
  uart_init();
  uart_cb(uart_cb_work);
  for(int i=0;i<MAX_COMMANDS_NUM;i++)
  {
    cmd_items[i].cmd = SIM800C_CMD_NOP;
  }
  cmd_list = 0;
  if(NRF_SUCCESS != app_timer_create(&timer_id,APP_TIMER_MODE_SINGLE_SHOT,sim800c_command_handler))
  {
    while(1);
  }
  app_timer_start(timer_id,APP_TIMER_TICKS(10),NULL);
  return 0;
}

int sim800c_set_cb(sim800c_callback_t cb)
{
  if(cb)
    callback = cb;
  return 0;
}

int sim800c_deinit()
{
  callback = default_handler;
  uart_deinit();  
  POWER_KEY_OFF();
  GSM_OFF();
  return 0;
}

static void sim800c_command_handler_timer(uint32_t tick)
{
  app_timer_start(timer_id,tick,NULL);
}

static int sim800c_command_power_on(int *cmd_state);
static int sim800c_command_power_off(int *cmd_state);
static int sim800c_command_power_down(int *cmd_state);
static int sim800c_command_sapbr(int *cmd_state);
static int sim800c_command_httpinit(int *cmd_state);
static int sim800c_command_httppara(int *cmd_state);
static int sim800c_command_httpdata(int *cmd_state);
static int sim800c_command_httpaction(int *cmd_state);
static int sim800c_command_httpterm(int *cmd_state);

static void sim800c_command_handler(void *p_context)
{
#define CMD_STATE_DEFAULT 0
#define CMD_STATE_INIT 1
static int cmd_state = CMD_STATE_DEFAULT;
//  if( (!cmd_list) && (cmd_current.cmd == SIM800C_CMD_NOP)){
//    return;
//  }
  if( GSM_STATUS() != gsm_status )
  {
    gsm_status = GSM_STATUS();
    if(gsm_status)
      callback(SIM800C_CMD_NOP, SIM800C_STATUS_ENABLE, NULL, 0);
    else
      callback(SIM800C_CMD_NOP, SIM800C_STATUS_DISABLE, NULL, 0);
  }
  switch(cmd_current.cmd){
    case SIM800C_CMD_NOP:
      {
        if(cmd_list)
        {
          if( 0 == sim800c_cmd_pop(&cmd_current))
          {
            cmd_state = CMD_STATE_INIT;
            sim800c_command_handler_timer(10);
            return;
          }
        }
      }
      break;
    case SIM800C_CMD_POWER_ON:
      sim800c_command_power_on(&cmd_state);
      return;
    case SIM800C_CMD_POWER_OFF:
      sim800c_command_power_off(&cmd_state);
      return;
    case SIM800C_CMD_POWER_DOWN:
      sim800c_command_power_down(&cmd_state);
      return;
    case SIM800C_CMD_AT:
      sim800c_command_at(&cmd_state);
      return;
    case SIM800C_CMD_SAPBR:
      sim800c_command_sapbr(&cmd_state);
      return;
    case SIM800C_CMD_HTTPINIT:
      sim800c_command_httpinit(&cmd_state);
      break;
    case SIM800C_CMD_HTTPPARA:
      sim800c_command_httppara(&cmd_state);
      break;
    case SIM800C_CMD_HTTPDATA:
      sim800c_command_httpdata(&cmd_state);
      break;
    case SIM800C_CMD_HTTPACTION:
      sim800c_command_httpaction(&cmd_state);
      break;
    case SIM800C_CMD_HTTPTERM:
      sim800c_command_httpterm(&cmd_state);
      break;
  };
  sim800c_command_handler_timer(100);
}

static void sim800c_handler(void * p_context)
{
  int low_power = 1;
  
  switch(sim800c_state)
  {
    case SIM800C_STATE_COMMAND_PROCESS:
      if( (!cmd_list) && (cmd_current.cmd == SIM800C_CMD_NOP))
      {
        app_timer_start(timer_id,APP_TIMER_TICKS(1000),NULL);
      }
      else
      {
        sim800c_command_handler(p_context);
      }
      break;
  }
}


int sim800c_cmd_send(sim800c_cmd_t cmd, void *param)
{
//  if(!cmd_list)
//    app_timer_start(timer_id,APP_TIMER_TICKS(10),NULL);
  return sim800c_cmd_push(cmd, param);
}


static int sim800c_cmd_pop(sim800c_command_t *cmd)
{
  sim800c_command_t *item = cmd_list;
  if(!cmd_list)
    return -1;
  
  memcpy(cmd,item,sizeof(sim800c_command_t));
  cmd_list->cmd = SIM800C_CMD_NOP;
  cmd_list = cmd_list->next;
  return 0;
}

static int sim800c_cmd_push(sim800c_cmd_t cmd, void *param)
{
  sim800c_command_t *item = cmd_list;
  for(int i=0;i<MAX_COMMANDS_NUM;i++)
  {
    if(cmd_items[i].cmd == SIM800C_CMD_NOP){
      cmd_items[i].cmd = cmd;
      cmd_items[i].param = param;
      if(item)
      {
        while(item->next != 0)
          { item = item->next; }
        item->next = &cmd_items[i];
        item->next->next = 0;
      }
      else
      {
        cmd_list = &cmd_items[i];
        cmd_list->next = 0;
      }
      return 0;
    }
  }
  return -1;
}

static int sim800c_command_httpaction(int *cmd_state)
{
  static int timeout = 0;
  struct httpaction_param_s *param;
#define CMD_STATE_WAIT_UART_RESP (CMD_STATE_INIT+3)
#define CMD_STATE_RESP_OK (CMD_STATE_INIT+4)
#define CMD_STATE_RESP_TIMEOUT (CMD_STATE_INIT+5)
#define CMD_STATE_RESP_ERROR (CMD_STATE_INIT+6)
  
  switch(*cmd_state)
  {
    case CMD_STATE_INIT:
      {
        uint8_t at_request[0xFF];
        *cmd_state = CMD_STATE_WAIT_UART_RESP;
        timeout = 20;
        uart_cb_ok = uart_cb_err = 0;
        param = (struct httpaction_param_s*)cmd_current.param;
        sprintf(at_request,"AT+HTTPACTION=%d\r\n",param->action);
        uart_send(at_request,strlen(at_request));
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
      }
      break;
    case CMD_STATE_WAIT_UART_RESP:
      {
        if(timeout) timeout--;
        else
        {
          *cmd_state = CMD_STATE_RESP_TIMEOUT;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          break;
        }
        if(uart_cb_ok)
        {
          uart_cb_ok = 0;
          *cmd_state = CMD_STATE_RESP_OK;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          break;
        }       
        if(uart_cb_err)
        {
          uart_cb_err = 0;
          *cmd_state = CMD_STATE_RESP_ERROR;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          break;
        }
        sim800c_command_handler_timer(APP_TIMER_TICKS(100));
      }
      break;
    case CMD_STATE_RESP_TIMEOUT:
      {
        cmd_current.cmd = SIM800C_CMD_NOP;
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_TIMEOUT,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
      }
      break;
    case CMD_STATE_RESP_OK:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_OK,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
    case CMD_STATE_RESP_ERROR:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_ERR,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
    default:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_OTHER,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
  }
  return 0;
}

static int sim800c_command_httpdata(int *cmd_state)
{
  static int timeout = 0;
  struct httpdata_param_s *param;
#define CMD_STATE_WAIT_UART_RESP (CMD_STATE_INIT+3)
#define CMD_STATE_RESP_OK (CMD_STATE_INIT+4)
#define CMD_STATE_RESP_TIMEOUT (CMD_STATE_INIT+5)
#define CMD_STATE_RESP_ERROR (CMD_STATE_INIT+6)
#define CMD_STAT_SEND_DATA (CMD_STATE_INIT+7)
#define CMD_STAT_SEND_DATA_WAIT (CMD_STATE_INIT+8)
  
  switch(*cmd_state)
  {
    case CMD_STATE_INIT:
      {
        uint8_t at_request[0xFF];
        *cmd_state = CMD_STATE_WAIT_UART_RESP;
        timeout = 20;
        uart_cb_ok = uart_cb_err = 0;
        param = (struct httpdata_param_s*)cmd_current.param;
        sprintf(at_request,"AT+HTTPDATA=%d,%d\r\n",param->data_len,10000);
        uart_send(at_request,strlen(at_request));
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
      }
      break;
    case CMD_STATE_WAIT_UART_RESP:
      {
        if(timeout) timeout--;
        else
        {
          *cmd_state = CMD_STATE_RESP_TIMEOUT;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          break;
        }
        if(uart_cb_download)
        {
          uart_cb_download = 0;
          *cmd_state = CMD_STAT_SEND_DATA;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          break;
        }
        if(uart_cb_err)
        {
          uart_cb_err = 0;
          *cmd_state = CMD_STATE_RESP_ERROR;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        }
        sim800c_command_handler_timer(APP_TIMER_TICKS(100));
      }
      break;
    case CMD_STAT_SEND_DATA:
      {
        timeout = 100;
        param = (struct httpdata_param_s*)cmd_current.param;
        uart_send_buffer(param->data,param->data_len);
        *cmd_state = CMD_STAT_SEND_DATA_WAIT;
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
      }
      break;
    case CMD_STAT_SEND_DATA_WAIT:
      {
        if(timeout) timeout--;
        else
        {
          *cmd_state = CMD_STATE_RESP_TIMEOUT;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          break;
        }
        if(uart_cb_ok)
        {
          uart_cb_ok = 0;
          *cmd_state = CMD_STATE_RESP_OK;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          break;
        }   
        if(uart_cb_err)
        {
          uart_cb_err = 0;
          *cmd_state = CMD_STATE_RESP_ERROR;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          break;
        }
        sim800c_command_handler_timer(APP_TIMER_TICKS(100));
      }
      break;
    case CMD_STATE_RESP_TIMEOUT:
      {
        cmd_current.cmd = SIM800C_CMD_NOP;
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_TIMEOUT,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
      }
      break;
    case CMD_STATE_RESP_OK:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_OK,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
    case CMD_STATE_RESP_ERROR:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_ERR,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
    default:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_OTHER,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
  }
  return 0;
}

static int sim800c_command_httppara(int *cmd_state)
{
  static int timeout = 0;
  struct httppara_param_s *param;
#define CMD_STATE_WAIT_UART_RESP (CMD_STATE_INIT+3)
#define CMD_STATE_RESP_OK (CMD_STATE_INIT+4)
#define CMD_STATE_RESP_TIMEOUT (CMD_STATE_INIT+5)
#define CMD_STATE_RESP_ERROR (CMD_STATE_INIT+6)
  
  switch(*cmd_state)
  {
    case CMD_STATE_INIT:
      {
        uint8_t at_request[0xFF];
        *cmd_state = CMD_STATE_WAIT_UART_RESP;
        timeout = 20;
        uart_cb_ok = uart_cb_err = 0;
        param = (struct httppara_param_s*)cmd_current.param;
        sprintf(at_request,"AT+HTTPPARA=%s,%s\r\n",param->param_tag,param->param_value);
        uart_send(at_request,strlen(at_request));
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
      }
      break;
    case CMD_STATE_WAIT_UART_RESP:
      {
        if(timeout) timeout--;
        else
        {
          *cmd_state = CMD_STATE_RESP_TIMEOUT;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          break;
        }
        if(uart_cb_ok)
        {
          uart_cb_ok = 0;
          *cmd_state = CMD_STATE_RESP_OK;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        }
        
        if(uart_cb_err)
        {
          uart_cb_err = 0;
          *cmd_state = CMD_STATE_RESP_ERROR;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        }
        sim800c_command_handler_timer(APP_TIMER_TICKS(100));
      }
      break;
    case CMD_STATE_RESP_TIMEOUT:
      {
        cmd_current.cmd = SIM800C_CMD_NOP;
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_TIMEOUT,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
      }
      break;
    case CMD_STATE_RESP_OK:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_OK,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
    case CMD_STATE_RESP_ERROR:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_ERR,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
    default:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_OTHER,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
  }
  return 0;
}

static int sim800c_command_httpinit(int *cmd_state)
{
  static int timeout = 0;
#define CMD_STATE_WAIT_UART_RESP (CMD_STATE_INIT+3)
#define CMD_STATE_RESP_OK (CMD_STATE_INIT+4)
#define CMD_STATE_RESP_TIMEOUT (CMD_STATE_INIT+5)
#define CMD_STATE_RESP_ERROR (CMD_STATE_INIT+6)
  
  switch(*cmd_state)
  {
    case CMD_STATE_INIT:
      {
        uint8_t data[0xFF];
        *cmd_state = CMD_STATE_WAIT_UART_RESP;
        timeout = 20;
        uart_cb_ok = uart_cb_err = 0;
        sprintf(data,"AT+HTTPINIT\r\n");
        uart_send(data,strlen(data));
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
      }
      break;
    case CMD_STATE_WAIT_UART_RESP:
      {
        if(timeout) timeout--;
        else
        {
          *cmd_state = CMD_STATE_RESP_TIMEOUT;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          break;
        }
        if(uart_cb_ok || uart_cb_err)
        {
          if(uart_cb_ok)
          {
            uart_cb_ok = 0;
            *cmd_state = CMD_STATE_RESP_OK;
            sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          }
          
          if(uart_cb_err)
          {
            uart_cb_err = 0;
            *cmd_state = CMD_STATE_RESP_ERROR;
            sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          }
        }
        sim800c_command_handler_timer(APP_TIMER_TICKS(100));
      }
      break;
    case CMD_STATE_RESP_TIMEOUT:
      {
        cmd_current.cmd = SIM800C_CMD_NOP;
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_TIMEOUT,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
      }
      break;
    case CMD_STATE_RESP_OK:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_OK,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
    case CMD_STATE_RESP_ERROR:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_ERR,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
    default:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_OTHER,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
  }
  return 0;
}

static int sim800c_command_httpterm(int *cmd_state)
{
  static int timeout = 0;
#define CMD_STATE_WAIT_UART_RESP (CMD_STATE_INIT+3)
#define CMD_STATE_RESP_OK (CMD_STATE_INIT+4)
#define CMD_STATE_RESP_TIMEOUT (CMD_STATE_INIT+5)
#define CMD_STATE_RESP_ERROR (CMD_STATE_INIT+6)
  
  switch(*cmd_state)
  {
    case CMD_STATE_INIT:
      {
        uint8_t data[0xFF];
        *cmd_state = CMD_STATE_WAIT_UART_RESP;
        timeout = 20;
        uart_cb_ok = uart_cb_err = 0;
        sprintf(data,"AT+HTTPTERM\r\n");
        uart_send(data,strlen(data));
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
      }
      break;
    case CMD_STATE_WAIT_UART_RESP:
      {
        if(timeout) timeout--;
        else
        {
          *cmd_state = CMD_STATE_RESP_TIMEOUT;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          break;
        }
        if(uart_cb_ok || uart_cb_err)
        {
          if(uart_cb_ok)
          {
            uart_cb_ok = 0;
            *cmd_state = CMD_STATE_RESP_OK;
            sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          }
          
          if(uart_cb_err)
          {
            uart_cb_err = 0;
            *cmd_state = CMD_STATE_RESP_ERROR;
            sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          }
        }
        sim800c_command_handler_timer(APP_TIMER_TICKS(100));
      }
      break;
    case CMD_STATE_RESP_TIMEOUT:
      {
        cmd_current.cmd = SIM800C_CMD_NOP;
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_TIMEOUT,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
      }
      break;
    case CMD_STATE_RESP_OK:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_OK,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
    case CMD_STATE_RESP_ERROR:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_ERR,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
    default:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_OTHER,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
  }
  return 0;
}

static int sim800c_command_at(int *cmd_state)
{
  static int timeout = 0;
#define CMD_STATE_WAIT_UART_RESP (CMD_STATE_INIT+3)
#define CMD_STATE_RESP_OK (CMD_STATE_INIT+4)
#define CMD_STATE_RESP_TIMEOUT (CMD_STATE_INIT+5)
#define CMD_STATE_RESP_ERROR (CMD_STATE_INIT+6)
  
  switch(*cmd_state)
  {
    case CMD_STATE_INIT:
      {
        uint8_t data[0xFF];
        *cmd_state = CMD_STATE_WAIT_UART_RESP;
        timeout = 20;
        uart_cb_ok = uart_cb_err = 0;
        sprintf(data,"AT\r\n");
        uart_send(data,strlen(data));
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
      }
      break;
    case CMD_STATE_WAIT_UART_RESP:
      {
        if(timeout) timeout--;
        else
        {
          *cmd_state = CMD_STATE_RESP_TIMEOUT;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          break;
        }
        if(uart_cb_ok || uart_cb_err)
        {
          if(uart_cb_ok)
          {
            uart_cb_ok = 0;
            *cmd_state = CMD_STATE_RESP_OK;
            sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          }
          
          if(uart_cb_err)
          {
            uart_cb_err = 0;
            *cmd_state = CMD_STATE_RESP_ERROR;
            sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          }
        }
        sim800c_command_handler_timer(APP_TIMER_TICKS(100));
      }
      break;
    case CMD_STATE_RESP_TIMEOUT:
      {
        cmd_current.cmd = SIM800C_CMD_NOP;
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_TIMEOUT,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
      }
      break;
    case CMD_STATE_RESP_OK:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_OK,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
    case CMD_STATE_RESP_ERROR:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_ERR,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
    default:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_OTHER,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
  }
  return 0;
}

static int sim800c_command_sapbr(int *cmd_state)
{
  static int timeout = 0;
  struct sapbr_param_s *param;
#define CMD_STATE_WAIT_UART_RESP (CMD_STATE_INIT+3)
#define CMD_STATE_RESP_OK (CMD_STATE_INIT+4)
#define CMD_STATE_RESP_TIMEOUT (CMD_STATE_INIT+5)
#define CMD_STATE_RESP_ERROR (CMD_STATE_INIT+6)
  
  switch(*cmd_state)
  {
    case CMD_STATE_INIT:
      {
        uint8_t at_request[0xFF];
        *cmd_state = CMD_STATE_WAIT_UART_RESP;
        timeout = 20;
        uart_cb_ok = uart_cb_err = 0;
        param = (struct sapbr_param_s*)cmd_current.param;
        switch(param->cmd_type)
        {
          case 0:
          case 1:
          case 2:
            sprintf(at_request,"AT+SAPBR=%d,%d\r\n",param->cmd_type,param->cid);
            break;
          case 3:
            sprintf(at_request,"AT+SAPBR=%d,%d,\"%s\",\"%s\"\r\n",param->cmd_type,param->cid,param->param_tag,param->param_value);
            break;
          case 4:
            sprintf(at_request,"AT+SAPBR=%d,%d,\"%s\"\r\n",param->cmd_type,param->cid,param->param_tag);
            break;
          default:
            cmd_current.cmd = SIM800C_CMD_NOP;
            *cmd_state = CMD_STATE_INIT;
            callback(cmd_current.cmd, SIM800C_RESP_PARAM_ERR,NULL, 0);
            sim800c_command_handler_timer(APP_TIMER_TICKS(10));
            return 0;
        }
        uart_cb_ok = uart_cb_err = 0;
        uart_send(at_request,strlen(at_request));
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
      }
      break;
    case CMD_STATE_WAIT_UART_RESP:
      {
        if(timeout) timeout--;
        else
        {
          *cmd_state = CMD_STATE_RESP_TIMEOUT;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          break;
        }
        if(uart_cb_ok)
        {
          uart_cb_ok = 0;
          *cmd_state = CMD_STATE_RESP_OK;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        }
        
        if(uart_cb_err)
        {
          uart_cb_err = 0;
          *cmd_state = CMD_STATE_RESP_ERROR;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        }
        sim800c_command_handler_timer(APP_TIMER_TICKS(100));
      }
      break;
    case CMD_STATE_RESP_TIMEOUT:
      {
        cmd_current.cmd = SIM800C_CMD_NOP;
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_TIMEOUT,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
      }
      break;
    case CMD_STATE_RESP_OK:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_OK,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
    case CMD_STATE_RESP_ERROR:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_ERR,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
    default:
      {
        *cmd_state = CMD_STATE_INIT;
        callback(cmd_current.cmd, SIM800C_RESP_OTHER,NULL, 0);
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        cmd_current.cmd = SIM800C_CMD_NOP;
      }
      break;
  }
  return 0;
}

static int sim800c_command_power_on(int *cmd_state)
{
  static int timeout = 0;
#define CMD_STATE_KEY_ON (CMD_STATE_INIT+1)
#define CMD_STATE_KEY_OFF (CMD_STATE_INIT+2)
#define CMD_STATE_STATUS_WAIT (CMD_STATE_INIT+3)
#define CMD_STATE_STATUS_COMPLETE (CMD_STATE_INIT+4)
#define CMD_STATE_STATUS_TIMEOUT (CMD_STATE_INIT+5)
  switch(*cmd_state)
  {
    case CMD_STATE_INIT:
      {
        GSM_ON();
        POWER_KEY_OFF();
        sim800c_command_handler_timer(APP_TIMER_TICKS(1000));
        *cmd_state = CMD_STATE_KEY_ON;
        timeout = 5;
      }
      break;
    case CMD_STATE_KEY_ON:
      {
        POWER_KEY_ON();
        sim800c_command_handler_timer(APP_TIMER_TICKS(1000));
        *cmd_state = CMD_STATE_KEY_OFF;
      }
      break;
    case CMD_STATE_KEY_OFF:
      {
        POWER_KEY_OFF();
        sim800c_command_handler_timer(APP_TIMER_TICKS(1000));
        *cmd_state = CMD_STATE_STATUS_WAIT;
      }
      break;
    case CMD_STATE_STATUS_WAIT:
      {
        sim800c_command_handler_timer(APP_TIMER_TICKS(1000));
        if(timeout) timeout--;
        else
        {
          *cmd_state = CMD_STATE_STATUS_TIMEOUT;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          break;
        }
        if(GSM_STATUS() > 0)
        {
          *cmd_state = CMD_STATE_STATUS_COMPLETE;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        }
      }
      break;
    case CMD_STATE_STATUS_TIMEOUT:
      {
        callback(cmd_current.cmd, SIM800C_POWER_ERROR,NULL, 0);
        cmd_current.cmd = SIM800C_CMD_NOP;
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        *cmd_state=CMD_STATE_INIT;
      }
      break;
    case CMD_STATE_STATUS_COMPLETE:
      {
        callback(cmd_current.cmd, SIM800C_POWER_ON,NULL, 0);
        cmd_current.cmd = SIM800C_CMD_NOP;
        sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        *cmd_state=CMD_STATE_INIT;
      }
      break;
  }
  return 0;
}

static int sim800c_command_power_off(int *cmd_state)
{
  static int timeout = 0;
  GSM_OFF();
  POWER_KEY_OFF();
  callback(cmd_current.cmd, SIM800C_POWER_OFF,NULL, 0);
  cmd_current.cmd = SIM800C_CMD_NOP;
  sim800c_command_handler_timer(1);
  return 0;
}

static int sim800c_command_power_down(int *cmd_state)
{
  static int timeout = 0;
  if(!IS_GSM_ON())
  {
    callback(cmd_current.cmd, SIM800C_POWER_OFF,NULL, 0);
    cmd_current.cmd = SIM800C_CMD_NOP;
    sim800c_command_handler_timer(1);
    return 0;
  }
  switch(*cmd_state)
  {
    case CMD_STATE_INIT:
      {
        POWER_KEY_OFF();
        sim800c_command_handler_timer(APP_TIMER_TICKS(1000));
        *cmd_state = CMD_STATE_KEY_ON;
        timeout = 5;
      }
      break;
    case CMD_STATE_KEY_ON:
      {
        POWER_KEY_ON();
        sim800c_command_handler_timer(APP_TIMER_TICKS(1000));
        *cmd_state = CMD_STATE_KEY_ON;
      }
      break;
    case CMD_STATE_KEY_OFF:
      {
        POWER_KEY_OFF();
        sim800c_command_handler_timer(APP_TIMER_TICKS(1000));
        *cmd_state = CMD_STATE_STATUS_WAIT;
      }
      break;
    case CMD_STATE_STATUS_WAIT:
      {
        sim800c_command_handler_timer(APP_TIMER_TICKS(1000));
        if(timeout) timeout--;
        else
        {
          *cmd_state = CMD_STATE_STATUS_TIMEOUT;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
          break;
        }
        if(GSM_STATUS() == 0)
        {
          *cmd_state = CMD_STATE_STATUS_COMPLETE;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
        }
      }
      break;
    case CMD_STATE_STATUS_COMPLETE:
      {
        callback(cmd_current.cmd, SIM800C_POWER_ERROR,NULL, 0);
        cmd_current.cmd = SIM800C_CMD_NOP;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
      }
      break;
    case CMD_STATE_STATUS_TIMEOUT:
      {
        callback(cmd_current.cmd, SIM800C_POWER_OFF,NULL, 0);
        cmd_current.cmd = SIM800C_CMD_NOP;
          sim800c_command_handler_timer(APP_TIMER_TICKS(10));
      }
      break;
  }
  return 0;
}
