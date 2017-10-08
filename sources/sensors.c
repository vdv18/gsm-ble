#include "sensors.h"
#include "nrf.h"
#include "app_timer.h"

#define SAADC_DATA_CNT 4
static uint32_t saadc_data[SAADC_DATA_CNT];

static void default_handler( enum sensors_index_e sensor, uint64_t data );
static sensors_handler_t handler = default_handler;
static app_timer_id_t timer_id;

void SAADC_IRQHandler()
{
  if(NRF_SAADC->RESULT.AMOUNT >= SAADC_DATA_CNT)
  {
    
  }
}


static void sensors_handler(void * p_context)
{
  // Start measure data
  NRF_SAADC->RESULT.PTR = (uint32_t)&saadc_data;
}

void sensors_init(sensors_handler_t handler)
{
  if(NRF_SUCCESS != app_timer_create(&timer_id,APP_TIMER_MODE_REPEATED,sensors_handler))
  {
    while(1);
  }
  app_timer_start(timer_id,APP_TIMER_TICKS(1000),NULL);
  
  //
  
  NRF_SAADC->CH[0].PSELP=5;
  NRF_SAADC->CH[0].PSELN=9;
  
  NRF_SAADC->CH[1].PSELP=6;
  NRF_SAADC->CH[1].PSELN=9;
  
  NRF_SAADC->CH[2].PSELP=7;
  NRF_SAADC->CH[2].PSELN=9;
  
  NRF_SAADC->CH[3].PSELP=8;
  NRF_SAADC->CH[3].PSELN=9;
  
  NRF_SAADC->CH[0].CONFIG = (0<<0) | (0<<4) | (0<<8) | (0<<12) | (5<<16) | (0<<20) | (0<<24);
  NRF_SAADC->CH[1].CONFIG = (0<<0) | (0<<4) | (0<<8) | (0<<12) | (5<<16) | (0<<20) | (0<<24);
  NRF_SAADC->CH[2].CONFIG = (0<<0) | (0<<4) | (0<<8) | (0<<12) | (5<<16) | (0<<20) | (0<<24);
  NRF_SAADC->CH[3].CONFIG = (0<<0) | (0<<4) | (0<<8) | (0<<12) | (5<<16) | (0<<20) | (0<<24);
  
  
  NRF_SAADC->RESULT.PTR = (uint32_t)&saadc_data;
  NRF_SAADC->RESULT.MAXCNT = SAADC_DATA_CNT;
  
  NRF_SAADC->INTENSET = (1<<1);
  
  NRF_SAADC->SAMPLERATE = (2047)&0x7FF;
  NRF_SAADC->OVERSAMPLE = 0;
  NRF_SAADC->RESOLUTION = 2;
  NRF_SAADC->ENABLE = 1;
}

void sensors_deinit()
{
  
}


static void default_handler( enum sensors_index_e sensor, uint64_t data )
{
}