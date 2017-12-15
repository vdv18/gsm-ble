#include "advertizer.h"
#include "nrf.h"
#include "ble.h"
#include "ble_hci.h"
#include "nrf_sdm.h"

#include "app_util.h"
#include "app_timer.h"

#include <stdlib.h>
#include <stdint.h>

#define TEST_BASE_UUID                  {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x00, 0x00, 0x40, 0x6E} /**< Used vendor specific UUID. */

static void assertion_handler(uint32_t id, uint32_t pc, uint32_t info)
{
  while(1){
    NVIC_SystemReset();
  };
}
static ble_gap_adv_params_t m_adv_params;   
static app_timer_t timer1;
static app_timer_id_t timer_id = &timer1;


static uint8_t data[0x23] = "     GB0002";
static uint8_t scan_data[0x23] = "  ";
static void advertizer_timer_handler(void * p_context)
{
//  uint32_t ret_code;
//  static uint8_t temp = 0x30;
//  data[8] = temp++;
//  if(temp>0x39)temp=0x30;
//  ret_code = sd_ble_gap_adv_data_set(data, 13, 0, 0);
}

uint32_t ble_adv_init()
{
  uint32_t ret_code;
  ble_cfg_t ble_cfg;
  uint32_t *app_ram_start = (uint32_t*)0x2000b668;
  
  
  memset(&ble_cfg, 0, sizeof(ble_cfg));
  ble_cfg.conn_cfg.conn_cfg_tag = 1;
  ble_cfg.conn_cfg.params.gap_conn_cfg.conn_count = 1;
  ble_cfg.conn_cfg.params.gap_conn_cfg.event_length = 3;
  
  ret_code = sd_ble_cfg_set(BLE_CONN_CFG_GAP, &ble_cfg, *app_ram_start);
  if (ret_code != NRF_SUCCESS)
  {
      return ret_code;
  }
  
  memset(&ble_cfg, 0, sizeof(ble_cfg));
  ble_cfg.gap_cfg.role_count_cfg.periph_role_count  = 1;
  ble_cfg.gap_cfg.role_count_cfg.central_role_count = 0;
  ble_cfg.gap_cfg.role_count_cfg.central_sec_count  = 0;           
  ret_code = sd_ble_cfg_set(BLE_GAP_CFG_ROLE_COUNT, &ble_cfg, *app_ram_start);
  if (ret_code != NRF_SUCCESS)
  {
      return ret_code;
  }
  
  memset(&ble_cfg, 0, sizeof(ble_cfg));
  ble_cfg.conn_cfg.conn_cfg_tag                 = 1;
  ble_cfg.conn_cfg.params.gatt_conn_cfg.att_mtu = 23;
  ret_code = sd_ble_cfg_set(BLE_CONN_CFG_GATT, &ble_cfg, *app_ram_start);
  if (ret_code != NRF_SUCCESS)
  {
      return ret_code;
  }
    
  memset(&ble_cfg, 0, sizeof(ble_cfg));
  ble_cfg.common_cfg.vs_uuid_cfg.vs_uuid_count = 0;
  ret_code = sd_ble_cfg_set(BLE_COMMON_CFG_VS_UUID, &ble_cfg, *app_ram_start);
  if (ret_code != NRF_SUCCESS)
  {
      return ret_code;
  }
  
  memset(&ble_cfg, 0, sizeof(ble_cfg));
  ble_gatts_cfg_attr_tab_size_t table =
  {
      .attr_tab_size = 248
  };
  ble_cfg.gatts_cfg.attr_tab_size = table;
  ret_code = sd_ble_cfg_set(BLE_GATTS_CFG_ATTR_TAB_SIZE, &ble_cfg, *app_ram_start);
  if (ret_code != NRF_SUCCESS)
  {
      return ret_code;
  }
  
  memset(&ble_cfg, 0, sizeof(ble_cfg));
  ble_gatts_cfg_service_changed_t sc =
  {
      .service_changed = 0
  };
  ble_cfg.gatts_cfg.service_changed = sc;
  ret_code = sd_ble_cfg_set(BLE_GATTS_CFG_SERVICE_CHANGED, &ble_cfg, *app_ram_start);
  if (ret_code != NRF_SUCCESS)
  {
      return ret_code;
  }
  
  ret_code = sd_ble_enable(app_ram_start);
  
//  static ble_gap_addr_t addr = {
//    1,
//    BLE_GAP_ADDR_TYPE_RANDOM_STATIC,
//    {0x02,0x03,0x04,},
//  };
//  ret_code = sd_ble_gap_addr_set(&addr);
//  if (ret_code != NRF_SUCCESS)
//  {
//      return ret_code;
//  }
  // Adverizing init
//  data[0] = 0x02;
//  data[1] = 0x01;
//  data[2] = 0x04;
//  data[3] = 0x09;
//  data[4] = 0x08;
  uint16_t test[4] = {0x00,0x00,0x00,0x00};
  advertizer_update_data(0x0000, test,4);
//  ret_code = sd_ble_gap_adv_data_set(data, 13, 0, 0);
//  if (ret_code != NRF_SUCCESS)
//  {
//      return ret_code;
//  }
  //
  memset(&m_adv_params, 0, sizeof(m_adv_params));

  m_adv_params.type        = BLE_GAP_ADV_TYPE_ADV_SCAN_IND;
  m_adv_params.p_peer_addr = 0;    // Undirected advertisement.
  m_adv_params.fp          = BLE_GAP_ADV_FP_ANY;
  m_adv_params.interval    = MSEC_TO_UNITS(100, UNIT_0_625_MS);;//50*20*8; //(100*1000/625)//MSEC_TO_UNITS(100, UNIT_1_25_MS);//
  m_adv_params.timeout     = 0;       // Never time out.
  ret_code = sd_ble_gap_adv_start(&m_adv_params, 1);
  if (ret_code != NRF_SUCCESS)
  {
      return ret_code;
  }
  return NRF_SUCCESS;
}
void advertizer_init()
{
  ble_adv_init();
  timer_id = &timer1;
//  if(NRF_SUCCESS != app_timer_create(&timer_id,APP_TIMER_MODE_REPEATED,advertizer_timer_handler))
//  {
//    while(1);
//  }
//  app_timer_start(timer_id,APP_TIMER_TICKS(100),NULL);
}

void advertizer_hadler()
{
  uint32_t ret_code = 0;
  ble_evt_t * p_ble_evt;
  static uint8_t evt_buffer[56];
  static uint16_t evt_len = 56;
  evt_len = 56;
  ret_code = sd_ble_evt_get(evt_buffer, &evt_len);
  if (ret_code != NRF_SUCCESS)
  {
      return;
  }
  p_ble_evt = (ble_evt_t *)evt_buffer;
  
  switch(p_ble_evt->header.evt_id)
  {
    case BLE_GAP_EVT_CONNECTED:
      {
        sd_ble_gap_disconnect(p_ble_evt->evt.gap_evt.conn_handle, BLE_HCI_LOCAL_HOST_TERMINATED_CONNECTION);
      }
      break;
    case BLE_GAP_EVT_DISCONNECTED:
      {
      }
      break;
  }
}

void advertizer_update_data(uint16_t id, uint16_t *_data,int _data_len)
{
  uint32_t ret_code;
  //static uint8_t data[0x23] = "     GB0001";
  data[0] = 0x02;
  data[1] = 0x01;
  data[2] = 0x04;
  data[3] = 0x07;
  data[4] = 0x08;
  //5
  //6
  //7
  //8
  //9
  //10
  data[11] = _data_len*2+2+1;
  data[12] = 0xFF;
  data[13] = (id >> 8) & 0xFF;
  data[14] = (id) & 0xFF;
  for(int i=0;i<_data_len;i++)
  {
    data[15+i*2]  = (_data[i]>>8) & 0xFF;
    data[15+i*2+1]= (_data[i])    & 0xFF;
  }
  //ret_code = sd_ble_gap_adv_stop();
  ret_code = sd_ble_gap_adv_data_set(data, 15+data[11], 0, 0);
  //ret_code = sd_ble_gap_adv_start(&m_adv_params, 1);
}
void advertizer_deinit()
{
}