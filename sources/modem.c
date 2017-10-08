#include "modem.h"
#include "app_timer.h"


#define POWER_KEY_ON()  {NRF_P0->OUTSET = 1<<5;}
#define POWER_KEY_OFF() {NRF_P0->OUTCLR = 1<<5;}
#define GSM_ON()        {NRF_P0->OUTSET = 1<<6;}
#define GSM_OFF()       {NRF_P0->OUTCLR = 1<<6;}
#define GSM_STATUS()    ((NRF_P0->IN & 1<<4) > 0)

static void default_handler(modem_state_t state);
static modem_handler_t handler = default_handler;
static app_timer_id_t timer_id;

#define STATE_INIT              0x00
#define STATE_PWR_KEY_ON        0x01
#define STATE_PWR_KEY_OFF       0x02
#define STATE_CHECK_STATUS      0x03
#define STATE_WORK              0x04
static int state = STATE_INIT;

static void modem_handler(void * p_context)
{
  switch(state)
  {
    case STATE_INIT:app_timer_start(timer_id,APP_TIMER_TICKS(600),NULL);
      state=STATE_PWR_KEY_ON;
      GSM_ON();
      break;
    case STATE_PWR_KEY_ON:app_timer_start(timer_id,APP_TIMER_TICKS(1000),NULL);
      state=STATE_PWR_KEY_OFF;
      GSM_ON();
      POWER_KEY_ON();
      break;
    case STATE_PWR_KEY_OFF:app_timer_start(timer_id,APP_TIMER_TICKS(2500),NULL);
      state=STATE_CHECK_STATUS;
      POWER_KEY_OFF();
      break;
    case STATE_CHECK_STATUS:
      if(GSM_STATUS() > 0)
      {
        state = STATE_WORK;
      }
      app_timer_start(timer_id,APP_TIMER_TICKS(100),NULL);
      break;
    case STATE_WORK:
      {
        // Work
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
  app_timer_start(timer_id,APP_TIMER_TICKS(100),NULL);
  handler = _handler;
}
void modem_deinit(void)
{
  handler = default_handler;
}


static void default_handler(modem_state_t state)
{
}