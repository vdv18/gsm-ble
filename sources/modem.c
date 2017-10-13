#include "modem.h"
#include "app_timer.h"
#include "sim800c.h"

static void sim800c_callback(sim800c_cmd_t cmd, sim800c_resp_t resp, char *msg, int len);
static void default_handler(modem_state_t state){};
static modem_handler_t callback = default_handler;
static app_timer_t timer;
static app_timer_id_t timer_id = &timer;

typedef struct command_list_s {
  sim800c_cmd_t cmd;
  void *param;
} command_list_t;
//struct sapbr_param_s {
//  uint8_t cmd_type;
//  uint8_t cid;
//  uint8_t param_tag[0x10];
//  uint8_t param_value[0x10];
//};
const struct sapbr_param_s sapbr_contype = {
  .cmd_type = 3,
  .cid = 1,
  .param_tag = "Contype",
  .param_value = "GPRS"
};
const struct sapbr_param_s sapbr_apn = {
  .cmd_type = 3,
  .cid = 1,
  .param_tag = "APN",
  .param_value = "Beeline"
};
const struct sapbr_param_s sapbr_open_gprs_context = {
  .cmd_type = 1,
  .cid = 1,
};
const struct sapbr_param_s sapbr_query_gprs_context = {
  .cmd_type = 1,
  .cid = 1,
};

static const command_list_t cmd_list_init[] = {
  { SIM800C_CMD_SAPBR, (void*)&sapbr_contype },
  { SIM800C_CMD_SAPBR, (void*)&sapbr_apn },
  { SIM800C_CMD_SAPBR, (void*)&sapbr_open_gprs_context },
  { SIM800C_CMD_SAPBR, (void*)&sapbr_query_gprs_context },
};


enum modem_state_int_e {
  STATE_INIT              =0x00,
  STATE_REINIT,
  STATE_WAIT_INIT,
  STATE_WORK_INIT,
  STATE_WORK,
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

static void sim800c_callback(sim800c_cmd_t cmd, sim800c_resp_t resp, char *msg, int len)
{
  switch(cmd)
  {
    case SIM800C_CMD_SAPBR:
      {
        
      }
      break;
    case SIM800C_CMD_NOP:
      {
        switch(resp)
        {
          case SIM800C_STATUS_ENABLE:
            //modem_state = STATE_NO_WORK;
            break;
          case SIM800C_STATUS_DISABLE:
            modem_state = STATE_NO_WORK;
            break;
        }
      }
      break;
    case SIM800C_CMD_POWER_ON:
      {
        switch(resp)
        {
          case SIM800C_POWER_ON:
            modem_state = STATE_WORK_INIT;
            break;
          default:
            modem_state = STATE_NO_WORK;
            break;
        }
      }
      break;
    case SIM800C_CMD_POWER_OFF:
    case SIM800C_CMD_POWER_DOWN:
      break;
  }
}

static void modem_handler(void * p_context)
{
  switch(modem_state)
  {
    case STATE_REINIT:
      {
        sim800c_cmd_send(SIM800C_CMD_POWER_OFF, NULL);
        modem_state = STATE_INIT;
      }
      break;
    case STATE_INIT:
      {
        sim800c_cmd_send(SIM800C_CMD_POWER_ON, NULL);
        modem_state = STATE_WAIT_INIT;
      }
      break;
    case STATE_WAIT_INIT:
      {
        //sim800c_cmd_send(SIM800C_CMD_POWER_ON, NULL);
      }
      break;
    case STATE_WORK_INIT:
      {
        sim800c_cmd_send(SIM800C_CMD_SAPBR, (void*)&sapbr_contype);
        modem_state = STATE_WORK;
      }
      break;
    case STATE_WORK:
      {
      }
      break;
    case STATE_NO_WORK:
      {
      }
      break;
  }
  app_timer_start(timer_id,APP_TIMER_TICKS(10),NULL);
}


void modem_init(modem_handler_t _handler)
{
  
  if(NRF_SUCCESS != app_timer_create(&timer_id,APP_TIMER_MODE_SINGLE_SHOT,modem_handler))
  {
    while(1);
  }
  app_timer_start(timer_id,APP_TIMER_TICKS(1000),NULL);
  callback = _handler;
  sim800c_init();
  sim800c_set_cb(sim800c_callback);
//  for(int i=0;i<sizeof(cmd_list_init)/sizeof(command_list_t);i++)
//  {
//    sim800c_cmd_send(cmd_list_init[0].cmd, cmd_list_init[i].param);
//  }
}

void modem_deinit(void)
{
  callback = default_handler;
  sim800c_deinit();
}
