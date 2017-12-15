#ifndef __DS18B20_H__
#define __DS18B20_H__

enum ds18b20_device_e {
  DS18B20_CH0,
  DS18B20_CH1,
  DS18B20_CH2,
  DS18B20_CH3,
};

int ds18b20_reset( int device );
void ds18b20_start_measure( int device );
int ds18b20_read_measure( int device, uint8_t *data );

#endif//__DS18B20_H__
