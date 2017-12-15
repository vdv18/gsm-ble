#include "modem.h"
#include "app_timer.h"
#include "sim800c.h"
#include "nrf_delay.h"

static int post_ready = 0;
static void sim800c_callback(sim800c_cmd_t cmd, sim800c_resp_t resp, char *msg, int len);
static void default_handler(modem_state_t state, uint8_t *data, int size){};
static modem_handler_t callback = default_handler;
static app_timer_t timer;
static app_timer_id_t timer_id = &timer;

typedef struct command_list_s {
  sim800c_cmd_t cmd;
  void *param;
} command_list_t;

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
  .param_value = MODEM_APN
};
const struct sapbr_param_s sapbr_open_gprs_context = {
  .cmd_type = 1,
  .cid = 1,
};
const struct sapbr_param_s sapbr_query_gprs_context = {
  .cmd_type = 2,
  .cid = 1,
};
//
//static const command_list_t cmd_list_init[] = {
//  { SIM800C_CMD_SAPBR, (void*)&sapbr_contype },
//  { SIM800C_CMD_SAPBR, (void*)&sapbr_apn },
//  { SIM800C_CMD_SAPBR, (void*)&sapbr_open_gprs_context },
//  { SIM800C_CMD_SAPBR, (void*)&sapbr_query_gprs_context },
//};

static const struct httpaction_param_s httpaction_param = {
  .action = 1,
};
static const struct httppara_param_s httppara_cid = {
  .param_tag = "CID",
  .param_value = "1"
};
static const struct httppara_param_s httppara_url = {
  .param_tag = "URL",
  .param_value = MODEM_URL_POST,
};
static struct httpdata_param_s httpdata_param = {
  .data = 0,
  .data_len = 0
};

static struct httpread_param_s httpread_param = {
  .data = 0,
  .data_len = 0
};
//static const command_list_t cmd_list_send_post[] = {
//  { SIM800C_CMD_HTTPINIT, (void*)&sapbr_contype },
//  { SIM800C_CMD_HTTPPARA, (void*)&sapbr_apn },
//  { SIM800C_CMD_HTTPPARA, (void*)&sapbr_open_gprs_context },
//  { SIM800C_CMD_HTTPDATA, (void*)&sapbr_query_gprs_context },
//  { SIM800C_CMD_HTTPACTION, (void*)&sapbr_query_gprs_context },
//  { SIM800C_CMD_HTTPTERM, (void*)&sapbr_query_gprs_context },
//};


enum modem_state_int_e {
  STATE_INIT              =0x00,
  STATE_REINIT,
  STATE_WAIT_INIT,
  STATE_GSM_INIT,
  STATE_WORK_INIT,
  STATE_WORK,
  STATE_WORK_SEND_POST_DATA,
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
#define AT_OK   (1<<0)
#define CALL_READY (1<<1)
#define SMS_READY (1<<2)
#define SAPBR_READY (1<<3)
static uint32_t modem_status = 0;
#define HTTP_INIT (1<<0)
#define HTTP_PARA (1<<1)
#define HTTP_ACTION (1<<2)
#define HTTP_TERM (1<<3)
#define HTTP_DATA (1<<4)
#define HTTP_READ (1<<5)
#define HTTP_ACTION_STATUS (1<<6)
#define HTTP_ACTION_READ_DATA (1<<7)
#define HTTP_RESP (1<<30)

static int http_action_read = 0;
static uint32_t http_status = 0;
static uint8_t *modem_buffer = 0;
static int modem_buffer_size = 0;
static void sim800c_callback(sim800c_cmd_t cmd, sim800c_resp_t resp, char *msg, int len)
{
  switch(cmd)
  {
    case SIM800C_CMD_HTTPINIT:
      {
        switch(resp)
        {
          case SIM800C_RESP_OK:
            http_status |= HTTP_INIT | HTTP_RESP;
            break;
          case SIM800C_RESP_ERR:
            http_status |= HTTP_RESP;
            break;
        }
      }
      break;
    case SIM800C_CMD_HTTPPARA:
      {
        switch(resp)
        {
          case SIM800C_RESP_OK:
            http_status |= HTTP_PARA | HTTP_RESP;
            break;
          case SIM800C_RESP_ERR:
            http_status |= HTTP_RESP;
            break;
        }
      }
      break;
    case SIM800C_CMD_HTTPACTION:
      {
        switch(resp)
        {
          case SIM800C_RESP_HTTP_ACTION_STATUS:
            http_status |= HTTP_ACTION_STATUS | HTTP_RESP;
            if(strstr(msg,"HTTPACTION"))
            {
              char *temp = 0;
              if(temp = strstr(msg, ":"))
              {
                int data[3];
                temp++;
                if(sscanf(temp,"%d,%d,%d",&data[0],&data[1],&data[2]))
                {
                  if(data[1] == 200 && data[2] > 0)
                  {
                    http_action_read = data[2];
                    http_status |= HTTP_ACTION_READ_DATA;
                  }
                }
              }
            }
            break;
          case SIM800C_RESP_OK:
            http_status |= HTTP_ACTION | HTTP_RESP;
            break;
          case SIM800C_RESP_ERR:
            http_status |= HTTP_RESP;
            break;
        }
      }
      break;
    case SIM800C_CMD_HTTPTERM:
      {
        switch(resp)
        {
          case SIM800C_RESP_OK:
            http_status |= HTTP_TERM | HTTP_RESP;
            break;
          case SIM800C_RESP_ERR:
            http_status |= HTTP_RESP;
            break;
        }
      }
      break;
    case SIM800C_CMD_HTTPREAD:
      {
        switch(resp)
        {
          case SIM800C_RESP_OK:
            char *end,*start;
            char *dst = (char*)modem_buffer;
            modem_buffer_size = 0;
            start = strstr(msg,"DOWNLOAD");
            end = strstr(msg,"OK");
            if((end !=0) && (start != 0) && (dst != 0))
            {
              start += 8;
              while(start<=end)
              {
                *dst++ = *start++;
                modem_buffer_size++;
              }
            }
            http_status |= HTTP_READ | HTTP_RESP;
            break;
          case SIM800C_RESP_ERR:
            http_status |= HTTP_RESP;
            break;
        }
      }
      break;
    case SIM800C_CMD_HTTPDATA:
      {
        switch(resp)
        {
          case SIM800C_RESP_OK:
            http_status |= HTTP_DATA | HTTP_RESP;
            break;
          case SIM800C_RESP_ERR:
            http_status |= HTTP_RESP;
            break;
        }
      }
      break;
    case SIM800C_CMD_SAPBR:
      {
        switch(resp)
        {
          case SIM800C_RESP_OK:
            modem_status |= SAPBR_READY;
            break;
          case SIM800C_RESP_ERR:
            modem_state = STATE_NO_WORK;
            break;
        }
      }
      break;
    case SIM800C_CMD_AT:
      {
        switch(resp)
        {
          case SIM800C_RESP_OK:
            //modem_state = STATE_NO_WORK;
            modem_status |= AT_OK;
            break;
          case SIM800C_RESP_ERR:
            modem_state = STATE_NO_WORK;
            break;
        }
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
          case SIM800C_CALL_READY:
            modem_status |= CALL_READY;
            break;
          case SIM800C_SMS_READY:
            modem_status |= SMS_READY;
            break;
        }
      }
      break;
    case SIM800C_CMD_POWER_ON:
      {
        switch(resp)
        {
          case SIM800C_POWER_ON:
            modem_state = STATE_GSM_INIT;
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

void modem_set_buffer(uint8_t *data, int size)
{
  modem_buffer = data;
  modem_buffer_size = size;
}

const char test_data[] = "{\"test\":\"1\"}";
static void modem_handler(void * p_context)
{
  static int gsm_init_timeout = 0;
  static int post_data_state = 0;
  static enum modem_state_int_e modem_state_prev = STATE_INIT;
  enum modem_state_int_e modem_state_old = modem_state;
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
        if(modem_state_prev != modem_state)
        {
          gsm_init_timeout = 0;
        }
        if(gsm_init_timeout++ > 300)
        {
          gsm_init_timeout = 0;
          modem_state = STATE_REINIT;
        }
      }
      break;
    case STATE_GSM_INIT:
      {
        if(modem_state_prev != modem_state)
        {
          gsm_init_timeout = 0;
        }
        if(gsm_init_timeout % 30 == 0)
          sim800c_cmd_send(SIM800C_CMD_AT, (void*)&sapbr_contype);
         
            
        if(gsm_init_timeout++ > 300)
        {
          gsm_init_timeout = 0;
          modem_state = STATE_REINIT;
        }
        //if((modem_status & (CALL_READY | SMS_READY | AT_OK))  ==  (CALL_READY | SMS_READY))
        if((modem_status & (CALL_READY | SMS_READY | AT_OK)))
        {
          gsm_init_timeout = 0;
          modem_state = STATE_WORK_INIT;
          app_timer_start(timer_id,APP_TIMER_TICKS(2000),NULL);
          return;
        }
      }
      break;
    case STATE_WORK_INIT:
      {
        if(modem_state_prev != modem_state)
        {
          sim800c_cmd_send(SIM800C_CMD_SAPBR, (void*)&sapbr_contype);
          sim800c_cmd_send(SIM800C_CMD_SAPBR, (void*)&sapbr_apn);
          sim800c_cmd_send(SIM800C_CMD_SAPBR, (void*)&sapbr_open_gprs_context);
          sim800c_cmd_send(SIM800C_CMD_SAPBR, (void*)&sapbr_query_gprs_context);
        }
        if(modem_status & SAPBR_READY)
        {
          modem_state = STATE_WORK;
          if(0){ // Set example JSON request
            httpdata_param.data = (uint8_t *)test_data;
            httpdata_param.data_len = strlen(test_data);
            post_ready = 0;
          }
          callback(MODEM_INITIALIZED,NULL,0);
          app_timer_start(timer_id,APP_TIMER_TICKS(2000),NULL);
          return;
        }
      }
      break;
    case STATE_WORK:
      {
        if(modem_state_prev != modem_state)
        {
          callback(MODEM_READY,NULL,0);
        }
        if(post_ready)
        {
          callback(MODEM_PROCESSING,NULL,0);
          modem_state = STATE_WORK_SEND_POST_DATA;
        }
      }
      break;
    case STATE_WORK_SEND_POST_DATA:
      {
        static int timeout = 50;
        if(modem_state_prev != modem_state)
        {
          http_status = 0x00;
          post_data_state = 0;
        }
//        if(http_status & (HTTP_INIT|HTTP_PARA|HTTP_ACTION|HTTP_TERM|HTTP_DATA|HTTP_RESP) 
//           ==
//           (HTTP_INIT|HTTP_PARA|HTTP_ACTION|HTTP_TERM|HTTP_DATA|HTTP_RESP))
//        {
//          ready = 0;
//        }
        switch(post_data_state){
          case 0:
            timeout = 50;
            http_status = 0;
            sim800c_cmd_send(SIM800C_CMD_HTTPINIT, (void*)NULL); 
            post_data_state++;
            break;
          case 1:
            if(http_status & (HTTP_RESP | HTTP_INIT) == (HTTP_RESP | HTTP_INIT))
            {
              http_status &=~(HTTP_RESP);
              post_data_state++;
            }
            break;
          case 2:
            timeout = 50;
            sim800c_cmd_send(SIM800C_CMD_HTTPPARA, (void*)&httppara_cid); 
            post_data_state++;
            break;
          case 3:
            if(http_status & (HTTP_RESP | HTTP_PARA) == (HTTP_RESP | HTTP_PARA))
            {
              http_status &=~(HTTP_RESP);
              post_data_state++;
            }
            break;
          case 4:
            timeout = 50;
            sim800c_cmd_send(SIM800C_CMD_HTTPPARA, (void*)&httppara_url); 
            post_data_state++;
              app_timer_start(timer_id,APP_TIMER_TICKS(200),NULL);
              return;
            break;
          case 5:
            if(http_status & (HTTP_RESP | HTTP_PARA) == (HTTP_RESP | HTTP_PARA))
            {
              http_status &=~(HTTP_RESP);
              post_data_state++;
              app_timer_start(timer_id,APP_TIMER_TICKS(200),NULL);
              return;
            }
            break;
          case 6:
            timeout = 100;
            sim800c_cmd_send(SIM800C_CMD_HTTPDATA, (void*)&httpdata_param); 
            post_data_state++;
              app_timer_start(timer_id,APP_TIMER_TICKS(200),NULL);
              return;
            break;
          case 7:
            if(http_status & (HTTP_RESP | HTTP_PARA) == (HTTP_RESP | HTTP_PARA))
            {
              http_status &=~(HTTP_RESP);
              post_data_state++;
              app_timer_start(timer_id,APP_TIMER_TICKS(200),NULL);
              return;
            }
            break;
          case 8:
            timeout = 100;
            sim800c_cmd_send(SIM800C_CMD_HTTPACTION, (void*)&httpaction_param); 
            post_data_state++;
            app_timer_start(timer_id,APP_TIMER_TICKS(100),NULL);
            return;
          case 9: // WAIT HTTPACTION
            if( ( http_status & (HTTP_RESP | HTTP_ACTION | HTTP_ACTION_STATUS )) 
                  == 
                ( HTTP_RESP | HTTP_ACTION | HTTP_ACTION_STATUS ) )
            {
              http_status &=~(HTTP_RESP);
              if(http_status & HTTP_ACTION_READ_DATA)
              {
                post_data_state++;
              }
              else
              {
                post_data_state++;
                post_data_state++;
                post_data_state++;
              }
              app_timer_start(timer_id,APP_TIMER_TICKS(100),NULL);
              return;
            }
            break;
          case 10:
            timeout = 50;
            // size data: http_action_read;
            httpread_param.data_len = http_action_read;
            sim800c_cmd_send(SIM800C_CMD_HTTPREAD, (void*)&httpread_param); 
            post_data_state++;
            app_timer_start(timer_id,APP_TIMER_TICKS(100),NULL);
            return;
            break;
          case 11:
            if(http_status & HTTP_RESP == HTTP_RESP)
            {
              http_status &=~(HTTP_RESP);
              post_data_state++;
              if(http_status & HTTP_READ == HTTP_READ)
              {
                callback(MODEM_SETTINGS_RECV,modem_buffer,modem_buffer_size);
              }
              app_timer_start(timer_id,APP_TIMER_TICKS(100),NULL);
              return;
            }
            break;
          case 12:
            timeout = 50;
            sim800c_cmd_send(SIM800C_CMD_HTTPTERM, (void*)NULL); 
            post_data_state++;
            app_timer_start(timer_id,APP_TIMER_TICKS(100),NULL);
            return;
            break;
          case 13:
            if(http_status & HTTP_RESP == HTTP_RESP)
            {
              http_status &=~(HTTP_RESP);
              post_data_state++;
              app_timer_start(timer_id,APP_TIMER_TICKS(100),NULL);
              return;
            }
            break;
          case 14:
            timeout = 50;
            post_ready = 0;
            post_data_state = 0;
            callback(MODEM_SEND_COMPLETE,NULL,0);
            modem_state = STATE_WORK;
            break;
        };
        if(timeout) timeout--;
        else
        {
          post_data_state = 0;
          sim800c_cmd_send(SIM800C_CMD_HTTPTERM, (void*)NULL); 
          modem_state = STATE_WORK;
        }
      }
      break;
    case STATE_NO_WORK:
      {
          modem_state = STATE_REINIT;
      }
      break;
  }
  modem_state_prev = modem_state_old;
  app_timer_start(timer_id,APP_TIMER_TICKS(100),NULL);
}


void modem_send_json(uint8_t *data, int size)
{
  httpdata_param.data = (uint8_t *)data;
  httpdata_param.data_len = size;
  post_ready = 1;
}

void modem_init(modem_handler_t _handler)
{
  uint32_t temp =0;
  if(_handler)
    callback = _handler;
  if(NRF_SUCCESS != app_timer_create(&timer_id,APP_TIMER_MODE_SINGLE_SHOT,modem_handler))
  {
    while(1);
  }
  NRF_P0->OUTCLR = 1<<5;
  temp = NRF_P0->PIN_CNF[5];
  NRF_P0->PIN_CNF[5] = ((uint32_t)GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_INPUT_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_PULL_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_DRIVE_Pos)
                             | ((uint32_t)GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_SENSE_Pos);
  //NRF_P0->OUTSET = 1<<5;
  NRF_P0->OUTSET = 1<<5;
  nrf_delay_ms(10);
//  if( (NRF_P0->IN & (1<<5)) == (1<<5) )
//  {
//    NRF_P0->OUTCLR = 1<<5;
//    NRF_P0->PIN_CNF[5] = temp;
//    sim800c_init();
//    nrf_delay_ms(100);
//    app_timer_start(timer_id,APP_TIMER_TICKS(1000),NULL);
//    sim800c_set_cb(sim800c_callback);
//    callback(MODEM_INITIALIZING,NULL,0);
//  }
//  else
  {
    NRF_P0->OUTCLR = 1<<5;
    NRF_P0->PIN_CNF[5] = temp;
    callback(MODEM_DISABLED,NULL,0);
  }
}

void modem_deinit(void)
{
  callback = default_handler;
  sim800c_deinit();
}
