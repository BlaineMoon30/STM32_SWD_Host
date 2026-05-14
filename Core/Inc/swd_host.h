#ifndef __SWD_HOST_H
#define __SWD_HOST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef enum
{
  SWD_HOST_OK = 0,
  SWD_HOST_ERROR = 1,
  SWD_HOST_TIMEOUT = 2,
  SWD_HOST_ACK_WAIT = 3,
  SWD_HOST_ACK_FAULT = 4,
  SWD_HOST_PARITY_ERROR = 5,
  SWD_HOST_ALIGNMENT_ERROR = 6,
  SWD_HOST_TARGET_LOCKED = 7
} swd_host_status_t;

typedef enum
{
  STM32F1_RDP_LEVEL_0 = 0,
  STM32F1_RDP_LEVEL_1 = 1,
  STM32F1_RDP_LEVEL_2 = 2
} stm32f1_rdp_level_t;

typedef enum
{
  STM32L0_RDP_LEVEL_0 = 0,
  STM32L0_RDP_LEVEL_1 = 1,
  STM32L0_RDP_LEVEL_2 = 2
} stm32l0_rdp_level_t;

typedef enum
{
  SWD_HOST_CONNECT_NORMAL = 0,
  SWD_HOST_CONNECT_UNDER_RESET = 1
} swd_host_connect_mode_t;

typedef enum
{
  SWD_HOST_SPEED_VERY_LOW = 0,
  SWD_HOST_SPEED_LOW = 1,
  SWD_HOST_SPEED_MEDIUM = 2,
  SWD_HOST_SPEED_HIGH = 3,
  SWD_HOST_SPEED_VERY_HIGH = 4
} swd_host_speed_t;

typedef struct
{
  swd_host_connect_mode_t connect_mode;
  swd_host_speed_t speed;
} swd_host_config_t;

swd_host_status_t swd_host_init(void);
swd_host_status_t swd_host_init_with_config(const swd_host_config_t *config);
swd_host_status_t swd_host_configure(const swd_host_config_t *config);
void swd_host_get_config(swd_host_config_t *config);
swd_host_status_t swd_host_set_connect_mode(swd_host_connect_mode_t connect_mode);
swd_host_status_t swd_host_set_speed(swd_host_speed_t speed);
swd_host_status_t swd_host_connect(void);
void swd_host_disconnect(void);

swd_host_status_t swd_host_read_u8(uint32_t address, uint8_t *value);
swd_host_status_t swd_host_read_u16(uint32_t address, uint16_t *value);
swd_host_status_t swd_host_read_u32(uint32_t address, uint32_t *value);
swd_host_status_t swd_host_read_ap_idr(uint32_t apsel, uint32_t *value);

swd_host_status_t swd_host_write_u8(uint32_t address, uint8_t value);
swd_host_status_t swd_host_write_u16(uint32_t address, uint16_t value);
swd_host_status_t swd_host_write_u32(uint32_t address, uint32_t value);

swd_host_status_t stm32f1_read_u32(uint32_t address, uint32_t *value);
swd_host_status_t stm32f1_write_u32(uint32_t address, uint32_t value);
swd_host_status_t stm32f1_get_rdp_level(stm32f1_rdp_level_t *level);
swd_host_status_t stm32f1_set_rdp_level(stm32f1_rdp_level_t level);

swd_host_status_t stm32l0_read_u32(uint32_t address, uint32_t *value);
swd_host_status_t stm32l0_write_u32(uint32_t address, uint32_t value);
swd_host_status_t stm32l0_get_rdp_level(stm32l0_rdp_level_t *level);
swd_host_status_t stm32l0_set_rdp_level(stm32l0_rdp_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* __SWD_HOST_H */
