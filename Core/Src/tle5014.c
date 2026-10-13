/* Read-only SSC driver, adapted from the original N10 TLE5014 driver.
 * CH1: SPI2 PB13/PB15/PB12; CH2: SPI3 PB3/PB5/PA15.
 * Mode 1, 16-bit, one bidirectional data wire. No EEPROM/calibration writes.
 */
#include "tle5014.h"
#include "cmsis_os2.h"
extern SPI_HandleTypeDef hspi2, hspi3;
volatile TLE5014_Reading tle_readings[2];
static void wait_ns(uint32_t ns)
{
  uint32_t cycles=ns*(SystemCoreClock/1000000u)/1000u+1u;
  uint32_t start=DWT->CYCCNT;
  while (DWT->CYCCNT-start<cycles) { }
}
static uint8_t crc8(const uint8_t *p, unsigned n)
{
  uint8_t c=0xff;
  while(n--) {
    c^=*p++;
    for(unsigned i=0;i<8;i++) c=(uint8_t)((c&0x80u)?(c<<1)^0x1du:c<<1);
  }
  return (uint8_t)~c;
}
void TLE5014_Init(void)
{
  CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT=0;
  DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
  HAL_GPIO_WritePin(TLE5014_CS_CH1_GPIO_Port,TLE5014_CS_CH1_Pin,GPIO_PIN_SET);
  HAL_GPIO_WritePin(TLE5014_CS_CH2_GPIO_Port,TLE5014_CS_CH2_Pin,GPIO_PIN_SET);
  for(unsigned i=0;i<2;i++) tle_readings[i].status=TLE5014_ERR_SPI;
}
/* Runtime diagnostic selection: 0=hardware /32, 1=hardware /256,
 * 2=GPIO SSC. Changes apply only between completed transactions. */
volatile uint32_t tle_transport[2];
volatile uint32_t tle_stat[2], tle_stat_safety[2], tle_stat_result[2];
static uint32_t active_transport[2]={0,0};
static void configure_transport(unsigned ch)
{
  if(active_transport[ch]==tle_transport[ch]) return;
  SPI_HandleTypeDef *spi=ch?&hspi3:&hspi2;
  __HAL_SPI_DISABLE(spi);
  GPIO_InitTypeDef g={0};
  g.Pin=ch?GPIO_PIN_3:GPIO_PIN_13;
  g.Mode=tle_transport[ch]==2?GPIO_MODE_OUTPUT_PP:GPIO_MODE_AF_PP;
  g.Speed=GPIO_SPEED_FREQ_HIGH;
  g.Alternate=ch?GPIO_AF6_SPI3:GPIO_AF5_SPI2;
  HAL_GPIO_WritePin(GPIOB,g.Pin,GPIO_PIN_RESET);
  HAL_GPIO_Init(GPIOB,&g);
  g.Pin=ch?GPIO_PIN_5:GPIO_PIN_15;
  g.Mode=tle_transport[ch]==2?GPIO_MODE_INPUT:GPIO_MODE_AF_PP;
  HAL_GPIO_Init(GPIOB,&g);
  if(tle_transport[ch]!=2) {
    spi->Init.BaudRatePrescaler=tle_transport[ch]==1?SPI_BAUDRATEPRESCALER_256:SPI_BAUDRATEPRESCALER_32;
    if(HAL_SPI_Init(spi)!=HAL_OK) Error_Handler();
  }
  active_transport[ch]=tle_transport[ch];
}
static uint16_t bb_word(unsigned ch,uint16_t tx,int output)
{
  uint32_t clk=ch?GPIO_PIN_3:GPIO_PIN_13;
  unsigned bit=ch?5u:15u;
  uint16_t rx=0;
  MODIFY_REG(GPIOB->MODER,3u<<(2u*bit),(output?1u:0u)<<(2u*bit));
  for(unsigned i=0;i<16;i++) {
    GPIOB->BSRR=clk;
    if(output) GPIOB->BSRR=(tx&0x8000u)?1u<<bit:1u<<(bit+16u);
    wait_ns(2000);
    rx=(uint16_t)((rx<<1)|((GPIOB->IDR>>bit)&1u));
    GPIOB->BSRR=clk<<16;
    wait_ns(2000);
    tx<<=1;
  }
  return rx;
}
static TLE5014_Status read_frame(unsigned ch,uint16_t command,uint16_t rx[2],uint32_t *err)
{
  SPI_HandleTypeDef *spi=ch?&hspi3:&hspi2;
  GPIO_TypeDef *port=ch?TLE5014_CS_CH2_GPIO_Port:TLE5014_CS_CH1_GPIO_Port;
  uint16_t pin=ch?TLE5014_CS_CH2_Pin:TLE5014_CS_CH1_Pin;
  HAL_GPIO_WritePin(port,pin,GPIO_PIN_RESET);
  wait_ns(200);
  HAL_StatusTypeDef hal=HAL_OK;
  if(tle_transport[ch]==2) {
    bb_word(ch,command,1);
    /* Release DATA immediately after the command, before turn-around delay. */
    unsigned bit=ch?5u:15u;
    MODIFY_REG(GPIOB->MODER,3u<<(2u*bit),0u);
    wait_ns(200);
    rx[0]=bb_word(ch,0,0); rx[1]=bb_word(ch,0,0);
    *err=0;
  } else {
    hal=HAL_SPI_Transmit(spi,(const uint8_t *)&command,1,10);
    if(hal==HAL_OK) { wait_ns(200); hal=HAL_SPI_Receive(spi,(uint8_t *)rx,2,10); }
    *err=HAL_SPI_GetError(spi);
  }
  wait_ns(200);
  HAL_GPIO_WritePin(port,pin,GPIO_PIN_SET);
  wait_ns(700);
  if(hal!=HAL_OK) return TLE5014_ERR_SPI;
  uint8_t bytes[5]={command>>8,command&0xff,rx[0]>>8,rx[0]&0xff,rx[1]>>8};
  return crc8(bytes,5)!=(uint8_t)rx[1]?TLE5014_ERR_CRC:TLE5014_OK;
}
static void read_angle(unsigned ch)
{
  configure_transport(ch);
  uint16_t rx[2]={0}; uint32_t err=0;
  TLE5014_Status status=read_frame(ch,0x8021u,rx,&err);
  if(status==TLE5014_OK && (rx[1]&0x7000u)!=0x7000u) status=TLE5014_ERR_STAT;
  tle_readings[ch].raw=rx[0]; tle_readings[ch].safety=rx[1];
  tle_readings[ch].hal_error=err;
  if(status==TLE5014_OK) {
    int32_t value=rx[0]&0x7fff;
    if(value&0x4000) value-=0x8000;
    tle_readings[ch].centideg=value*36000/32768;
    ++tle_readings[ch].good_count;
    tle_readings[ch].last_good_ms=osKernelGetTickCount();
  } else ++tle_readings[ch].error_count;
  tle_readings[ch].status=status;
  if(status!=TLE5014_OK) {
    uint16_t sr[2]={0}; uint32_t ignored;
    /* STAT read also acknowledges latched faults, as in the original project. */
    tle_stat_result[ch]=read_frame(ch,0xc001u,sr,&ignored);
    tle_stat[ch]=sr[0]; tle_stat_safety[ch]=sr[1];
  }
}
void TLE5014_Poll(void) { read_angle(0); read_angle(1); }
