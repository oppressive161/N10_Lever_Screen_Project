#ifndef CST816S_H
#define CST816S_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#define CST816S_I2C_ADDR  (0x15u << 1)

#define CST816S_RST_PORT  TP_RST_GPIO_Port
#define CST816S_RST_PIN   TP_RST_Pin
#define CST816S_INT_PORT  TP_INT_GPIO_Port
#define CST816S_INT_PIN   TP_INT_Pin

/* Gesture IDs (register 0x01) */
#define CST816S_GES_NONE        0x00u
#define CST816S_GES_SWIPE_UP    0x01u
#define CST816S_GES_SWIPE_DOWN  0x02u
#define CST816S_GES_SWIPE_LEFT  0x03u
#define CST816S_GES_SWIPE_RIGHT 0x04u
#define CST816S_GES_CLICK       0x05u
#define CST816S_GES_DOUBLE      0x0Bu
#define CST816S_GES_LONG        0x0Cu

typedef struct
{
  uint8_t gesture;   /* CST816S_GES_* */
  uint8_t fingers;   /* 0 or 1 */
  uint16_t x;        /* 0..239 */
  uint16_t y;        /* 0..239 */
} CST816S_Touch;

/* Resets the chip; GPIOs are configured by CubeMX for the U585 board.
 * Returns true if the chip answers on I2C. */
bool CST816S_Init(I2C_HandleTypeDef *hi2c);

/* Reads the current touch/gesture registers. */
bool CST816S_Read(I2C_HandleTypeDef *hi2c, CST816S_Touch *t);

#ifdef __cplusplus
}
#endif

#endif /* CST816S_H */
