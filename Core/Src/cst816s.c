/**
  ******************************************************************************
  * @file    cst816s.c
  * @brief   CST816S capacitive touch controller driver
  ******************************************************************************
  */
#include "cst816s.h"

#define REG_GESTURE   0x01u /* gesture, finger num, XY block starts here */
#define REG_CHIP_ID   0xA7u
#define REG_DIS_SLEEP 0xFEu

#define I2C_TIMEOUT_MS 10u

bool CST816S_Init(I2C_HandleTypeDef *hi2c)
{
  /* reset pulse */
  HAL_GPIO_WritePin(CST816S_RST_PORT, CST816S_RST_PIN, GPIO_PIN_RESET);
  HAL_Delay(10);
  HAL_GPIO_WritePin(CST816S_RST_PORT, CST816S_RST_PIN, GPIO_PIN_SET);
  HAL_Delay(100);

  /* keep the chip awake so taps are never missed between polls */
  uint8_t v = 0x01;
  (void)HAL_I2C_Mem_Write(hi2c, CST816S_I2C_ADDR, REG_DIS_SLEEP,
                          I2C_MEMADD_SIZE_8BIT, &v, 1, I2C_TIMEOUT_MS);

  uint8_t id = 0;
  if (HAL_I2C_Mem_Read(hi2c, CST816S_I2C_ADDR, REG_CHIP_ID,
                       I2C_MEMADD_SIZE_8BIT, &id, 1, I2C_TIMEOUT_MS) != HAL_OK)
  {
    return false;
  }
  return (id != 0x00u && id != 0xFFu);
}

bool CST816S_Read(I2C_HandleTypeDef *hi2c, CST816S_Touch *t)
{
  uint8_t d[6];

  if (HAL_I2C_Mem_Read(hi2c, CST816S_I2C_ADDR, REG_GESTURE,
                       I2C_MEMADD_SIZE_8BIT, d, sizeof(d),
                       I2C_TIMEOUT_MS) != HAL_OK)
  {
    return false;
  }
  t->gesture = d[0];
  t->fingers = d[1];
  t->x = (uint16_t)(((d[2] & 0x0Fu) << 8) | d[3]);
  t->y = (uint16_t)(((d[4] & 0x0Fu) << 8) | d[5]);
  return true;
}
