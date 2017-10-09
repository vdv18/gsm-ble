#ifndef __CENTRAL_H__
#define __CENTRAL_H__

#include <stdint.h>

void advertizer_init();
void advertizer_hadler();
void advertizer_update_data(uint16_t id, uint16_t *_data,int _data_len);
void advertizer_deinit();

#endif