#ifndef __LED_H__
#define __LED_H__
#include <stdint.h>
#define LEDS_NUM 3

typedef enum {
  LED_MODE_NONE = -1,
  LED_MODE_OFF  = 0,
  LED_MODE_ON   = 1,
  LED_MODE_BLINK= 2,
  LED_MODE_FAST_BLINK = 3,
  LED_MODE_ULTRA_FAST_BLINK = 4,
  LED_MODE_SLOW_BLINK= 5,
} led_mode_t;

enum led_id_e {
  LED_1 = 0,
  LED_2 = 1,
  LED_3 = 2
} ;


void led_init();
void led_set(enum led_id_e led,led_mode_t mode);
led_mode_t led_get(enum led_id_e led);
void led_deinit();


#endif//__LED_H__
