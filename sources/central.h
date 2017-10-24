#ifndef __CENTRAL_H__
#define __CENTRAL_H__

#include <stdint.h>

#define MAC_DATA_LIST_SIZE (20)

typedef struct mac_data_s {
  uint32_t timestamp;
  uint8_t mac[6];
  uint8_t name[0x10];
  uint16_t id;
  int16_t data[4];
} mac_data_t;

extern mac_data_t central_mac_data_list[];
extern int central_mac_data_size;

void central_init();
void central_handler();
void central_deinit();

#endif