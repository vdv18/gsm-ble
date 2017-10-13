#ifndef __SIM800C_H__
#define __SIM800C_H__

#include <stdint.h>
#include <string.h>

#define SIM800C_MAX_LIST_CMD 20

typedef enum {
  SIM800C_RESP_OK,
  SIM800C_RESP_ERR,
  SIM800C_RESP_PARAM_ERR,
  SIM800C_RESP_TIMEOUT,
  SIM800C_RESP_OTHER,
  SIM800C_RESP_HTTP_DOWNLOAD,
  SIM800C_SMS_IN,
  SIM800C_RING_IN,
  SIM800C_STATUS_ENABLE,
  SIM800C_STATUS_DISABLE,
  SIM800C_POWER_ON,
  SIM800C_POWER_OFF,
  SIM800C_POWER_DOWN,
  SIM800C_POWER_ERROR,
  SIM800C_CALL_READY,
  SIM800C_SMS_READY,
  SIM800C_GSM_ENABLED,
  SIM800C_GPRS_ENABLED,
} sim800c_resp_t;

typedef enum {
  SIM800C_CMD_NOP,
  SIM800C_CMD_POWER_ON,
  SIM800C_CMD_POWER_OFF,
  SIM800C_CMD_POWER_DOWN,
  SIM800C_CMD_AT,
  SIM800C_CMD_SAPBR,
  SIM800C_CMD_HTTPINIT,
  SIM800C_CMD_HTTPPARA,
  SIM800C_CMD_HTTPDATA,
  SIM800C_CMD_HTTPACTION,
  SIM800C_CMD_HTTPTERM,
} sim800c_cmd_t;

typedef void (*sim800c_callback_t)(sim800c_cmd_t cmd, sim800c_resp_t resp, char *msg, int len);

int sim800c_init();
int sim800c_set_cb(sim800c_callback_t cb);
int sim800c_deinit();

int sim800c_cmd_send(sim800c_cmd_t cmd, void *param);
int sim800c_cmd_flush(sim800c_cmd_t cmd, void *param);

//SIM800C_CMD_SAPBR
struct sapbr_param_s {
  uint8_t cmd_type;
  uint8_t cid;
  uint8_t param_tag[0x10];
  uint8_t param_value[0x10];
};
//SIM800C_CMD_HTTPACTION
struct httpaction_param_s {
  uint8_t action;
};
//SIM800C_CMD_HTTPPARA
struct httppara_param_s {
  uint8_t param_tag[0x10];
  uint8_t param_value[0x80];
};
//SIM800C_CMD_HTTPDATA
struct httpdata_param_s {
  uint8_t *data;
  uint16_t data_len;
};

#endif//__SIM800C_H__