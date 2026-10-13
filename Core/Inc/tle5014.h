#ifndef TLE5014_H
#define TLE5014_H
#include "main.h"
typedef enum { TLE5014_OK=0, TLE5014_ERR_SPI, TLE5014_ERR_CRC, TLE5014_ERR_STAT } TLE5014_Status;
/* Fixed-width fields simplify debugger inspection. Angle is valid only if status=OK. */
typedef struct {
  uint32_t status;
  int32_t centideg;
  uint32_t raw, safety, good_count, error_count, hal_error, last_good_ms;
} TLE5014_Reading;
extern volatile TLE5014_Reading tle_readings[2];
void TLE5014_Init(void);
void TLE5014_Poll(void);
#endif
