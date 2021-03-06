#ifndef __MODEM_H__
#define __MODEM_H__

#include <stdint.h>
#include "config.h"

typedef enum {
  MODEM_INITIALIZING,
  MODEM_INITIALIZED,
  MODEM_DISABLED,
  MODEM_READY,
  MODEM_SEND_COMPLETE,
  MODEM_PROCESSING,
  MODEM_SETTINGS_RECV,
} modem_state_t;

typedef void (*modem_handler_t)(modem_state_t state, uint8_t *data, int data_size);

void modem_send_json(uint8_t *data, int size);
void modem_set_buffer(uint8_t *data, int size);
void modem_init(modem_handler_t _handler);
void modem_deinit(void);


#endif//__MODEM_H__