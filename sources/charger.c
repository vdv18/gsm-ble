#include "charger.h"
#include "nrf.h"


void charger_init()
{
  NRF_P0->PIN_CNF[27] = ((uint32_t)GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_INPUT_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_PULL_Pos)
                             | ((uint32_t)GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_SENSE_Pos);
}

charger_state_t charger_state()
{
  return ((NRF_P0->IN >>27) & 1 == 0 )? CHARGER_STATE_YES : CHARGER_STATE_NO;
}