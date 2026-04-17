#include "swd_host.h"

#define SWD_TURNAROUND_CYCLES                1U
#define SWD_LINE_RESET_CYCLES                60U
#define SWD_ACK_OK                           0x1U
#define SWD_ACK_WAIT                         0x2U
#define SWD_ACK_FAULT                        0x4U
#define SWD_MAX_RETRIES                      100U
#define SWD_RESET_HOLD_DELAY_MS              20U
#define SWD_RESET_RELEASE_DELAY_MS           100U
#define SWD_AP_READY_DELAY_MS                1U
#define SWD_CONNECT_SEQUENCE_REPEATS         2U
#define SWD_CONNECT_IDLE_LOW_CYCLES          8U
#define SWD_TRANSACTION_IDLE_CYCLES          8U
#define SWD_WAIT_RETRY_DELAY_MS              1U

#define DP_REG_ABORT                         0x00U
#define DP_REG_IDCODE                        0x00U
#define DP_REG_CTRL_STAT                     0x04U
#define DP_REG_SELECT                        0x08U
#define DP_REG_RDBUFF                        0x0CU

#define DP_ABORT_ORUNERRCLR                  (1UL << 4)
#define DP_ABORT_WDERRCLR                    (1UL << 3)
#define DP_ABORT_STKERRCLR                   (1UL << 2)
#define DP_ABORT_STKCMPCLR                   (1UL << 1)

#define DP_CTRL_CSYSPWRUPREQ                 (1UL << 30)
#define DP_CTRL_CSYSPWRUPACK                 (1UL << 31)
#define DP_CTRL_CDBGPWRUPREQ                 (1UL << 28)
#define DP_CTRL_CDBGPWRUPACK                 (1UL << 29)

#define AP_REG_CSW                           0x00U
#define AP_REG_TAR                           0x04U
#define AP_REG_DRW                           0x0CU
#define AP_REG_IDR                           0xFCU

#define MEM_AP_CSW_BASE                      0x23000010UL
#define MEM_AP_CSW_SIZE_8BIT                 0x0UL
#define MEM_AP_CSW_SIZE_16BIT                0x1UL
#define MEM_AP_CSW_SIZE_32BIT                0x2UL

#define ARM_DEMCR                            0xE000EDFCUL
#define ARM_DHCSR                            0xE000EDF0UL
#define ARM_DEMCR_TRCENA                     (1UL << 24)
#define ARM_DHCSR_DBGKEY                     0xA05F0000UL
#define ARM_DHCSR_C_DEBUGEN                  (1UL << 0)
#define ARM_DHCSR_C_HALT                     (1UL << 1)

#define STM32F1_FLASH_KEYR                   0x40022004UL
#define STM32F1_FLASH_OPTKEYR                0x40022008UL
#define STM32F1_FLASH_SR                     0x4002200CUL
#define STM32F1_FLASH_CR                     0x40022010UL

#define STM32F1_FLASH_KEY1                   0x45670123UL
#define STM32F1_FLASH_KEY2                   0xCDEF89ABUL
#define STM32F1_FLASH_SR_BSY                 (1UL << 0)
#define STM32F1_FLASH_SR_PGERR               (1UL << 2)
#define STM32F1_FLASH_SR_WRPRTERR            (1UL << 4)
#define STM32F1_FLASH_SR_EOP                 (1UL << 5)
#define STM32F1_FLASH_CR_OPTPG               (1UL << 4)
#define STM32F1_FLASH_CR_OPTER               (1UL << 5)
#define STM32F1_FLASH_CR_STRT                (1UL << 6)
#define STM32F1_FLASH_CR_LOCK                (1UL << 7)
#define STM32F1_FLASH_CR_OPTWRE              (1UL << 9)

#define STM32F1_OB_RDP_ADDRESS               0x1FFFF800UL
#define STM32F1_OB_RDP_LEVEL0_VALUE          0x5AA5U
#define STM32F1_OB_RDP_LEVEL1_VALUE          0xFF00U
#define STM32F1_OB_RDP_LEVEL2_VALUE          0x33CCU
#define STM32F1_RDP_LEVEL1_TO_0_RESET_HOLD_MS      100U
#define STM32F1_RDP_LEVEL1_TO_0_RELEASE_DELAY_MS   2000U

typedef struct
{
  swd_host_speed_t speed;
  uint32_t half_period_delay_cycles;
} swd_host_speed_profile_t;

static uint32_t g_dp_select_cache = 0xFFFFFFFFUL;
static uint32_t g_mem_ap_csw_cache = 0xFFFFFFFFUL;
static uint8_t g_swdio_is_output = 0U;
static swd_host_config_t g_swd_host_config =
{
  SWD_HOST_CONNECT_UNDER_RESET,
  SWD_HOST_SPEED_MEDIUM
};

static const swd_host_speed_profile_t g_swd_speed_profiles[] =
{
  {SWD_HOST_SPEED_VERY_LOW, 96U},
  {SWD_HOST_SPEED_LOW, 48U},
  {SWD_HOST_SPEED_MEDIUM, 16U},
  {SWD_HOST_SPEED_HIGH, 8U},
  {SWD_HOST_SPEED_VERY_HIGH, 2U}
};

static void swd_delay_half_period(void);
static void swd_clock_cycle(void);
static void swd_swdio_mode(uint8_t output);
static void swd_write_bit(uint8_t bit_value);
static uint8_t swd_read_bit(void);
static void swd_write_bits(uint32_t value, uint8_t bit_count);
static uint32_t swd_read_bits(uint8_t bit_count);
static uint8_t swd_parity32(uint32_t value);
static void swd_turnaround_to_target(void);
static void swd_turnaround_to_host(void);
static void swd_line_reset(void);
static void swd_send_jtag_to_swd_sequence(void);
static void swd_send_idle_cycles(uint8_t swdio_level, uint32_t cycles);
static void swd_set_target_reset(uint8_t asserted);
static void swd_end_transaction(void);
static swd_host_status_t swd_raw_access(uint8_t ap_access, uint8_t read_access, uint8_t address, uint32_t *data);
static swd_host_status_t swd_dp_write(uint8_t address, uint32_t value);
static swd_host_status_t swd_dp_read(uint8_t address, uint32_t *value);
static swd_host_status_t swd_select_ap_bank(uint32_t apsel, uint8_t bank);
static swd_host_status_t swd_ap_write(uint32_t apsel, uint8_t address, uint32_t value);
static swd_host_status_t swd_ap_read(uint32_t apsel, uint8_t address, uint32_t *value);
static swd_host_status_t swd_memap_set_csw(uint32_t csw_value);
static swd_host_status_t swd_memap_write(uint32_t address, uint32_t value, uint32_t csw_size);
static swd_host_status_t swd_memap_read(uint32_t address, uint32_t *value, uint32_t csw_size);
static swd_host_status_t swd_target_reset_pulse(uint32_t hold_ms, uint32_t release_ms);
static swd_host_status_t stm32f1_flash_wait_ready(uint32_t timeout_ms);
static swd_host_status_t stm32f1_flash_unlock(void);
static swd_host_status_t stm32f1_option_unlock(void);
static swd_host_status_t stm32f1_clear_flash_status(void);
static swd_host_status_t stm32f1_option_erase(void);
static swd_host_status_t stm32f1_option_program_halfword(uint32_t address, uint16_t value);
static swd_host_status_t stm32f1_reconnect_for_level1_access(void);
static uint16_t stm32f1_get_rdp_halfword(stm32f1_rdp_level_t level);
static uint32_t swd_get_half_period_delay_cycles(void);
static uint8_t swd_is_valid_speed(swd_host_speed_t speed);
static uint8_t swd_is_valid_connect_mode(swd_host_connect_mode_t connect_mode);

static void swd_delay_half_period(void)
{
  for (volatile uint32_t i = 0U; i < swd_get_half_period_delay_cycles(); ++i)
  {
    __NOP();
  }
}

static void swd_clock_cycle(void)
{
  swd_delay_half_period();
  HAL_GPIO_WritePin(SWD_HOST_SWCLK_GPIO_Port, SWD_HOST_SWCLK_Pin, GPIO_PIN_SET);
  swd_delay_half_period();
  HAL_GPIO_WritePin(SWD_HOST_SWCLK_GPIO_Port, SWD_HOST_SWCLK_Pin, GPIO_PIN_RESET);
}

static uint32_t swd_get_half_period_delay_cycles(void)
{
  for (uint32_t i = 0U; i < (sizeof(g_swd_speed_profiles) / sizeof(g_swd_speed_profiles[0])); ++i)
  {
    if (g_swd_speed_profiles[i].speed == g_swd_host_config.speed)
    {
      return g_swd_speed_profiles[i].half_period_delay_cycles;
    }
  }

  return 16U;
}

static uint8_t swd_is_valid_speed(swd_host_speed_t speed)
{
  return (uint8_t)(speed <= SWD_HOST_SPEED_VERY_HIGH);
}

static uint8_t swd_is_valid_connect_mode(swd_host_connect_mode_t connect_mode)
{
  return (uint8_t)(connect_mode <= SWD_HOST_CONNECT_UNDER_RESET);
}

static void swd_swdio_mode(uint8_t output)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (g_swdio_is_output == output)
  {
    return;
  }

  GPIO_InitStruct.Pin = SWD_HOST_SWDIO_Pin;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Mode = output ? GPIO_MODE_OUTPUT_PP : GPIO_MODE_INPUT;
  HAL_GPIO_Init(SWD_HOST_SWDIO_GPIO_Port, &GPIO_InitStruct);
  g_swdio_is_output = output;
}

static void swd_write_bit(uint8_t bit_value)
{
  HAL_GPIO_WritePin(SWD_HOST_SWDIO_GPIO_Port, SWD_HOST_SWDIO_Pin, bit_value ? GPIO_PIN_SET : GPIO_PIN_RESET);
  swd_clock_cycle();
}

static uint8_t swd_read_bit(void)
{
  uint8_t bit_value;

  swd_delay_half_period();
  bit_value = (uint8_t)(HAL_GPIO_ReadPin(SWD_HOST_SWDIO_GPIO_Port, SWD_HOST_SWDIO_Pin) == GPIO_PIN_SET);
  HAL_GPIO_WritePin(SWD_HOST_SWCLK_GPIO_Port, SWD_HOST_SWCLK_Pin, GPIO_PIN_SET);
  swd_delay_half_period();
  //bit_value = (uint8_t)(HAL_GPIO_ReadPin(SWD_HOST_SWDIO_GPIO_Port, SWD_HOST_SWDIO_Pin) == GPIO_PIN_SET);
  HAL_GPIO_WritePin(SWD_HOST_SWCLK_GPIO_Port, SWD_HOST_SWCLK_Pin, GPIO_PIN_RESET);

  return bit_value;
}

static void swd_write_bits(uint32_t value, uint8_t bit_count)
{
  for (uint8_t bit = 0U; bit < bit_count; ++bit)
  {
    swd_write_bit((uint8_t)((value >> bit) & 0x1U));
  }
}

static uint32_t swd_read_bits(uint8_t bit_count)
{
  uint32_t value = 0U;

  for (uint8_t bit = 0U; bit < bit_count; ++bit)
  {
    value |= ((uint32_t)swd_read_bit() << bit);
  }

  return value;
}

static uint8_t swd_parity32(uint32_t value)
{
  value ^= value >> 16;
  value ^= value >> 8;
  value ^= value >> 4;
  value &= 0xFU;

  return (uint8_t)((0x6996U >> value) & 0x1U);
}

static void swd_turnaround_to_target(void)
{
  swd_swdio_mode(0U);

  for (uint32_t i = 0U; i < SWD_TURNAROUND_CYCLES; ++i)
  {
    swd_clock_cycle();
  }
}

static void swd_turnaround_to_host(void)
{
  swd_swdio_mode(1U);
  HAL_GPIO_WritePin(SWD_HOST_SWDIO_GPIO_Port, SWD_HOST_SWDIO_Pin, GPIO_PIN_SET);

  for (uint32_t i = 0U; i < SWD_TURNAROUND_CYCLES; ++i)
  {
    swd_clock_cycle();
  }
}

static void swd_line_reset(void)
{
  swd_swdio_mode(1U);
  HAL_GPIO_WritePin(SWD_HOST_SWDIO_GPIO_Port, SWD_HOST_SWDIO_Pin, GPIO_PIN_SET);

  for (uint32_t i = 0U; i < SWD_LINE_RESET_CYCLES; ++i)
  {
    swd_clock_cycle();
  }
}

static void swd_send_jtag_to_swd_sequence(void)
{
  swd_swdio_mode(1U);
  swd_write_bits(0xE79EU, 16U);
}

static void swd_send_idle_cycles(uint8_t swdio_level, uint32_t cycles)
{
  swd_swdio_mode(1U);
  HAL_GPIO_WritePin(SWD_HOST_SWDIO_GPIO_Port,
                    SWD_HOST_SWDIO_Pin,
                    swdio_level ? GPIO_PIN_SET : GPIO_PIN_RESET);

  for (uint32_t i = 0U; i < cycles; ++i)
  {
    swd_clock_cycle();
  }
}

static void swd_set_target_reset(uint8_t asserted)
{
  HAL_GPIO_WritePin(SWD_HOST_nRESET_GPIO_Port,
                    SWD_HOST_nRESET_Pin,
                    asserted ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void swd_end_transaction(void)
{
  swd_swdio_mode(1U);
  HAL_GPIO_WritePin(SWD_HOST_SWDIO_GPIO_Port, SWD_HOST_SWDIO_Pin, GPIO_PIN_RESET);
  swd_send_idle_cycles(0U, SWD_TRANSACTION_IDLE_CYCLES);
}

static swd_host_status_t swd_raw_access(uint8_t ap_access, uint8_t read_access, uint8_t address, uint32_t *data)
{
  uint8_t request;
  uint8_t ack;
  uint32_t value;

  request = (uint8_t)(0x81U |
                      (ap_access ? 0x02U : 0x00U) |
                      (read_access ? 0x04U : 0x00U) |
                      ((address & 0x0CU) << 1));

  if ((((request >> 1) ^ (request >> 2) ^ (request >> 3) ^ (request >> 4)) & 0x1U) != 0U)
  {
    request |= 0x20U;
  }

  for (uint32_t retry = 0U; retry < SWD_MAX_RETRIES; ++retry)
  {
    swd_swdio_mode(1U);
    swd_write_bits(request, 8U);
    swd_turnaround_to_target();
    ack = (uint8_t)swd_read_bits(3U);

    if (ack == SWD_ACK_OK)
    {
      if (read_access != 0U)
      {
        value = swd_read_bits(32U);

        if (swd_read_bit() != swd_parity32(value))
        {
          swd_end_transaction();
          return SWD_HOST_PARITY_ERROR;
        }

        swd_turnaround_to_host();
        if (data != NULL)
        {
          *data = value;
        }
      }
      else
      {
        swd_turnaround_to_host();
        if (data != NULL)
        {
          swd_write_bits(*data, 32U);
          swd_write_bit(swd_parity32(*data));
        }
      }

      swd_end_transaction();
      return SWD_HOST_OK;
    }

    swd_turnaround_to_host();

    if (ack == SWD_ACK_WAIT)
    {
      swd_end_transaction();
      HAL_Delay(SWD_WAIT_RETRY_DELAY_MS);
      continue;
    }

    if (ack == SWD_ACK_FAULT)
    {
      swd_end_transaction();
      return SWD_HOST_ACK_FAULT;
    }

    swd_line_reset();
    return SWD_HOST_ERROR;
  }

  return SWD_HOST_ACK_WAIT;
}

static swd_host_status_t swd_dp_write(uint8_t address, uint32_t value)
{
  return swd_raw_access(0U, 0U, address, &value);
}

static swd_host_status_t swd_dp_read(uint8_t address, uint32_t *value)
{
  return swd_raw_access(0U, 1U, address, value);
}

static swd_host_status_t swd_select_ap_bank(uint32_t apsel, uint8_t bank)
{
  uint32_t select_value = (apsel << 24) | (((uint32_t)bank & 0x0FU) << 4);

  if (g_dp_select_cache == select_value)
  {
    return SWD_HOST_OK;
  }

  g_dp_select_cache = 0xFFFFFFFFUL;

  if (swd_dp_write(DP_REG_SELECT, select_value) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  g_dp_select_cache = select_value;
  return SWD_HOST_OK;
}

static swd_host_status_t swd_ap_write(uint32_t apsel, uint8_t address, uint32_t value)
{
  if (swd_select_ap_bank(apsel, (uint8_t)((address >> 4) & 0x0FU)) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  return swd_raw_access(1U, 0U, (uint8_t)(address & 0x0CU), &value);
}

static swd_host_status_t swd_ap_read(uint32_t apsel, uint8_t address, uint32_t *value)
{
  swd_host_status_t status;

  if (swd_select_ap_bank(apsel, (uint8_t)((address >> 4) & 0x0FU)) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  status = swd_raw_access(1U, 1U, (uint8_t)(address & 0x0CU), value);
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  return swd_dp_read(DP_REG_RDBUFF, value);
}

static swd_host_status_t swd_memap_set_csw(uint32_t csw_value)
{
  if (g_mem_ap_csw_cache == csw_value)
  {
    return SWD_HOST_OK;
  }

  if (swd_ap_write(0U, AP_REG_CSW, csw_value) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  g_mem_ap_csw_cache = csw_value;
  return SWD_HOST_OK;
}

static swd_host_status_t swd_memap_write(uint32_t address, uint32_t value, uint32_t csw_size)
{
  if (swd_memap_set_csw(MEM_AP_CSW_BASE | csw_size) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_ap_write(0U, AP_REG_TAR, address) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  return swd_ap_write(0U, AP_REG_DRW, value);
}

static swd_host_status_t swd_memap_read(uint32_t address, uint32_t *value, uint32_t csw_size)
{
  if (swd_memap_set_csw(MEM_AP_CSW_BASE | csw_size) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_ap_write(0U, AP_REG_TAR, address) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  return swd_ap_read(0U, AP_REG_DRW, value);
}

static swd_host_status_t swd_target_reset_pulse(uint32_t hold_ms, uint32_t release_ms)
{
  HAL_GPIO_WritePin(SWD_HOST_nRESET_GPIO_Port, SWD_HOST_nRESET_Pin, GPIO_PIN_RESET);
  HAL_Delay(hold_ms);
  HAL_GPIO_WritePin(SWD_HOST_nRESET_GPIO_Port, SWD_HOST_nRESET_Pin, GPIO_PIN_SET);
  HAL_Delay(release_ms);

  return SWD_HOST_OK;
}

static swd_host_status_t stm32f1_flash_wait_ready(uint32_t timeout_ms)
{
  uint32_t tickstart = HAL_GetTick();
  uint32_t flash_sr;

  do
  {
    if (swd_host_read_u32(STM32F1_FLASH_SR, &flash_sr) != SWD_HOST_OK)
    {
      return SWD_HOST_ERROR;
    }

    if ((flash_sr & STM32F1_FLASH_SR_BSY) == 0U)
    {
      if ((flash_sr & (STM32F1_FLASH_SR_PGERR | STM32F1_FLASH_SR_WRPRTERR)) != 0U)
      {
        return SWD_HOST_ERROR;
      }

      return SWD_HOST_OK;
    }
  } while ((HAL_GetTick() - tickstart) < timeout_ms);

  return SWD_HOST_TIMEOUT;
}

static swd_host_status_t stm32f1_flash_unlock(void)
{
  uint32_t flash_cr;

  if (swd_host_read_u32(STM32F1_FLASH_CR, &flash_cr) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if ((flash_cr & STM32F1_FLASH_CR_LOCK) == 0U)
  {
    return SWD_HOST_OK;
  }

  if (swd_host_write_u32(STM32F1_FLASH_KEYR, STM32F1_FLASH_KEY1) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_host_write_u32(STM32F1_FLASH_KEYR, STM32F1_FLASH_KEY2) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  return SWD_HOST_OK;
}

static swd_host_status_t stm32f1_option_unlock(void)
{
  uint32_t flash_cr;

  if (swd_host_read_u32(STM32F1_FLASH_CR, &flash_cr) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if ((flash_cr & STM32F1_FLASH_CR_OPTWRE) != 0U)
  {
    return SWD_HOST_OK;
  }

  if (swd_host_write_u32(STM32F1_FLASH_OPTKEYR, STM32F1_FLASH_KEY1) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_host_write_u32(STM32F1_FLASH_OPTKEYR, STM32F1_FLASH_KEY2) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  return SWD_HOST_OK;
}

static swd_host_status_t stm32f1_clear_flash_status(void)
{
  return swd_host_write_u32(STM32F1_FLASH_SR,
                            STM32F1_FLASH_SR_EOP | STM32F1_FLASH_SR_PGERR | STM32F1_FLASH_SR_WRPRTERR);
}

static swd_host_status_t stm32f1_option_erase(void)
{
  uint32_t flash_cr;

  if (stm32f1_flash_wait_ready(100U) != SWD_HOST_OK)
  {
    return SWD_HOST_TIMEOUT;
  }

  if (stm32f1_clear_flash_status() != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_host_read_u32(STM32F1_FLASH_CR, &flash_cr) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  flash_cr |= STM32F1_FLASH_CR_OPTWRE | STM32F1_FLASH_CR_OPTER;
  if (swd_host_write_u32(STM32F1_FLASH_CR, flash_cr) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_host_write_u32(STM32F1_FLASH_CR, flash_cr | STM32F1_FLASH_CR_STRT) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (stm32f1_flash_wait_ready(200U) != SWD_HOST_OK)
  {
    return SWD_HOST_TIMEOUT;
  }

  flash_cr &= ~STM32F1_FLASH_CR_OPTER;
  if (swd_host_write_u32(STM32F1_FLASH_CR, flash_cr) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  return stm32f1_clear_flash_status();
}

static swd_host_status_t stm32f1_option_program_halfword(uint32_t address, uint16_t value)
{
  uint32_t flash_cr;
  uint32_t timeout_ms = 100U;

  if ((address == STM32F1_OB_RDP_ADDRESS) && (value == STM32F1_OB_RDP_LEVEL0_VALUE))
  {
    /*
     * RDP Level 1 -> Level 0 triggers a main flash mass erase before the
     * option byte is finally reprogrammed, so allow much longer here.
     */
    timeout_ms = 3000U;
  }

  if (stm32f1_flash_wait_ready(100U) != SWD_HOST_OK)
  {
    return SWD_HOST_TIMEOUT;
  }

  if (swd_host_read_u32(STM32F1_FLASH_CR, &flash_cr) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  flash_cr |= STM32F1_FLASH_CR_OPTWRE | STM32F1_FLASH_CR_OPTPG;
  if (swd_host_write_u32(STM32F1_FLASH_CR, flash_cr) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_host_write_u16(address, value) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (stm32f1_flash_wait_ready(timeout_ms) != SWD_HOST_OK)
  {
    return SWD_HOST_TIMEOUT;
  }

  flash_cr &= ~STM32F1_FLASH_CR_OPTPG;
  if (swd_host_write_u32(STM32F1_FLASH_CR, flash_cr) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  return stm32f1_clear_flash_status();
}

static swd_host_status_t stm32f1_reconnect_for_level1_access(void)
{
  swd_host_config_t saved_config = g_swd_host_config;
  swd_host_status_t status;

  g_swd_host_config.connect_mode = SWD_HOST_CONNECT_UNDER_RESET;
  g_swd_host_config.speed = SWD_HOST_SPEED_VERY_LOW;

  status = swd_host_connect();

  g_swd_host_config = saved_config;
  return status;
}

static uint16_t stm32f1_get_rdp_halfword(stm32f1_rdp_level_t level)
{
  if (level == STM32F1_RDP_LEVEL_0)
  {
    return STM32F1_OB_RDP_LEVEL0_VALUE;
  }

  if (level == STM32F1_RDP_LEVEL_1)
  {
    return STM32F1_OB_RDP_LEVEL1_VALUE;
  }

  return STM32F1_OB_RDP_LEVEL2_VALUE;
}

swd_host_status_t swd_host_init(void)
{
  return swd_host_init_with_config(NULL);
}

swd_host_status_t swd_host_init_with_config(const swd_host_config_t *config)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (config != NULL)
  {
    if (swd_host_configure(config) != SWD_HOST_OK)
    {
      return SWD_HOST_ERROR;
    }
  }

  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitStruct.Pin = SWD_HOST_SWCLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SWD_HOST_SWCLK_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SWD_HOST_nRESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SWD_HOST_nRESET_GPIO_Port, &GPIO_InitStruct);

  HAL_GPIO_WritePin(SWD_HOST_SWCLK_GPIO_Port, SWD_HOST_SWCLK_Pin, GPIO_PIN_RESET);
  swd_set_target_reset(0U);

  g_swdio_is_output = 0U;
  swd_swdio_mode(1U);
  HAL_GPIO_WritePin(SWD_HOST_SWDIO_GPIO_Port, SWD_HOST_SWDIO_Pin, GPIO_PIN_SET);

  g_dp_select_cache = 0xFFFFFFFFUL;
  g_mem_ap_csw_cache = 0xFFFFFFFFUL;

  return SWD_HOST_OK;
}

swd_host_status_t swd_host_configure(const swd_host_config_t *config)
{
  if (config == NULL)
  {
    return SWD_HOST_ERROR;
  }

  if ((swd_is_valid_connect_mode(config->connect_mode) == 0U) ||
      (swd_is_valid_speed(config->speed) == 0U))
  {
    return SWD_HOST_ERROR;
  }

  g_swd_host_config = *config;
  return SWD_HOST_OK;
}

void swd_host_get_config(swd_host_config_t *config)
{
  if (config != NULL)
  {
    *config = g_swd_host_config;
  }
}

swd_host_status_t swd_host_set_connect_mode(swd_host_connect_mode_t connect_mode)
{
  if (swd_is_valid_connect_mode(connect_mode) == 0U)
  {
    return SWD_HOST_ERROR;
  }

  g_swd_host_config.connect_mode = connect_mode;
  return SWD_HOST_OK;
}

swd_host_status_t swd_host_set_speed(swd_host_speed_t speed)
{
  if (swd_is_valid_speed(speed) == 0U)
  {
    return SWD_HOST_ERROR;
  }

  g_swd_host_config.speed = speed;
  return SWD_HOST_OK;
}

swd_host_status_t swd_host_connect(void)
{
  uint32_t idcode;
  uint32_t ctrl_stat;
  uint32_t ap_idr;

  /* Invalidate caches: line reset below resets DP/AP hardware state */
  g_dp_select_cache = 0xFFFFFFFFUL;
  g_mem_ap_csw_cache = 0xFFFFFFFFUL;

  if (g_swd_host_config.connect_mode == SWD_HOST_CONNECT_UNDER_RESET)
  {
    swd_set_target_reset(1U);
    HAL_Delay(SWD_RESET_HOLD_DELAY_MS);
  }
  else
  {
    swd_set_target_reset(1U);
    HAL_Delay(SWD_RESET_HOLD_DELAY_MS);
    swd_set_target_reset(0U);
    HAL_Delay(SWD_RESET_RELEASE_DELAY_MS);
  }

  for (uint32_t sequence = 0U; sequence < SWD_CONNECT_SEQUENCE_REPEATS; ++sequence)
  {
    swd_line_reset();
    swd_send_jtag_to_swd_sequence();
    swd_line_reset();
    swd_send_idle_cycles(0U, SWD_CONNECT_IDLE_LOW_CYCLES);
  }

  if (g_swd_host_config.connect_mode == SWD_HOST_CONNECT_UNDER_RESET)
  {
    swd_set_target_reset(0U);
    HAL_Delay(SWD_RESET_RELEASE_DELAY_MS);
    swd_line_reset();
  }

  /* Keep a low-idle window after the final line reset before the first DP access. */
  swd_send_idle_cycles(0U, SWD_CONNECT_IDLE_LOW_CYCLES);

  if (swd_dp_read(DP_REG_IDCODE, &idcode) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (idcode == 0U)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_dp_write(DP_REG_ABORT,
                   DP_ABORT_ORUNERRCLR | DP_ABORT_WDERRCLR | DP_ABORT_STKERRCLR | DP_ABORT_STKCMPCLR) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_dp_write(DP_REG_SELECT, 0U) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_dp_read(DP_REG_CTRL_STAT, &ctrl_stat) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_dp_write(DP_REG_CTRL_STAT, DP_CTRL_CDBGPWRUPREQ | DP_CTRL_CSYSPWRUPREQ) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  for (uint32_t retry = 0U; retry < SWD_MAX_RETRIES; ++retry)
  {
    if (swd_dp_read(DP_REG_CTRL_STAT, &ctrl_stat) != SWD_HOST_OK)
    {
      return SWD_HOST_ERROR;
    }

    if ((ctrl_stat & (DP_CTRL_CDBGPWRUPACK | DP_CTRL_CSYSPWRUPACK)) ==
        (DP_CTRL_CDBGPWRUPACK | DP_CTRL_CSYSPWRUPACK))
    {
      break;
    }

    if (retry == (SWD_MAX_RETRIES - 1U))
    {
      return SWD_HOST_TIMEOUT;
    }
  }

  HAL_Delay(SWD_AP_READY_DELAY_MS);

  if (swd_host_read_ap_idr(0U, &ap_idr) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_memap_set_csw(MEM_AP_CSW_BASE | MEM_AP_CSW_SIZE_32BIT) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_host_write_u32(ARM_DEMCR, ARM_DEMCR_TRCENA) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_host_write_u32(ARM_DHCSR, ARM_DHCSR_DBGKEY | ARM_DHCSR_C_DEBUGEN | ARM_DHCSR_C_HALT) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  return SWD_HOST_OK;
}

void swd_host_disconnect(void)
{
  swd_line_reset();
}

swd_host_status_t swd_host_read_u8(uint32_t address, uint8_t *value)
{
  uint32_t raw_value;
  uint32_t lane_shift;

  if (value == NULL)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_memap_read(address, &raw_value, MEM_AP_CSW_SIZE_8BIT) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  lane_shift = (address & 0x3U) * 8U;
  *value = (uint8_t)((raw_value >> lane_shift) & 0xFFU);

  return SWD_HOST_OK;
}

swd_host_status_t swd_host_read_u16(uint32_t address, uint16_t *value)
{
  uint32_t raw_value;
  uint32_t lane_shift;

  if ((value == NULL) || ((address & 0x1U) != 0U))
  {
    return SWD_HOST_ALIGNMENT_ERROR;
  }

  if (swd_memap_read(address, &raw_value, MEM_AP_CSW_SIZE_16BIT) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  lane_shift = (address & 0x2U) * 8U;
  *value = (uint16_t)((raw_value >> lane_shift) & 0xFFFFU);

  return SWD_HOST_OK;
}

swd_host_status_t swd_host_read_u32(uint32_t address, uint32_t *value)
{
  if ((value == NULL) || ((address & 0x3U) != 0U))
  {
    return SWD_HOST_ALIGNMENT_ERROR;
  }

  return swd_memap_read(address, value, MEM_AP_CSW_SIZE_32BIT);
}

swd_host_status_t swd_host_read_ap_idr(uint32_t apsel, uint32_t *value)
{
  if (value == NULL)
  {
    return SWD_HOST_ERROR;
  }

  return swd_ap_read(apsel, AP_REG_IDR, value);
}

swd_host_status_t swd_host_write_u8(uint32_t address, uint8_t value)
{
  uint32_t write_value = (uint32_t)value << ((address & 0x3U) * 8U);

  return swd_memap_write(address, write_value, MEM_AP_CSW_SIZE_8BIT);
}

swd_host_status_t swd_host_write_u16(uint32_t address, uint16_t value)
{
  uint32_t write_value;

  if ((address & 0x1U) != 0U)
  {
    return SWD_HOST_ALIGNMENT_ERROR;
  }

  write_value = (uint32_t)value << ((address & 0x2U) * 8U);
  return swd_memap_write(address, write_value, MEM_AP_CSW_SIZE_16BIT);
}

swd_host_status_t swd_host_write_u32(uint32_t address, uint32_t value)
{
  if ((address & 0x3U) != 0U)
  {
    return SWD_HOST_ALIGNMENT_ERROR;
  }

  return swd_memap_write(address, value, MEM_AP_CSW_SIZE_32BIT);
}

swd_host_status_t stm32f1_read_u32(uint32_t address, uint32_t *value)
{
  swd_host_status_t status = swd_host_connect();

  if (status != SWD_HOST_OK)
  {
    return status;
  }

  return swd_host_read_u32(address, value);
}

swd_host_status_t stm32f1_write_u32(uint32_t address, uint32_t value)
{
  swd_host_status_t status = swd_host_connect();

  if (status != SWD_HOST_OK)
  {
    return status;
  }

  return swd_host_write_u32(address, value);
}

swd_host_status_t stm32f1_get_rdp_level(stm32f1_rdp_level_t *level)
{
  uint16_t rdp_value;
  swd_host_status_t status;

  if (level == NULL)
  {
    return SWD_HOST_ERROR;
  }

  status = swd_host_connect();
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  status = swd_host_read_u16(STM32F1_OB_RDP_ADDRESS, &rdp_value);
  if (status != SWD_HOST_OK)
  {
    /*
     * On STM32F1, once RDP Level 1 is active, option-byte reads through the
     * MEM-AP can fail even though the SWD connection itself is still alive.
     * Treat a failed read here as "protected" so the caller can still drive
     * the Level 1 -> Level 0 recovery flow.
     */
    *level = STM32F1_RDP_LEVEL_1;
    return SWD_HOST_OK;
  }

  if (rdp_value == STM32F1_OB_RDP_LEVEL0_VALUE)
  {
    *level = STM32F1_RDP_LEVEL_0;
  }
  else if (rdp_value == STM32F1_OB_RDP_LEVEL2_VALUE)
  {
    *level = STM32F1_RDP_LEVEL_2;
  }
  else
  {
    *level = STM32F1_RDP_LEVEL_1;
  }

  return SWD_HOST_OK;
}

swd_host_status_t stm32f1_set_rdp_level(stm32f1_rdp_level_t level)
{
  swd_host_status_t status;
  stm32f1_rdp_level_t current_level;

  status = stm32f1_get_rdp_level(&current_level);
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  if (current_level == level)
  {
    return SWD_HOST_OK;
  }

  if (level == STM32F1_RDP_LEVEL_2)
  {
    return SWD_HOST_TARGET_LOCKED;
  }

  if (current_level == STM32F1_RDP_LEVEL_1)
  {
    status = stm32f1_reconnect_for_level1_access();
    if (status != SWD_HOST_OK)
    {
      return status;
    }
  }

  status = stm32f1_flash_unlock();
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  status = stm32f1_option_unlock();
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  status = stm32f1_option_erase();
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  if (current_level == STM32F1_RDP_LEVEL_0)
  {
    /*
     * Level 0 -> Level 1 worked reliably with the original simple sequence:
     * erase the option bytes, then program only the RDP halfword.
     */
    status = stm32f1_option_program_halfword(STM32F1_OB_RDP_ADDRESS, stm32f1_get_rdp_halfword(level));
    if (status != SWD_HOST_OK)
    {
      return status;
    }
  }
  else
  {
    /*
     * Level 1 -> Level 0 cannot rely on reading option bytes first because
     * those reads may already be blocked by RDP. Follow the STM32F1 manual's
     * minimal regression path: erase option bytes, program RDP=0xA5, then
     * allow a long reset/reload delay.
     */
    status = stm32f1_option_program_halfword(STM32F1_OB_RDP_ADDRESS, stm32f1_get_rdp_halfword(level));
    if (status != SWD_HOST_OK)
    {
      return status;
    }
  }

  if (level == STM32F1_RDP_LEVEL_0)
  {
    if (swd_target_reset_pulse(STM32F1_RDP_LEVEL1_TO_0_RESET_HOLD_MS,
                               STM32F1_RDP_LEVEL1_TO_0_RELEASE_DELAY_MS) != SWD_HOST_OK)
    {
      return SWD_HOST_ERROR;
    }

    swd_host_disconnect();
    return SWD_HOST_OK;
  }

  if (swd_target_reset_pulse(20U, 50U) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  return SWD_HOST_OK;
}
