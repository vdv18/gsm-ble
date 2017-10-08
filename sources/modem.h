#ifndef __MODEM_H__
#define __MODEM_H__

typedef enum {
  MODEM_INITIALIZED,
  MODEM_DISABLED,
} modem_state_t;

typedef void (*modem_handler_t)(modem_state_t state);

void modem_init(modem_handler_t _handler);
void modem_deinit(void);


#endif//__MODEM_H__