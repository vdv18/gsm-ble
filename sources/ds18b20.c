#include "nrf.h"
#include "ds18b20.h"
/*
enum ds18b20_device_e {
  DS18B20_CH1,
  DS18B20_CH2,
  DS18B20_CH3,
  DS18B20_CH4,
};

void ds18b20_reset( int device);
void ds18b20_start_measure( int device );
void ds18b20_read_measure( int device, uint8_t *data );
*/

#define COUNT_US(us) (64UL*us)

#define RESET_1DEALY_US  (500)
#define RESET_2DEALY_US  (10)
#define RESET_3DEALY_US  (50)
#define RESET_4DEALY_US  (480)

#define START_COUNT()   { DWT->CYCCNT = 0;DWT->CTRL |= 0x1;}
#define CHECK_COUNT(us)  ((DWT->CYCCNT > COUNT_US(us))?1:0)
#define DEALY_US(us)    {\
        volatile uint32_t count = 0;\
        DWT->CYCCNT = 0;\
        DWT->CTRL |= 0x1;\
        while(count < COUNT_US(us)){count = DWT->CYCCNT;}\
        }
        
#define GPIO_PP(pin)\
        NRF_GPIO->PIN_CNF[pin] = 0\
          | (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)\
          | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)\
          | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)\
          | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos)\
          | (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
#define GPIO_OD(pin)\
        NRF_GPIO->PIN_CNF[pin] = 0\
          | (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)\
          | (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos)\
          | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)\
          | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos)\
          | (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);

int get_pin(int device)
{
  int pin;
  switch(device)
  {
    case DS18B20_CH3:
      {
        pin = 28;
      }
      break;
    case DS18B20_CH2:
      {
        pin = 29;
      }
      break;
    case DS18B20_CH1:
      {
        pin = 30;
      }
      break;
    case DS18B20_CH0:
      {
        pin = 31;
      }
      break;
  }
  return pin;
}
        
int ds18b20_reset( int device)
{
  volatile uint32_t count = 0;
  int pin;
  pin = get_pin(device);
  CoreDebug->DEMCR |= 0x01000000;
  if(pin){
    GPIO_PP(pin);
    NRF_GPIO->OUTSET = 1<<pin;
    DEALY_US(RESET_1DEALY_US);
    DEALY_US(RESET_1DEALY_US);
    NRF_GPIO->OUTCLR = 1<<pin;
    DEALY_US(RESET_1DEALY_US);
    NRF_GPIO->OUTSET = 1<<pin;
    DEALY_US(RESET_2DEALY_US);
    GPIO_OD(pin);
    DEALY_US(RESET_3DEALY_US);
    START_COUNT();
    while(NRF_GPIO->IN & (1<<pin) && CHECK_COUNT(RESET_4DEALY_US));
    if((NRF_GPIO->IN & (1<<pin)) == 0)
    {
      return 1;
    }
    GPIO_PP(pin);
    NRF_GPIO->OUTSET = 1<<pin;
  }
  return 0;
}
static void ds18b20_write_bit (  int device, int bit )
{
  int pin;
  CoreDebug->DEMCR |= 0x01000000;
  pin = get_pin(device);
  GPIO_PP(pin);
  NRF_GPIO->OUTSET = 1<<pin;
  DEALY_US(1);
  if(bit)
  {
    NRF_GPIO->OUTCLR = 1<<pin;
    DEALY_US(1);
    NRF_GPIO->OUTSET = 1<<pin;
  }
  else
  {
    NRF_GPIO->OUTCLR = 1<<pin;
    DEALY_US(1);
  }
  DEALY_US(60);
  NRF_GPIO->OUTSET = 1<<pin;
  GPIO_PP(pin);
}
static int ds18b20_read_bit (  int device )
{
  int ret = 0;
  int pin;
  CoreDebug->DEMCR |= 0x01000000;
  pin = get_pin(device);
  GPIO_PP(pin);
  NRF_GPIO->OUTSET = 1<<pin;
  DEALY_US(5);
  NRF_GPIO->OUTCLR = 1<<pin;
  DEALY_US(2);
  NRF_GPIO->OUTSET = 1<<pin;
  GPIO_OD(pin);
  DEALY_US(20);
  if((NRF_GPIO->IN & (1<<pin)) == 0)
  {
    ret = 0;
  }
  else
  {
    ret = 1;
  }
  DEALY_US(65);
  NRF_GPIO->OUTSET = 1<<pin;
  GPIO_PP(pin);
  return ret;
}

static void write_bytes( int device, uint8_t *data, int size)
{
  uint8_t cmd = 0x44;
  volatile uint32_t count = 0;
  int pin;
  CoreDebug->DEMCR |= 0x01000000;
  pin = get_pin(device);
  while(size--)
  {
    cmd = *(data++);
    if(pin){
      DEALY_US(100);
      for(int i=0;i<8;i++)
        ds18b20_write_bit(device,((cmd&(1<<i))>0));
    }
  }
}
static void read_bytes( int device, uint8_t *data, int size)
{
  uint8_t read = 0;
  while(size--)
  {
    DEALY_US(100);
    for(int i=0;i<8;i++)
    {
      read = read >> 1;
      if(ds18b20_read_bit(device) > 0)
        read |= 0x80;
    }
    *(data++) = read;
  }
}


void ds18b20_start_measure( int device )
{
  uint8_t cmd[2] = {0xCC,0x44};
  write_bytes(device,cmd,2);
}
static unsigned char get_crc(unsigned char *data, unsigned char count);
int ds18b20_read_measure( int device, uint8_t *data )
{
  uint8_t crc = 0;
  uint8_t cmd[2] = {0xCC,0xBE};
  write_bytes(device,cmd,2);
  read_bytes(device,data,9);
  crc = get_crc(data, 8);
  if(crc == data[8])
    return 0;
  return -1;
}
  
static unsigned char crc88540_table[256] = {
    0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
  157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
   35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
  190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
   70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
  219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
  101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
  248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
  140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
   17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
  175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
   50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
  202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
   87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
  233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
  116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53
};

unsigned char get_crc(unsigned char *data, unsigned char count)
{
    unsigned char result=0; 

    while(count--) {
      result = crc88540_table[result ^ *data++];
    }
    
    return result;
}
