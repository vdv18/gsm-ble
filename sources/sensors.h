#ifndef __SENSORS_H__
#define __SENSORS_H__
#include <stdint.h>
enum sensors_index_e {
  SENSOR_ADC_1,
  SENSOR_ADC_2,
  SENSOR_ADC_3,
  SENSOR_ADC_4,
  SENSOR_I2C_PARAM_1,
};

typedef void (*sensors_handler_t)( enum sensors_index_e sensor, uint64_t data );

void sensors_init(sensors_handler_t handler);
void sensors_deinit();

#endif//__SENSORS_H__