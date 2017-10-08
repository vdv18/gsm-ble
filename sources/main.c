#include "nrf.h"
#include "ble.h"
#include "nrf_sdm.h"
#include "sensors.h"
#include "modem.h"
#include "led.h"

#include "central.h"
#include "advertizer.h"

#include <stdlib.h>


void assertion_handler(uint32_t id, uint32_t pc, uint32_t info)
{
  while(1){
    NVIC_SystemReset();
  };
}
uint32_t sd_init()
{  
  uint32_t ret_code;
  nrf_clock_lf_cfg_t clock;
  
  clock.source = NRF_CLOCK_LF_SRC_XTAL;
  clock.rc_ctiv = 0;
  clock.rc_temp_ctiv = 0;
  clock.accuracy = NRF_CLOCK_LF_ACCURACY_20_PPM;
  ret_code = sd_softdevice_enable(&clock, assertion_handler);
  if (ret_code != NRF_SUCCESS)
  {
      return ret_code;
  }
  NVIC_EnableIRQ(SWI2_EGU2_IRQn);
  
  return NRF_SUCCESS;
}
static int central = 0;
void SWI2_EGU2_IRQHandler()
{
  if(central)
    central_handler();
  else
    advertizer_hadler();
}
static void power_manage(void)
{
//    ret_code_t err_code = sd_app_evt_wait();
//    APP_ERROR_CHECK(err_code);
}
static int ready = 0;
static uint64_t sensors[4];
void sensors_handler( enum sensors_index_e sensor, uint64_t data )
{
  switch(sensor)
  {
    case SENSOR_ADC_1:
      sensors[0] = data;
      break;
    case SENSOR_ADC_2:
      sensors[1] = data;
      break;
    case SENSOR_ADC_3:
      sensors[2] = data;
      break;
    case SENSOR_ADC_4:
      sensors[3] = data;
      break;
  }
}

void modem_handler(modem_state_t state)
{
  switch(state)
  {
    case MODEM_INITIALIZED:
      central = 1;
      central_init();
      sensors_init(sensors_handler);
      break;
    case MODEM_DISABLED:
      central = 0;
      advertizer_init();
      sensors_init(sensors_handler);
      break;
      
  }
}

void main()
{
  sd_init();
  if(app_timer_init() != NRF_SUCCESS)
  {
    while(1);
  }
  led_init();
//  led_set(LED_1,LED_MODE_FAST_BLINK);
//  led_set(LED_2,LED_MODE_BLINK);
//  led_set(LED_3,LED_MODE_ON);
  //modem_init(modem_handler);
  modem_handler(MODEM_DISABLED);
  while(1){
    if (ready)
    {
        power_manage();
    }
  };
}