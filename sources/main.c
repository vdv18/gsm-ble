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

#include "pt.h"
#include "gsm.h"

#include "jsmn.h"

#include <stdlib.h>
#include "config.h"

/* Request URL */
//#define GSM_HTTP_URL    "http://stm32f4-discovery.net/gsm_example.php"
#define GSM_HTTP_URL    "http://monitorholoda.flynet.pro/json_post.php"
uint32_t get_timestamp();

jsmn_parser parser;
jsmntok_t tokens[50];

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
  
  clock.source = NRF_CLOCK_LF_SRC_SYNTH;
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
static int modem_settings_recv = 0;
static uint16_t sensors_id_seq = 0;
static uint8_t sensors_ready;
static uint16_t sensors[4];
static uint16_t sensor_raw_data[4];
static uint16_t sensor_converted_data[4];
static uint16_t int_temp = 0;
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
    case SENSOR_INT_TEMP:
      int_temp = data;
      break;
  }
}
void modem_send_json(uint8_t *data, int size);

void timer_send_data_callback( void * p_data);
void convert_sensors_data_from_raw(enum sensors_index_e sensor, uint16_t *raw, uint16_t *converted);


#define JSON_DATA_SIZE 1024*6
//#define JSON_DATA_SIZE 4096*2
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
  size += prepare_value_int(&data[size], "timestamp", get_timestamp());
  size += prepare_comma(&data[size]);

  size += sprintf(&data[size],"\"sensors\":[{");
  size += prepare_value_str(&data[size], "type", "T");
  size += prepare_comma(&data[size]);
  size += prepare_value_str(&data[size], "mode", "C");
  size += prepare_comma(&data[size]);
  size += prepare_value_int(&data[size], "timestamp", get_timestamp());
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
    size += prepare_value_int(&data[size], "DATA",(int16_t)sensor_converted_data[j]);
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
  size += prepare_value_int(&data[size], "timestamp", get_timestamp());
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

void convert_sensors_data_from_raw(enum sensors_index_e sensor, uint16_t *raw, uint16_t *converted)
{
  // read sample
  uint16_t data = *raw;
  
  // process conversion
  float temp = 0.0;
  temp =  0.000005*data*data - 0.0402*data + 51.514; //converting formula
  
  // write sample
  *converted = (uint16_t)((int16_t)temp);
}


/* GSM working structure and result enumeration */
static gvol GSM_t GSM;
volatile GSM_t *pgsm_value = (GSM_t *)&GSM;
GSM_Result_t gsmRes;

/* SMS read structure */
GSM_SMS_Entry_t SMS_Entry;
GSM_SMS_Entry_t SMS_Send;

/* Pointer to SMS info */
GSM_SmsInfo_t* SMS_Info = NULL;


/* GSM callback declaration */
int GSM_Callback(GSM_Event_t evt, GSM_EventParams_t* params);

/* Protothread */
struct pt PT,MODEM,MODEMH;
/* SMS send protothread system */
static
PT_THREAD(SMS_PTHREAD(struct pt* pt)) {
    PT_BEGIN(pt);                                           /* Begin with protothread */
    
    if (!SMS_Info) {                                        /* Check valid SMS received info structure */
        PT_EXIT(pt);
    }
    PT_WAIT_UNTIL(pt, GSM_IsReady(&GSM) == gsmOK);          /* Wait stack to be ready */
    
    /* Try to read, should be non-blocking in non-RTOS environment */
    if ((gsmRes = GSM_SMS_Read(&GSM, SMS_Info->Position, &SMS_Entry, 0)) == gsmOK) {
        //printf("SMS reading has begun! Waiting for response!\r\n");
        
        PT_WAIT_UNTIL(pt, GSM_IsReady(&GSM) == gsmOK);      /* Wait stack to be ready */
        gsmRes = GSM_GetLastReturnStatus(&GSM);             /* Get response status from non-blocking call */
        
        if (gsmRes == gsmOK) {                              /* Check if SMS was actually full read */
            printf("SMS read!\r\n");
            
            /* Make actions according to received SMS string */
            if (strcmp(SMS_Entry.Data, "LED ON") == 0) {
                led_set(LED_2,LED_MODE_ON);
                gsmRes = GSM_SMS_Send(&GSM, SMS_Entry.Number, "OK", 0);
            } else if (strcmp(SMS_Entry.Data, "LED OFF") == 0) {
                led_set(LED_2,LED_MODE_OFF);
                gsmRes = GSM_SMS_Send(&GSM, SMS_Entry.Number, "OK", 0);
            } else if (strcmp(SMS_Entry.Data, "LED TOGGLE") == 0) {
                gsmRes = GSM_SMS_Send(&GSM, SMS_Entry.Number, "OK", 0);
            } else {
                gsmRes = GSM_SMS_Send(&GSM, SMS_Entry.Number, "ERROR", 0);
            }
            
            /* Send it back to user */
            if (gsmRes == gsmOK) {
                printf("Send back started ok!\r\n");
                
                /* Wait till SMS full sent! */
                PT_WAIT_UNTIL(pt, GSM_IsReady(&GSM) == gsmOK);  /* Wait stack to be ready */
                gsmRes = GSM_GetLastReturnStatus(&GSM);     /* Check actual send status from non-blocking call */
                
                if (gsmRes == gsmOK) {
                    printf("SMS has been successfully sent!\r\n");
                } else {
                    printf("Error trying to send SMS: %d\r\n", gsmRes);
                }
            } else {
                printf("Send back start error: %d\r\n", gsmRes);
            }
        }
    } else {
        printf("SMS reading process failed: %d\r\n", gsmRes);
    }
    
    GSM_SMS_ClearReceivedInfo(&GSM, SMS_Info, 0);           /* Check SMS information slot */
    PT_WAIT_UNTIL(pt, GSM_IsReady(&GSM) == gsmOK);          /* Wait stack to be ready */
    
    SMS_Info = NULL;                                        /* Reset received SMS structure */
    PT_END(pt);                                             /* End protothread */
}


#define POWER_KEY_ON()  {NRF_P0->OUTSET = 1<<5;}
#define POWER_KEY_OFF() {NRF_P0->OUTCLR = 1<<5;}
#define IS_GSM_ON()     ((NRF_P0->OUT & 1<<6) > 0)
#define GSM_ON()        {NRF_P0->OUTCLR = 1<<6;}
#define GSM_OFF()       {NRF_P0->OUTSET = 1<<6;}
#define GSM_STATUS()    ((NRF_P0->IN & 1<<4) > 0)

static int modem_initialization = 0;
static int modem_power_on = 0;
#define TIME_DELAY(TIME)        time = get_timestamp();\
                                PT_WAIT_UNTIL(pt, (get_timestamp() - time > TIME));
#define TIME_TIMEOUT(TIME,PARAM)        time = get_timestamp();\
                                PT_WAIT_UNTIL(pt, (get_timestamp() - time > TIME) || (PARAM));
PT_THREAD(MODEM_INIT(struct pt *pt))
{
  static uint32_t time = 0;
  static int toggle = 0;
  PT_BEGIN(pt);

  NRF_P0->PIN_CNF[4] = ((uint32_t)GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_INPUT_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_PULL_Pos)
                             | ((uint32_t)GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_SENSE_Pos);
  NRF_P0->PIN_CNF[5] = ((uint32_t)GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_INPUT_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_PULL_Pos)
                             | ((uint32_t)GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_SENSE_Pos);
  NRF_P0->PIN_CNF[6] = ((uint32_t)GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_INPUT_Pos)
                             | ((uint32_t)GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos)
                             | ((uint32_t)GPIO_PIN_CNF_DRIVE_H0D1 << GPIO_PIN_CNF_DRIVE_Pos)
                             | ((uint32_t)0 << GPIO_PIN_CNF_SENSE_Pos);
  
  modem_initialization = 1;
  while(1) {
    if(modem_initialization)
    {
      modem_power_on = 0;
      modem_initialization = 0;
      POWER_KEY_OFF();
      GSM_OFF();
      TIME_DELAY(1000);
      GSM_ON();
      TIME_DELAY(1000);
      POWER_KEY_ON();
      TIME_DELAY(1000);
      POWER_KEY_OFF();
      TIME_DELAY(1000);
      TIME_TIMEOUT(5000, GSM_STATUS());
      if(! (GSM_STATUS()) )
        modem_initialization = 1;
      else
      {
        GSM_Init(&GSM, GSM_PIN, 9600, GSM_Callback);
        modem_power_on = 1;
      }
    }
    TIME_DELAY(1000);
  }
  PT_END(pt);
}
uint8_t send[] = "Hello from GSM module! The same as I sent you I just get back!";
uint8_t receive[1000];
uint32_t br;
PT_THREAD(MODEM_HANDLER(struct pt *pt))
{
  static uint32_t time = 0;
  int size;
  PT_BEGIN(pt);

  while(1) {
    time = get_timestamp();
    if(modem_power_on)
    {
      GSM_ProcessCallbacks(&GSM);
       /* Try to connect to network */
      gsmRes = GSM_GPRS_Attach(&GSM, GSM_APN, GSM_APN_USER, GSM_APN_PASS, 1);
      
      PT_WAIT_UNTIL(pt, GSM_IsReady(&GSM) == gsmOK);  /* Wait stack to be ready */
      gsmRes = GSM_GetLastReturnStatus(&GSM);     /* Check actual send status from non-blocking call */
      if(gsmRes != gsmOK) 
      {
        modem_initialization = 1;
        modem_power_on = 0;
        goto __modem_handler_end;
      }
      
      gsmRes = GSM_HTTP_Begin(&GSM, 1);
      
      PT_WAIT_UNTIL(pt, GSM_IsReady(&GSM) == gsmOK);  /* Wait stack to be ready */
      gsmRes = GSM_GetLastReturnStatus(&GSM);     /* Check actual send status from non-blocking call */
      if(gsmRes != gsmOK) 
      {
        modem_initialization = 1;
        modem_power_on = 0;
        goto __modem_handler_end;
      }
      
      size = prepare_json(data_json,JSON_DATA_SIZE);
      
      gsmRes = GSM_HTTP_SetData(&GSM, data_json, size, 1);
      
      PT_WAIT_UNTIL(pt, GSM_IsReady(&GSM) == gsmOK);  /* Wait stack to be ready */
      gsmRes = GSM_GetLastReturnStatus(&GSM);     /* Check actual send status from non-blocking call */
      if(gsmRes != gsmOK) 
      {
        modem_initialization = 1;
        modem_power_on = 0;
        goto __modem_handler_end;
      }
      
      gsmRes = GSM_HTTP_Execute(&GSM, GSM_HTTP_URL, GSM_HTTP_Method_POST, GSM_HTTP_SSL_Disable, 1);
      
      PT_WAIT_UNTIL(pt, GSM_IsReady(&GSM) == gsmOK);  /* Wait stack to be ready */
      gsmRes = GSM_GetLastReturnStatus(&GSM);     /* Check actual send status from non-blocking call */
      if(gsmRes != gsmOK) 
      {
        modem_initialization = 1;
        modem_power_on = 0;
        goto __modem_handler_end;
      }
      
      while (GSM_HTTP_DataAvailable(&GSM, 1))
      {
        gsmRes = GSM_HTTP_Read(&GSM, receive, sizeof(receive), &br, 1);
        PT_WAIT_UNTIL(pt, GSM_IsReady(&GSM) == gsmOK);  /* Wait stack to be ready */
        gsmRes = GSM_GetLastReturnStatus(&GSM);     /* Check actual send status from non-blocking call */
        if(gsmRes != gsmOK) 
        {
          modem_initialization = 1;
          modem_power_on = 0;
          goto __modem_handler_end;
        }
      }
      if(1){
        char obj[50];
        int obj_cnt = 0;
        jsmn_init(&parser);
        obj_cnt = jsmn_parse(&parser, receive, sizeof(receive), tokens, sizeof(tokens)/sizeof(jsmntok_t));
        if(obj_cnt > 6)
        if(tokens[0].type == JSMN_OBJECT)
        {
          if(tokens[1].type == JSMN_STRING && tokens[1].size == 1 && (strncmp(&receive[tokens[1].start],"cmd",3) == 0))
          {
            if(tokens[2].type == JSMN_STRING)
            {
               if((strncmp(&receive[tokens[2].start],"sms",3) == 0))
              {
                if((strncmp(&receive[tokens[3].start],"param",5) == 0))
                if(tokens[4].type == JSMN_ARRAY && tokens[4].size == 2 && tokens[5].type == JSMN_STRING &&  tokens[6].type == JSMN_STRING) 
                {
                  char gsm_number[0x10];
                  char gsm_message[160];
                  char *start;
                  int size;
                  start = &receive[tokens[5].start];
                  size = tokens[5].end - tokens[5].start;
                  if(size <= 0x10)
                  {
                    memcpy(gsm_number, start, size);
                    gsm_number[size] = 0;
                    
                    start = &receive[tokens[6].start];
                    size = tokens[6].end - tokens[6].start;
                    memcpy(gsm_message, start, size);
                    gsm_message[size] = 0;
                    if(size<=160)
                    {
                      gsmRes = GSM_SMS_Send(&GSM, gsm_number, gsm_message, 1);
                      /* Wait till SMS full sent! */
                      PT_WAIT_UNTIL(pt, GSM_IsReady(&GSM) == gsmOK);  /* Wait stack to be ready */
                      gsmRes = GSM_GetLastReturnStatus(&GSM);     /* Check actual send status from non-blocking call */
                      (void)gsmRes;  
                    }
                  }
                }
              }
              else if(tokens[2].type == JSMN_STRING && (strncmp(&receive[tokens[1].start],"send",4) == 0))
              {
              }
            }
          }
        }
        memset(receive,0,sizeof(receive));
      }
__modem_handler_end:
    }
    PT_WAIT_UNTIL(pt, ((get_timestamp() - time) > 20*1000));
  }
  PT_END(pt);
}

static int sms_clean_all = 0;
void main()
{
  sd_init();
  if(app_timer_init() != NRF_SUCCESS)
  {
    while(1);
  }
  led_init();
  SysTick_Config(64000);
  /* Proto thread initialization */
  PT_INIT(&PT);                                           /* Initialize SMS protothread */
  while(1){
    //if(modem_power_on)
    MODEM_INIT(&MODEM);
    MODEM_HANDLER(&MODEMH);
    if(modem_power_on)
    {
      static int temp = 0;      
      GSM_Update(&GSM);                                   /* Process GSM update */
      if(temp == 0)
      {
        temp =1;
        //GSM_SMS_Send(&GSM, "+79528098985", "OK", 0);
      }
      if (SMS_Info) {                                     /* Anything works? */
          SMS_PTHREAD(&PT);                               /* Call protothread function */
      } else {
          /* Check if any SMS received */
          if ((SMS_Info = GSM_SMS_GetReceivedInfo(&GSM, 1)) != NULL) {
              printf("SMS Received, start SMS processing\r\n");
          }
      }
    }
    /* If button is pressed */
    if (sms_clean_all) {
      sms_clean_all=0;
        /* Delete all SMS messages, process with blocking call */
        if ((gsmRes = GSM_SMS_MassDelete(&GSM, GSM_SMS_MassDelete_All, 1)) == gsmOK) {
            printf("SMS MASS DELETE OK\r\n");
        } else {
            printf("Error trying to mass delete: %d\r\n", gsmRes);
        }
    }
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

static int timestamp = 0;
uint32_t get_timestamp()
{
  return timestamp;
}

void SysTick_Handler(void)
{
    timestamp++;
    GSM_UpdateTime(&GSM, 1);                                /* Update GSM library time for 1 ms */
    if(modem_power_on)
    {
    }
}
int GSM_Callback(GSM_Event_t evt, GSM_EventParams_t* params) {
    switch (evt) {                              /* Check events */
        case gsmEventIdle:
            break;
        case gsmEventSMSCMTI:                   /* Information about new SMS received */
            break;
        default:
            break;
    }
    
    return 0;
}