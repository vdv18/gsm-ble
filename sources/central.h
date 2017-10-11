#ifndef __CENTRAL_H__
#define __CENTRAL_H__

#include <stdint.h>

typedef struct mac_data_s {
  uint32_t timestamp;
  uint8_t mac[6];
  uint16_t id;
  uint16_t data[4];
} mac_data_t;

extern mac_data_t data[];
extern int data_size;

void central_init();
void central_handler();
void central_deinit();

#endif