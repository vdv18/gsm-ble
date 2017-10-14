#ifndef __SENSORS_H__
#define __SENSORS_H__
#include <stdint.h>
enum sensors_index_e {
  SENSOR_ADC_1,
  SENSOR_ADC_2,
  SENSOR_ADC_3,
  SENSOR_ADC_4,
  SENSOR_CONVERTED_1,
  SENSOR_CONVERTED_2,
  SENSOR_CONVERTED_3,
  SENSOR_CONVERTED_4,
  SENSOR_I2C_PARAM_1,
};

typedef void (*sensors_converter_t)( uint16_t *raw, uint16_t *converted );
typedef void (*sensors_handler_t)( enum sensors_index_e sensor, uint16_t data );

void sensors_init(sensors_handler_t handler);
void sensors_set_converter(sensors_converter_t converter);
void sensors_deinit();

#endif//__SENSORS_H__