#ifndef __CENTRAL_H__
#define __CENTRAL_H__

#include <stdint.h>

#define MAC_DATA_LIST_SIZE              (20)
#define MAC_DATA_LIST_FLAG_ENABLED      (1<<0)
#define MAC_DATA_LIST_FLAG_SETTINGS_MAX (1<<1)
#define MAC_DATA_LIST_FLAG_SETTINGS_MIN (1<<2)
#define MAC_DATA_LIST_FLAG_SETTINGS     (3<<1)

typedef struct mac_data_s {
  uint32_t timestamp;
  uint8_t mac[6];
  uint8_t name[0x10];
  uint16_t id;
  int16_t data[4];
  uint8_t flag;
  uint32_t version;
} mac_data_t;

extern mac_data_t central_mac_data_list[];
extern int central_mac_data_size;

void central_init();
void central_off();
void central_on();
void central_handler();
void central_deinit();

#endif