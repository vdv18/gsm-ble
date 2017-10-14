#include "nrf.h"
#include "ble.h"
#include "nrf_sdm.h"
#include "sensors.h"
#include "modem.h"
#include "led.h"
#include "charger.h"
#include "app_timer.h"
#include "central.h"
#include "advertizer.h"

#include <stdlib.h>

static int central = 0; // 1=central; 0=advertizer

static app_timer_t timer_send_data;
static app_timer_id_t timer_send_data_id = &timer_send_data;

void assertion_handler(uint32_t id, uint32_t pc, uint32_t info)
{
  while(1){
    NVIC_SystemReset();
  };
}
uint32_t sd_init()
{  
  uint32_t ret_code;
  nrf_clock_lf_cfg_t clock;
  
  clock.source = NRF_CLOCK_LF_SRC_XTAL;
  clock.rc_ctiv = 0;
  clock.rc_temp_ctiv = 0;
  clock.accuracy = NRF_CLOCK_LF_ACCURACY_20_PPM;
  ret_code = sd_softdevice_enable(&clock, assertion_handler);
  if (ret_code != NRF_SUCCESS)
  {
      return ret_code;
  }
  NVIC_EnableIRQ(SWI2_EGU2_IRQn);
  
  return NRF_SUCCESS;
}
void SWI2_EGU2_IRQHandler()
{
  if(central)
    central_handler();
  else
    advertizer_hadler();
}
static void power_manage(void)
{
//    ret_code_t err_code = sd_app_evt_wait();
//    APP_ERROR_CHECK(err_code);
}
static int modem_complete = 0;
static int power_off = 0;
static int modem_ready = 0;
static uint16_t sensors_id_seq = 0;
static uint8_t sensors_ready;
static uint16_t sensors[4];
static uint16_t sensor_raw_data[4];
static uint16_t sensor_converted_data[4];
void sensors_handler( enum sensors_index_e sensor, uint16_t data )
{
  switch(sensor)
  {
    
    // RAW data
    case SENSOR_ADC_1:
      sensor_raw_data[0] = data &0xFFFF;
      break;
    case SENSOR_ADC_2:
      sensor_raw_data[1] = data &0xFFFF;
      break;
    case SENSOR_ADC_3:
      sensor_raw_data[2] = data &0xFFFF;
      break;
    case SENSOR_ADC_4:
      sensor_raw_data[3] = data &0xFFFF;
      break;
      
    // Converted data
    case SENSOR_CONVERTED_1:
      sensor_converted_data[0] = data &0xFFFF;
      break;
    case SENSOR_CONVERTED_2:
      sensor_converted_data[1] = data &0xFFFF;
      break;
    case SENSOR_CONVERTED_3:
      sensor_converted_data[2] = data &0xFFFF;
      break;
    case SENSOR_CONVERTED_4:
      sensor_converted_data[3] = data &0xFFFF;
      sensors_ready++;
      sensors_id_seq++;
      break;
  }
}
void modem_send_json(uint8_t *data, int size);

void timer_send_data_callback( void * p_data);

void modem_handler(modem_state_t state)
{
  switch(state)
  {
    case MODEM_INITIALIZED:
      modem_complete = 1;
      if(NRF_SUCCESS != app_timer_create(&timer_send_data_id,APP_TIMER_MODE_REPEATED,timer_send_data_callback))
      {
        while(1);
      }
      app_timer_start(timer_send_data_id,APP_TIMER_TICKS(10000),NULL);
      break;
    case MODEM_INITIALIZING:
      central = 1;
      central_init();
      sensors_init(sensors_handler);
      break;
    case MODEM_DISABLED:
      central = 0;
      advertizer_init();
      sensors_init(sensors_handler);
      break;
    case MODEM_PROCESSING:
      modem_ready = 0;
      break;
    case MODEM_READY:
      modem_ready = 1;
      break;
    case MODEM_SEND_COMPLETE:
      modem_complete = 1;
      break;
      
  }
}

#define JSON_DATA_SIZE 4096*2
static uint8_t data_json[JSON_DATA_SIZE];
int prepare_value_int(uint8_t *data, char const *var, int value)
{
  return sprintf(data,"\"%s\":\"%d\"",var,value);
}
int prepare_value_int_hex(uint8_t *data, char const *var, int value)
{
  return sprintf(data,"\"%s\":\"%04X\"",var,value);
}
int prepare_value_mac(uint8_t *data, char const *var, char *value)
{
  return sprintf(data,"\"%s\":\"%X%X%X%X%X%X\"",var,(int)value[0],(int)value[1],(int)value[2],
                 (int)value[3],(int)value[4],(int)value[5]);
}
int prepare_value_str(uint8_t *data, char const *var, char const *value)
{
  return sprintf(data,"\"%s\":\"%s\"",var,value);
}
int prepare_comma(uint8_t *data)
{
  return sprintf(data,",");
}
int prepare_data_array(uint8_t *data)
{
  int size = 0;
  size += sprintf(&data[size],"\"data\":[");
  
extern mac_data_t central_mac_data_list[];
extern int central_mac_data_size;
  
  size += sprintf(&data[size],"{");
  size += prepare_value_str(&data[size], "type", "Sensor");
  size += prepare_comma(&data[size]);
  size += prepare_value_str(&data[size], "name", "GSM_BLE");
  size += prepare_comma(&data[size]);
  size += prepare_value_mac(&data[size], "ID", (char*)(NRF_FICR->DEVICEADDR));
  size += prepare_comma(&data[size]);
  size += prepare_value_int(&data[size], "timestamp", 0);
  size += prepare_comma(&data[size]);

  size += sprintf(&data[size],"\"sensors\":[{");
  size += prepare_value_str(&data[size], "type", "T");
  size += prepare_comma(&data[size]);
  size += prepare_value_str(&data[size], "mode", "C");
  size += prepare_comma(&data[size]);
  size += prepare_value_int(&data[size], "timestamp", 0);
  size += prepare_comma(&data[size]);
  size += sprintf(&data[size],"\"data\":[");
  for(int j=0;j<4;j++)
  {
    char name[5] = "CH01";
    size += sprintf(&data[size],"{");
    name[3] = '1'+j;
    //size += prepare_value_str(&data[size], "CID", name);
    //size += prepare_comma(&data[size]);
    size += prepare_value_str(&data[size], "NAME",name);
    size += prepare_comma(&data[size]);
    size += prepare_value_int(&data[size], "DATA",(int)sensor_converted_data[j]);
    size += sprintf(&data[size],"}");
    if(j < 4-1)
    {
      size += prepare_comma(&data[size]);
    }
  }
  size += sprintf(&data[size],"]}]}");
  if(central_mac_data_size) {
    
    for(int i=0;i<central_mac_data_size;i++)
    {
      size += prepare_comma(&data[size]);
      size += sprintf(&data[size],"{");
      size += prepare_value_str(&data[size], "type", "Sensor");
      size += prepare_comma(&data[size]);
      size += prepare_value_str(&data[size], "name", central_mac_data_list[i].name);
      size += prepare_comma(&data[size]);
      size += prepare_value_mac(&data[size], "ID", central_mac_data_list[i].mac);
      size += prepare_comma(&data[size]);
      size += prepare_value_int(&data[size], "timestamp", central_mac_data_list[i].timestamp);
      size += prepare_comma(&data[size]);
      
      size += sprintf(&data[size],"\"sensors\":[{");
      
      size += prepare_value_str(&data[size], "type", "T");
      size += prepare_comma(&data[size]);
      size += prepare_value_str(&data[size], "mode", "C");
      size += prepare_comma(&data[size]);
      size += prepare_value_int(&data[size], "timestamp", central_mac_data_list[i].timestamp);
      size += prepare_comma(&data[size]);
      size += sprintf(&data[size],"\"data\":[");
      for(int j=0;j<4;j++)
      {
        char name[5] = "CH01";
        size += sprintf(&data[size],"{");
        name[3] = '1'+j;
        //size += prepare_value_str(&data[size], "CID", name);
        //size += prepare_comma(&data[size]);
        size += prepare_value_str(&data[size], "NAME",name);
        size += prepare_comma(&data[size]);
        size += prepare_value_int(&data[size], "DATA",(int)central_mac_data_list[i].data[j]);
        size += sprintf(&data[size],"}");
        if(j < 4-1)
        {
          size += prepare_comma(&data[size]);
        }
      }
      size += sprintf(&data[size],"]}]}");
    }
  }
  
  size += sprintf(&data[size],"]");
  return size;
}
int prepare_json(uint8_t *data, int max_data)
{
  int index = 0;
  uint8_t mac[6] = {0,};
  int size = 0;
  memset(data,0,max_data);
  
  mac[0] = 
  
  size += sprintf(&data[size],"{");
  
  size += prepare_value_str(&data[size], "type", "Master");
  size += prepare_comma(&data[size]);
  size += prepare_value_str(&data[size], "name", "GSM_BLE");
  size += prepare_comma(&data[size]);
  size += prepare_value_mac(&data[size], "ID", (char*)(NRF_FICR->DEVICEADDR));
  size += prepare_comma(&data[size]);
  size += prepare_value_int(&data[size], "timestamp", index++);
  size += prepare_comma(&data[size]);
  // prepare array of data
  size += prepare_data_array(&data[size]);
  
  size += sprintf(&data[size],"}");
}
void timer_send_data_callback( void * p_data)
{
  uint8_t *data = data_json;
  int size = 0;
  if(modem_ready)
  {
    if(data && modem_complete)
    {
      modem_complete = 0;
      size = prepare_json(data,JSON_DATA_SIZE);
      if(size)
        modem_send_json(data,size);
    }
  }
}

void convert_sensors_data_from_raw(uint16_t *raw, uint16_t *converted)
{
  // read sample
  uint16_t data = *raw;
  
  // process conversion
  
  // write sample
  *converted = data;
}

void main()
{
  sd_init();
  if(app_timer_init() != NRF_SUCCESS)
  {
    while(1);
  }
  led_init();
  modem_init(modem_handler);
  while(1){
    if(sensors_ready)
    {
      sensors_ready = 0;
      if(!central){
        advertizer_update_data(sensors_id_seq++,&sensor_converted_data[0],4);
      }
    }
    if(CHARGER_STATE_YES == charger_state())
    {
      led_set(LED_3,LED_MODE_ON);
    }
    if (power_off)
    {
        power_manage();
    }
  };
}