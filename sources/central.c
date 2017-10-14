#include "central.h"
#include "nrf.h"
#include "ble.h"
#include "nrf_sdm.h"
#include "led.h"
#include "app_util.h"
#include "app_timer.h"

mac_data_t central_mac_data_list[MAC_DATA_LIST_SIZE];
int central_mac_data_size = 0;

static app_timer_t timer;
static app_timer_id_t timer_id = &timer;
#define MIN_CONNECTION_INTERVAL   (7.5*1000/1250)      /**< Determines minimum connection interval in milliseconds. */
#define MAX_CONNECTION_INTERVAL   (30*1000/1250)       /**< Determines maximum connection interval in milliseconds. */
#define SLAVE_LATENCY             0                                     /**< Determines slave latency in terms of connection events. */
#define SUPERVISION_TIMEOUT       (4000*1000/10000)       /**< Determines supervision time-out in units of 10 milliseconds. */

/**@brief Connection parameters requested for connection. */
static ble_gap_conn_params_t const m_connection_param =
{
    (uint16_t)MIN_CONNECTION_INTERVAL,
    (uint16_t)MAX_CONNECTION_INTERVAL,
    (uint16_t)SLAVE_LATENCY,
    (uint16_t)SUPERVISION_TIMEOUT
};


#define SCAN_INTERVAL             0x00A0                                /**< Determines scan interval in units of 0.625 millisecond. */
#define SCAN_WINDOW               0x0050                                /**< Determines scan window in units of 0.625 millisecond. */
#define SCAN_TIMEOUT              0x0000    
static ble_gap_scan_params_t const m_scan_params =
{
    .active   = 0,
    .interval = SCAN_INTERVAL,
    .window   = SCAN_WINDOW,
    .timeout  = SCAN_TIMEOUT,
    #if (NRF_SD_BLE_API_VERSION <= 2)
        .selective   = 0,
        .p_whitelist = NULL,
    #endif
    #if (NRF_SD_BLE_API_VERSION >= 3)
        .use_whitelist  = 0,
        .adv_dir_report = 0,
    #endif
};


static void assertion_handler(uint32_t id, uint32_t pc, uint32_t info)
{
  while(1){
    NVIC_SystemReset();
  };
}

static uint32_t ble_central_init()
{
  uint32_t ret_code;
  nrf_clock_lf_cfg_t clock;
  ble_cfg_t ble_cfg;
  uint32_t *app_ram_start = (uint32_t*)0x2000b668;
  

  memset(&ble_cfg, 0, sizeof(ble_cfg));
  ble_cfg.conn_cfg.conn_cfg_tag = 1;
  ble_cfg.conn_cfg.params.gap_conn_cfg.conn_count = 8;
  ble_cfg.conn_cfg.params.gap_conn_cfg.event_length = 3;
  
  ret_code = sd_ble_cfg_set(BLE_CONN_CFG_GAP, &ble_cfg, *app_ram_start);
  if (ret_code != NRF_SUCCESS)
  {
      return ret_code;
  }
  
  memset(&ble_cfg, 0, sizeof(ble_cfg));
  ble_cfg.gap_cfg.role_count_cfg.periph_role_count  = 0;
  ble_cfg.gap_cfg.role_count_cfg.central_role_count = 8;
  ble_cfg.gap_cfg.role_count_cfg.central_sec_count  = 1;           
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
  ble_cfg.common_cfg.vs_uuid_cfg.vs_uuid_count = 1;
  ret_code = sd_ble_cfg_set(BLE_COMMON_CFG_VS_UUID, &ble_cfg, *app_ram_start);
  if (ret_code != NRF_SUCCESS)
  {
      return ret_code;
  }
  
  memset(&ble_cfg, 0, sizeof(ble_cfg));
  ble_gatts_cfg_attr_tab_size_t table =
  {
      .attr_tab_size = 1408
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
  if (ret_code != NRF_SUCCESS)
  {
      return ret_code;
  }  
  ret_code = sd_ble_gap_scan_start(&m_scan_params);
  if (ret_code != NRF_SUCCESS)
  {
      return ret_code;
  }  
}


/**
 * @brief Parses advertisement data, providing length and location of the field in case
 *        matching data is found.
 *
 * @param[in]  type       Type of data to be looked for in advertisement data.
 * @param[in]  p_advdata  Advertisement report length and pointer to report.
 * @param[out] p_typedata If data type requested is found in the data report, type data length and
 *                        pointer to data will be populated here.
 *
 * @retval NRF_SUCCESS if the data type is found in the report.
 * @retval NRF_ERROR_NOT_FOUND if the data type could not be found.
 */
static uint32_t adv_report_parse(uint8_t type, uint8_array_t * p_advdata, uint8_array_t * p_typedata)
{
    uint32_t  index = 0;
    uint8_t * p_data;

    p_data = p_advdata->p_data;

    while (index < p_advdata->size)
    {
        uint8_t field_length = p_data[index];
        uint8_t field_type   = p_data[index + 1];

        if (field_type == type)
        {
            p_typedata->p_data = &p_data[index + 2];
            p_typedata->size   = field_length - 1;
            return NRF_SUCCESS;
        }
        index += field_length + 1;
    }
    return NRF_ERROR_NOT_FOUND;
}

static uint32_t adv_report_count = 0;
static uint32_t connect = 1;
static uint32_t connected = 0;
static uint32_t conn_cnt = 0;
static uint32_t disconn_cnt = 0; 
static uint8_array_t adv_data;
static uint8_array_t adv_type;

static void central_timer_led_off(void * p_context)
{
  led_set(LED_1,LED_MODE_OFF);
}
static void ble_handler()
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
    case BLE_GAP_EVT_ADV_REPORT:
      {
        uint8_t conn_cfg_tag;
        ble_gap_evt_t  const * p_gap_evt  = &p_ble_evt->evt.gap_evt;
        ble_gap_addr_t const * p_peer_addr  = &p_gap_evt->params.adv_report.peer_addr;
        adv_data.p_data = (uint8_t *)p_gap_evt->params.adv_report.data;
        adv_data.size   = p_gap_evt->params.adv_report.dlen;
        if((adv_report_parse(BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME,&adv_data,&adv_type) == NRF_SUCCESS) 
           ||
           (adv_report_parse(BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME,&adv_data,&adv_type) == NRF_SUCCESS))
        {
          if(memcmp(adv_type.p_data,"GB00",4) == 0)
          if(adv_type.p_data[4] >= '0' && adv_type.p_data[4] <= '9')
          if(adv_type.p_data[5] >= '0' && adv_type.p_data[5] <= '9')
          {
            int found = 0;
            mac_data_t *item = 0;
            adv_report_count++;
            for(int i=0;i<central_mac_data_size;i++)
            {
              if(memcmp(p_peer_addr->addr,central_mac_data_list[i].mac,6) == 0)
              {
                found = 1;
                item = &central_mac_data_list[i];
              }
            }
            if(!item)
            {
              if(central_mac_data_size<MAC_DATA_LIST_SIZE)
              {
                item = &central_mac_data_list[central_mac_data_size++];
                memcpy(item->mac,p_peer_addr->addr,6);
              }
            }
            if(item)
            {
              item->timestamp++;
              item->id = (uint16_t)adv_data.p_data[13]<<8 | (uint16_t)adv_data.p_data[14];
              item->data[0] = (uint16_t)adv_data.p_data[15]<<8 | (uint16_t)adv_data.p_data[16];
              item->data[1] = (uint16_t)adv_data.p_data[17]<<8 | (uint16_t)adv_data.p_data[18];
              item->data[2] = (uint16_t)adv_data.p_data[19]<<8 | (uint16_t)adv_data.p_data[20];
              item->data[3] = (uint16_t)adv_data.p_data[21]<<8 | (uint16_t)adv_data.p_data[22];
              memcpy(item->name,adv_type.p_data,adv_type.size);
              led_set(LED_1,LED_MODE_ON);
              app_timer_start(timer_id,APP_TIMER_TICKS(10),NULL);
            }
          }
        }
      }
      break;
  }
  
}

void central_init()
{
  if(NRF_SUCCESS != app_timer_create(&timer_id,APP_TIMER_MODE_REPEATED,central_timer_led_off))
  {
    while(1);
  }
  
  ble_central_init();
}
void central_handler()
{
  ble_handler();
}

void central_deinit()
{
}