#include "led.h"
#include "nrf.h"
#include "app_timer.h"

struct led_struct_s {
  led_mode_t mode;
  int gpio;
  int temp;
};
static struct led_struct_s leds[LEDS_NUM];
static app_timer_t timer;
static app_timer_id_t timer_id = &timer;
static void led_handler(void * p_context)
{
  for(int i=0;i<LEDS_NUM;i++)
  switch(leds[i].mode)
  {
    case LED_MODE_OFF:
      NRF_P0->OUTCLR = (1<<leds[i].gpio);
      break;
    case LED_MODE_ON:
      NRF_P0->OUTSET = (1<<leds[i].gpio);
      break;
    case LED_MODE_BLINK:
      if(leds[i].temp++>5){
        leds[i].temp = 0;
        if(NRF_P0->OUT & (1<<leds[i].gpio))
          NRF_P0->OUTCLR = (1<<leds[i].gpio);
        else
          NRF_P0->OUTSET = (1<<leds[i].gpio);
      }
      break;
    case LED_MODE_FAST_BLINK:
      if(leds[i].temp++>1){
        leds[i].temp = 0;
        if(NRF_P0->OUT & (1<<leds[i].gpio))
          NRF_P0->OUTCLR = (1<<leds[i].gpio);
        else
          NRF_P0->OUTSET = (1<<leds[i].gpio);
      }
      break;
  }
}

void led_init()
{
    NRF_P0->OUTCLR = (0x7<<22);
    NRF_P0->PIN_CNF[22] = ((uint32_t)GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_INPUT_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_PULL_Pos)
                               | ((uint32_t)GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_SENSE_Pos);
    NRF_P0->PIN_CNF[23] = ((uint32_t)GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_INPUT_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_PULL_Pos)
                               | ((uint32_t)GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_SENSE_Pos);
    NRF_P0->PIN_CNF[24] = ((uint32_t)GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_INPUT_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_PULL_Pos)
                               | ((uint32_t)GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_SENSE_Pos);
    NRF_P0->OUTCLR = (0x7<<22);
    
    leds[0].gpio = 22;
    leds[0].mode = LED_MODE_OFF;
    
    leds[1].gpio = 23;
    leds[1].mode = LED_MODE_OFF;
    
    leds[2].gpio = 24;
    leds[2].mode = LED_MODE_OFF;
    
    if(NRF_SUCCESS != app_timer_create(&timer_id,APP_TIMER_MODE_REPEATED,led_handler))
    {
      while(1);
    }
    app_timer_start(timer_id,APP_TIMER_TICKS(100),NULL);
}

void led_set(enum led_id_e led,led_mode_t mode)
{
  if(led>=0 && led<LEDS_NUM)
  {
    leds[led].mode = mode;
    if(mode == LED_MODE_ON){
      NRF_P0->OUTSET = (1<<leds[led].gpio);
    } else {
      NRF_P0->OUTCLR = (1<<leds[led].gpio);
    }
  }
}

led_mode_t led_get(enum led_id_e led)
{
  if(led>=0 && led<LEDS_NUM)
    return leds[led].mode;
  return LED_MODE_NONE;
}

void led_deinit()
{
    NRF_P0->OUTCLR = (0x7<<22);
    NRF_P0->PIN_CNF[22] = ((uint32_t)GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_INPUT_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_PULL_Pos)
                               | ((uint32_t)GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_SENSE_Pos);
    NRF_P0->PIN_CNF[23] = ((uint32_t)GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_INPUT_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_PULL_Pos)
                               | ((uint32_t)GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_SENSE_Pos);
    NRF_P0->PIN_CNF[24] = ((uint32_t)GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_INPUT_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_PULL_Pos)
                               | ((uint32_t)GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                               | ((uint32_t)0 << GPIO_PIN_CNF_SENSE_Pos);
}