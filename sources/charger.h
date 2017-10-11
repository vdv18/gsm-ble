#ifndef __CHARGER_H__
#define __CHARGER_H__

typedef enum charger_state_e {
  CHARGER_STATE_YES,
  CHARGER_STATE_NO,
} charger_state_t;

void charger_init();
charger_state_t charger_state();

#endif