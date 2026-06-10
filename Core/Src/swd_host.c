#include "swd_host.h"
#include "l0_ob_loader_blob.h"
#include "l1_ob_loader_blob.h"
#include "g0_ob_loader_blob.h"
#include "g4_ob_loader_blob.h"
#include "f0_ob_loader_blob.h"
#include "u0_ob_loader_blob.h"
#include "l4_ob_loader_blob.h"

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
#define ARM_DCRSR                            0xE000EDF4UL
#define ARM_DCRDR                            0xE000EDF8UL
#define ARM_DEMCR_TRCENA                     (1UL << 24)
#define ARM_DHCSR_DBGKEY                     0xA05F0000UL
#define ARM_DHCSR_C_DEBUGEN                  (1UL << 0)
#define ARM_DHCSR_C_HALT                     (1UL << 1)
#define ARM_DHCSR_S_REGRDY                   (1UL << 16)
#define ARM_DHCSR_S_HALT                     (1UL << 17)
#define ARM_DCRSR_REGWnR                     (1UL << 16)
#define ARM_CORE_REG_SP                      13U
#define ARM_CORE_REG_PC                      15U
#define ARM_CORE_REG_XPSR                    16U
#define ARM_XPSR_THUMB                       (1UL << 24)

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

/* STM32L0 flash interface — registers, keys, bits, option byte */
#define STM32L0_FLASH_PECR                   0x40022004UL
#define STM32L0_FLASH_PEKEYR                 0x4002200CUL
#define STM32L0_FLASH_PRGKEYR                0x40022010UL
#define STM32L0_FLASH_OPTKEYR                0x40022014UL
#define STM32L0_FLASH_SR                     0x40022018UL
#define STM32L0_FLASH_OBR                    0x4002201CUL

#define STM32L0_FLASH_PEKEY1                 0x89ABCDEFUL
#define STM32L0_FLASH_PEKEY2                 0x02030405UL
#define STM32L0_FLASH_PRGKEY1                0x8C9DAEBFUL
#define STM32L0_FLASH_PRGKEY2                0x13141516UL
#define STM32L0_FLASH_OPTKEY1                0xFBEAD9C8UL
#define STM32L0_FLASH_OPTKEY2                0x24252627UL

#define STM32L0_FLASH_PECR_PELOCK            (1UL << 0)
#define STM32L0_FLASH_PECR_PRGLOCK           (1UL << 1)
#define STM32L0_FLASH_PECR_OPTLOCK           (1UL << 2)
#define STM32L0_FLASH_PECR_PROG              (1UL << 3)
#define STM32L0_FLASH_PECR_ERASE             (1UL << 9)
#define STM32L0_FLASH_PECR_FPRG              (1UL << 10)
#define STM32L0_FLASH_PECR_OBL_LAUNCH        (1UL << 18)

#define STM32L0_FLASH_SR_BSY                 (1UL << 0)
#define STM32L0_FLASH_SR_EOP                 (1UL << 1)
#define STM32L0_FLASH_SR_WRPERR              (1UL << 8)
#define STM32L0_FLASH_SR_PGAERR              (1UL << 9)
#define STM32L0_FLASH_SR_SIZERR              (1UL << 10)
#define STM32L0_FLASH_SR_OPTVERR             (1UL << 11)

#define STM32L0_OB_RDP_ADDRESS               0x1FF80000UL
#define STM32L0_OB_RDP_LEVEL0_BYTE           0xAAU
#define STM32L0_OB_RDP_LEVEL1_BYTE           0xBBU
#define STM32L0_OB_RDP_LEVEL2_BYTE           0xCCU
#define STM32L0_RDP_LEVEL1_TO_0_RESET_HOLD_MS      100U
#define STM32L0_RDP_LEVEL1_TO_0_RELEASE_DELAY_MS   2000U

/*
 * STM32G0 flash interface. Unlike the L0 (PECR/PEKEYR, BSY = bit 0), the G0
 * uses the newer flash controller shared with G4/L4/WB: CR.LOCK/OPTLOCK gate
 * register and option-byte writes, the RDP level lives in OPTR[7:0], BSY is
 * bit 16, and option programming is committed with CR.OPTSTRT then reloaded
 * with CR.OBL_LAUNCH. RDP cannot be flipped over the debug MEM-AP under
 * Level 1, so it is done by the on-target mini flashloader (see
 * g0_ob_loader_blob.h), exactly like the L0 path.
 */
#define STM32G0_FLASH_KEYR                   0x40022008UL
#define STM32G0_FLASH_OPTKEYR                0x4002200CUL
#define STM32G0_FLASH_SR                     0x40022010UL
#define STM32G0_FLASH_CR                     0x40022014UL
#define STM32G0_FLASH_OPTR                   0x40022020UL

#define STM32G0_FLASH_KEY1                   0x45670123UL
#define STM32G0_FLASH_KEY2                   0xCDEF89ABUL
#define STM32G0_FLASH_OPTKEY1                0x08192A3BUL
#define STM32G0_FLASH_OPTKEY2                0x4C5D6E7FUL

#define STM32G0_FLASH_CR_OPTSTRT             (1UL << 17)
#define STM32G0_FLASH_CR_OBL_LAUNCH          (1UL << 27)
#define STM32G0_FLASH_CR_OPTLOCK             (1UL << 30)
#define STM32G0_FLASH_CR_LOCK                (1UL << 31)

#define STM32G0_FLASH_SR_OPTVERR             (1UL << 15)
#define STM32G0_FLASH_SR_BSY                 (1UL << 16)

#define STM32G0_OPTR_RDP_MASK                0x000000FFUL
#define STM32G0_OB_RDP_LEVEL0_BYTE           0xAAU
#define STM32G0_OB_RDP_LEVEL1_BYTE           0xBBU
#define STM32G0_OB_RDP_LEVEL2_BYTE           0xCCU
#define STM32G0_RDP_LEVEL1_TO_0_RELEASE_DELAY_MS   2000U

/*
 * STM32L1 flash interface. Same EEPROM/PECR controller as the STM32L0 (same
 * keys, PECR/SR bit positions, option-byte layout) — only the register base
 * differs: L0 = 0x40022000, L1 = 0x40023C00. The option byte block and the
 * 16-bit halfword-complement RDP word format are identical to L0. As with L0,
 * RDP is flipped by the on-target mini flashloader (see l1_ob_loader_blob.h).
 */
#define STM32L1_FLASH_OBR                    0x40023C1CUL

#define STM32L1_OB_RDP_ADDRESS               0x1FF80000UL
#define STM32L1_OB_RDP_LEVEL0_BYTE           0xAAU
#define STM32L1_OB_RDP_LEVEL1_BYTE           0xBBU
#define STM32L1_OB_RDP_LEVEL2_BYTE           0xCCU
#define STM32L1_RDP_LEVEL1_TO_0_RELEASE_DELAY_MS   2000U

/*
 * STM32G4 flash interface. The flash IP is identical to the STM32G0 one
 * (same base 0x40022000, same register offsets, keys, bit positions, and
 * OPTR[7:0] RDP encoding). The constants below mirror the G0 values; the
 * loader-driving registers (KEYR/OPTKEYR/CR) live only inside the on-target
 * flashloader (see g4_ob_loader_blob.h), so the host only needs OPTR here.
 */
#define STM32G4_FLASH_OPTR                   0x40022020UL
#define STM32G4_OPTR_RDP_MASK                0x000000FFUL
#define STM32G4_OB_RDP_LEVEL0_BYTE           0xAAU
#define STM32G4_OB_RDP_LEVEL1_BYTE           0xBBU
#define STM32G4_OB_RDP_LEVEL2_BYTE           0xCCU
#define STM32G4_RDP_LEVEL1_TO_0_RELEASE_DELAY_MS   2000U

/*
 * STM32F0 flash interface. The F0 uses the older FPEC flash controller — the
 * same IP as the STM32F1 (register base 0x40022000, KEYR/OPTKEYR/SR/CR/OBR at
 * the F1 offsets, the same unlock keys, SR.BSY = bit 0, halfword option-byte
 * programming at 0x1FFFF800). It differs from F1 in two ways: the RDP byte
 * uses the modern 3-level scheme (0xAA = L0, 0xCC = L2, anything else = L1)
 * read back from OBR RDPRT1/RDPRT2, and the F0 FPEC HAS an FLASH_CR.OBL_LAUNCH
 * bit (bit 13) that F1 lacks. Like the L0/G0 families, RDP cannot be flipped
 * over the debug MEM-AP under Level 1, so it is done by the on-target mini
 * flashloader (see f0_ob_loader_blob.h). The loader itself sets OBL_LAUNCH to
 * reload the option bytes: a plain NRST / system reset does NOT reload them on
 * F0 (only a power-on reset or OBL_LAUNCH does), so relying on an nRST pulse
 * here would require a manual power cycle for the new RDP level to apply.
 */
#define STM32F0_FLASH_OBR                    0x4002201CUL
#define STM32F0_OBR_RDPRT1                   (1UL << 1)
#define STM32F0_OBR_RDPRT2                   (1UL << 2)
#define STM32F0_OB_RDP_LEVEL0_BYTE           0xAAU
#define STM32F0_OB_RDP_LEVEL1_BYTE           0xBBU
#define STM32F0_OB_RDP_LEVEL2_BYTE           0xCCU
#define STM32F0_RDP_LEVEL1_TO_0_RELEASE_DELAY_MS   2000U

/*
 * STM32U0 flash interface. The U0 uses the newer flash controller shared with
 * G0/G4/L4 — the register base (0x40022000), the KEYR/OPTKEYR/SR/CR/OPTR
 * offsets, the unlock keys, SR.BSY = bit 16, the OPTR[7:0] RDP byte, and the
 * OPTSTRT/OBL_LAUNCH commit sequence are all identical to the G0. RDP is
 * flipped by the on-target mini flashloader (see u0_ob_loader_blob.h).
 */
#define STM32U0_FLASH_OPTR                   0x40022020UL
#define STM32U0_OPTR_RDP_MASK                0x000000FFUL
#define STM32U0_OB_RDP_LEVEL0_BYTE           0xAAU
#define STM32U0_OB_RDP_LEVEL1_BYTE           0xBBU
#define STM32U0_OB_RDP_LEVEL2_BYTE           0xCCU
#define STM32U0_RDP_LEVEL1_TO_0_RELEASE_DELAY_MS   2000U

/*
 * STM32L4 flash interface. The flash IP is identical to the STM32G4/G0 one
 * (same base 0x40022000, same register offsets, keys, bit positions, and
 * OPTR[7:0] RDP encoding). As with G4, the host only needs OPTR here; the
 * loader-driving registers live inside the on-target flashloader (see
 * l4_ob_loader_blob.h).
 */
#define STM32L4_FLASH_OPTR                   0x40022020UL
#define STM32L4_OPTR_RDP_MASK                0x000000FFUL
#define STM32L4_OB_RDP_LEVEL0_BYTE           0xAAU
#define STM32L4_OB_RDP_LEVEL1_BYTE           0xBBU
#define STM32L4_OB_RDP_LEVEL2_BYTE           0xCCU
#define STM32L4_RDP_LEVEL1_TO_0_RELEASE_DELAY_MS   2000U

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
static swd_host_status_t stm32l0_flash_wait_ready(uint32_t timeout_ms);
static swd_host_status_t stm32l0_pecr_unlock(void);
static swd_host_status_t stm32l0_option_unlock(void);
static swd_host_status_t stm32l0_clear_flash_status(void);
static swd_host_status_t stm32l0_option_program_word(uint32_t address, uint32_t value);
static swd_host_status_t stm32l0_reconnect_for_level1_access(void);
static uint8_t stm32l0_get_rdp_byte(stm32l0_rdp_level_t level);
static swd_host_status_t stm32l0_cpu_halt(uint32_t timeout_ms);
static swd_host_status_t stm32l0_write_core_register(uint8_t regsel, uint32_t value);
static swd_host_status_t stm32l0_run_ob_loader(uint32_t ob_word_value);
static swd_host_status_t stm32g0_reconnect_for_level1_access(void);
static uint8_t stm32g0_get_rdp_byte(stm32g0_rdp_level_t level);
static swd_host_status_t stm32g0_run_ob_loader(uint32_t rdp_byte_value);
static swd_host_status_t stm32l1_reconnect_for_level1_access(void);
static uint8_t stm32l1_get_rdp_byte(stm32l1_rdp_level_t level);
static swd_host_status_t stm32l1_run_ob_loader(uint32_t ob_word_value);
static swd_host_status_t stm32g4_reconnect_for_level1_access(void);
static uint8_t stm32g4_get_rdp_byte(stm32g4_rdp_level_t level);
static swd_host_status_t stm32g4_run_ob_loader(uint32_t rdp_byte_value);
static swd_host_status_t stm32f0_reconnect_for_level1_access(void);
static uint8_t stm32f0_get_rdp_byte(stm32f0_rdp_level_t level);
static swd_host_status_t stm32f0_run_ob_loader(uint32_t rdp_byte_value);
static swd_host_status_t stm32u0_reconnect_for_level1_access(void);
static uint8_t stm32u0_get_rdp_byte(stm32u0_rdp_level_t level);
static swd_host_status_t stm32u0_run_ob_loader(uint32_t rdp_byte_value);
static swd_host_status_t stm32l4_reconnect_for_level1_access(void);
static uint8_t stm32l4_get_rdp_byte(stm32l4_rdp_level_t level);
static swd_host_status_t stm32l4_run_ob_loader(uint32_t rdp_byte_value);
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

static swd_host_status_t stm32l0_flash_wait_ready(uint32_t timeout_ms)
{
  uint32_t tickstart = HAL_GetTick();
  uint32_t flash_sr;

  do
  {
    if (swd_host_read_u32(STM32L0_FLASH_SR, &flash_sr) != SWD_HOST_OK)
    {
      return SWD_HOST_ERROR;
    }

    if ((flash_sr & STM32L0_FLASH_SR_BSY) == 0U)
    {
      if ((flash_sr & (STM32L0_FLASH_SR_WRPERR | STM32L0_FLASH_SR_PGAERR |
                       STM32L0_FLASH_SR_SIZERR | STM32L0_FLASH_SR_OPTVERR)) != 0U)
      {
        return SWD_HOST_ERROR;
      }

      return SWD_HOST_OK;
    }
  } while ((HAL_GetTick() - tickstart) < timeout_ms);

  return SWD_HOST_TIMEOUT;
}

static swd_host_status_t stm32l0_pecr_unlock(void)
{
  uint32_t flash_pecr;

  if (swd_host_read_u32(STM32L0_FLASH_PECR, &flash_pecr) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if ((flash_pecr & STM32L0_FLASH_PECR_PELOCK) == 0U)
  {
    return SWD_HOST_OK;
  }

  if (swd_host_write_u32(STM32L0_FLASH_PEKEYR, STM32L0_FLASH_PEKEY1) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_host_write_u32(STM32L0_FLASH_PEKEYR, STM32L0_FLASH_PEKEY2) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  return SWD_HOST_OK;
}

static swd_host_status_t stm32l0_option_unlock(void)
{
  uint32_t flash_pecr;

  if (swd_host_read_u32(STM32L0_FLASH_PECR, &flash_pecr) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if ((flash_pecr & STM32L0_FLASH_PECR_OPTLOCK) == 0U)
  {
    return SWD_HOST_OK;
  }

  if (swd_host_write_u32(STM32L0_FLASH_OPTKEYR, STM32L0_FLASH_OPTKEY1) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_host_write_u32(STM32L0_FLASH_OPTKEYR, STM32L0_FLASH_OPTKEY2) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  return SWD_HOST_OK;
}

static swd_host_status_t stm32l0_clear_flash_status(void)
{
  return swd_host_write_u32(STM32L0_FLASH_SR,
                            STM32L0_FLASH_SR_EOP | STM32L0_FLASH_SR_WRPERR |
                            STM32L0_FLASH_SR_PGAERR | STM32L0_FLASH_SR_SIZERR |
                            STM32L0_FLASH_SR_OPTVERR);
}

static swd_host_status_t stm32l0_option_program_word(uint32_t address, uint32_t value)
{
  uint32_t flash_pecr;
  uint32_t timeout_ms = 100U;

  /*
   * Programming RDP=0xAA (Level 0) triggers a flash mass erase on STM32L0,
   * so allow much longer for completion just like the F1 path.
   */
  if ((address == STM32L0_OB_RDP_ADDRESS) &&
      ((uint8_t)(value & 0xFFU) == STM32L0_OB_RDP_LEVEL0_BYTE))
  {
    timeout_ms = 3000U;
  }

  if (stm32l0_flash_wait_ready(100U) != SWD_HOST_OK)
  {
    return SWD_HOST_TIMEOUT;
  }

  if (stm32l0_clear_flash_status() != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* Erase the option byte word: set ERASE, write 0 to the OB address. */
  if (swd_host_read_u32(STM32L0_FLASH_PECR, &flash_pecr) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_host_write_u32(STM32L0_FLASH_PECR, flash_pecr | STM32L0_FLASH_PECR_ERASE) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_host_write_u32(address, 0UL) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (stm32l0_flash_wait_ready(timeout_ms) != SWD_HOST_OK)
  {
    return SWD_HOST_TIMEOUT;
  }

  if (swd_host_write_u32(STM32L0_FLASH_PECR, flash_pecr & ~STM32L0_FLASH_PECR_ERASE) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* Program the new option byte word. FTDW handles the actual write. */
  if (swd_host_write_u32(address, value) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (stm32l0_flash_wait_ready(timeout_ms) != SWD_HOST_OK)
  {
    return SWD_HOST_TIMEOUT;
  }

  if (stm32l0_clear_flash_status() != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /*
   * Trigger option byte reload via OBL_LAUNCH (PECR bit 18).
   * The target generates its own NRST_OUT pulse and resets, so the SWD
   * transaction that sets this bit typically fails to ACK — that is normal
   * and not an error. Re-read PECR fresh in case ERASE was still set.
   */
  if (swd_host_read_u32(STM32L0_FLASH_PECR, &flash_pecr) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  (void)swd_host_write_u32(STM32L0_FLASH_PECR,
                           (flash_pecr & ~STM32L0_FLASH_PECR_ERASE) | STM32L0_FLASH_PECR_OBL_LAUNCH);

  return SWD_HOST_OK;
}

static swd_host_status_t stm32l0_reconnect_for_level1_access(void)
{
  swd_host_config_t saved_config = g_swd_host_config;
  swd_host_status_t status;

  g_swd_host_config.connect_mode = SWD_HOST_CONNECT_UNDER_RESET;
  g_swd_host_config.speed = SWD_HOST_SPEED_VERY_LOW;

  status = swd_host_connect();

  g_swd_host_config = saved_config;
  return status;
}

static uint8_t stm32l0_get_rdp_byte(stm32l0_rdp_level_t level)
{
  if (level == STM32L0_RDP_LEVEL_0)
  {
    return STM32L0_OB_RDP_LEVEL0_BYTE;
  }

  if (level == STM32L0_RDP_LEVEL_2)
  {
    return STM32L0_OB_RDP_LEVEL2_BYTE;
  }

  return STM32L0_OB_RDP_LEVEL1_BYTE;
}

static swd_host_status_t stm32l0_cpu_halt(uint32_t timeout_ms)
{
  uint32_t tickstart;
  uint32_t dhcsr;

  if (swd_host_write_u32(ARM_DHCSR,
                         ARM_DHCSR_DBGKEY | ARM_DHCSR_C_DEBUGEN | ARM_DHCSR_C_HALT) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  tickstart = HAL_GetTick();
  do
  {
    if (swd_host_read_u32(ARM_DHCSR, &dhcsr) != SWD_HOST_OK)
    {
      return SWD_HOST_ERROR;
    }
    if ((dhcsr & ARM_DHCSR_S_HALT) != 0U)
    {
      return SWD_HOST_OK;
    }
  } while ((HAL_GetTick() - tickstart) < timeout_ms);

  return SWD_HOST_TIMEOUT;
}

static swd_host_status_t stm32l0_write_core_register(uint8_t regsel, uint32_t value)
{
  uint32_t tickstart;
  uint32_t dhcsr;

  if (swd_host_write_u32(ARM_DCRDR, value) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (swd_host_write_u32(ARM_DCRSR, ARM_DCRSR_REGWnR | (uint32_t)regsel) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  tickstart = HAL_GetTick();
  do
  {
    if (swd_host_read_u32(ARM_DHCSR, &dhcsr) != SWD_HOST_OK)
    {
      return SWD_HOST_ERROR;
    }
    if ((dhcsr & ARM_DHCSR_S_REGRDY) != 0U)
    {
      return SWD_HOST_OK;
    }
  } while ((HAL_GetTick() - tickstart) < 100U);

  return SWD_HOST_TIMEOUT;
}

/*
 * Run the embedded mini flashloader on the target. The flashloader is the
 * Thumb binary in l0_ob_loader_blob[] (built from flashloader/l0_ob_loader.c).
 * It runs on the L0 CPU itself, which lets it write PEKEYR / OPTKEYR / OB
 * as the native bus master — these writes are accepted under RDP Level 1,
 * unlike MEM-AP debug-master writes.
 *
 * Flow on the target (mirrors ST-Link's flashloader pattern):
 *   1. host halts the core via DHCSR.C_HALT
 *   2. host uploads ~100 bytes of loader code to SRAM 0x20000000
 *   3. host writes ob_word_value to 0x20000100 (loader reads it from there)
 *   4. host sets SP / PC / xPSR via DCRSR+DCRDR
 *   5. host releases C_HALT (resume)
 *   6. CPU unlocks flash, programs OB, sets PECR.OBL_LAUNCH
 *   7. OBL_LAUNCH triggers an internal chip reset; SWD breaks here, which
 *      we detect as a failed DHCSR read — that is the success signal.
 */
static swd_host_status_t stm32l0_run_ob_loader(uint32_t ob_word_value)
{
  uint32_t offset;
  uint32_t word;
  uint32_t tickstart;
  uint32_t dhcsr;

  /* 1. halt the target CPU */
  if (stm32l0_cpu_halt(100U) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 2. upload the loader binary, 4 bytes at a time, little-endian */
  for (offset = 0U; offset < L0_OB_LOADER_BLOB_SIZE; offset += 4U)
  {
    word =  (uint32_t)l0_ob_loader_blob[offset]
         | ((uint32_t)l0_ob_loader_blob[offset + 1U] << 8)
         | ((uint32_t)l0_ob_loader_blob[offset + 2U] << 16)
         | ((uint32_t)l0_ob_loader_blob[offset + 3U] << 24);

    if (swd_host_write_u32(L0_OB_LOADER_LOAD_ADDRESS + offset, word) != SWD_HOST_OK)
    {
      return SWD_HOST_ERROR;
    }
  }

  /* 3. parameter: the 32-bit option byte word the loader will write to OB */
  if (swd_host_write_u32(L0_OB_LOADER_PARAM_ADDRESS, ob_word_value) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 4. set SP, PC, xPSR.T so the core resumes at the loader entry in Thumb mode */
  if (stm32l0_write_core_register(ARM_CORE_REG_SP, L0_OB_LOADER_STACK_TOP) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }
  if (stm32l0_write_core_register(ARM_CORE_REG_PC, L0_OB_LOADER_ENTRY_ADDRESS) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }
  if (stm32l0_write_core_register(ARM_CORE_REG_XPSR, ARM_XPSR_THUMB) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 5. release C_HALT to let the loader run */
  if (swd_host_write_u32(ARM_DHCSR,
                         ARM_DHCSR_DBGKEY | ARM_DHCSR_C_DEBUGEN) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /*
   * 6. wait for either:
   *      - DHCSR read fails  : OBL_LAUNCH-induced chip reset broke SWD (success)
   *      - DHCSR.S_HALT set  : loader hit BKPT (also success, reset just delayed)
   *      - timeout
   */
  tickstart = HAL_GetTick();
  for (;;)
  {
    if (swd_host_read_u32(ARM_DHCSR, &dhcsr) != SWD_HOST_OK)
    {
      return SWD_HOST_OK;
    }
    if ((dhcsr & ARM_DHCSR_S_HALT) != 0U)
    {
      return SWD_HOST_OK;
    }
    if ((HAL_GetTick() - tickstart) > 500U)
    {
      return SWD_HOST_TIMEOUT;
    }
  }
}

static swd_host_status_t stm32g0_reconnect_for_level1_access(void)
{
  swd_host_config_t saved_config = g_swd_host_config;
  swd_host_status_t status;

  g_swd_host_config.connect_mode = SWD_HOST_CONNECT_UNDER_RESET;
  g_swd_host_config.speed = SWD_HOST_SPEED_VERY_LOW;

  status = swd_host_connect();

  g_swd_host_config = saved_config;
  return status;
}

static uint8_t stm32g0_get_rdp_byte(stm32g0_rdp_level_t level)
{
  if (level == STM32G0_RDP_LEVEL_0)
  {
    return STM32G0_OB_RDP_LEVEL0_BYTE;
  }

  if (level == STM32G0_RDP_LEVEL_2)
  {
    return STM32G0_OB_RDP_LEVEL2_BYTE;
  }

  return STM32G0_OB_RDP_LEVEL1_BYTE;
}

/*
 * Run the embedded STM32G0 mini flashloader on the target. The flashloader is
 * the Thumb binary in g0_ob_loader_blob[] (built from flashloader/g0_ob_loader.c).
 * It runs on the G0 CPU itself, which lets it write KEYR / OPTKEYR / OPTR as
 * the native bus master — these writes are accepted under RDP Level 1, unlike
 * MEM-AP debug-master writes. The loader does a read-modify-write of only the
 * RDP byte in OPTR, so every other user option bit is preserved.
 *
 * Flow mirrors stm32l0_run_ob_loader() exactly; only the loader payload and the
 * single parameter differ. The DHCSR halt / core-register helpers are
 * target-agnostic ARM debug operations, so they are shared with the L0 path.
 */
static swd_host_status_t stm32g0_run_ob_loader(uint32_t rdp_byte_value)
{
  uint32_t offset;
  uint32_t word;
  uint32_t tickstart;
  uint32_t dhcsr;

  /* 1. halt the target CPU */
  if (stm32l0_cpu_halt(100U) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 2. upload the loader binary, 4 bytes at a time, little-endian */
  for (offset = 0U; offset < G0_OB_LOADER_BLOB_SIZE; offset += 4U)
  {
    word =  (uint32_t)g0_ob_loader_blob[offset]
         | ((uint32_t)g0_ob_loader_blob[offset + 1U] << 8)
         | ((uint32_t)g0_ob_loader_blob[offset + 2U] << 16)
         | ((uint32_t)g0_ob_loader_blob[offset + 3U] << 24);

    if (swd_host_write_u32(G0_OB_LOADER_LOAD_ADDRESS + offset, word) != SWD_HOST_OK)
    {
      return SWD_HOST_ERROR;
    }
  }

  /* 3. parameter: the desired RDP byte the loader merges into OPTR[7:0] */
  if (swd_host_write_u32(G0_OB_LOADER_PARAM_ADDRESS, rdp_byte_value & STM32G0_OPTR_RDP_MASK) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 4. set SP, PC, xPSR.T so the core resumes at the loader entry in Thumb mode */
  if (stm32l0_write_core_register(ARM_CORE_REG_SP, G0_OB_LOADER_STACK_TOP) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }
  if (stm32l0_write_core_register(ARM_CORE_REG_PC, G0_OB_LOADER_ENTRY_ADDRESS) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }
  if (stm32l0_write_core_register(ARM_CORE_REG_XPSR, ARM_XPSR_THUMB) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 5. release C_HALT to let the loader run */
  if (swd_host_write_u32(ARM_DHCSR,
                         ARM_DHCSR_DBGKEY | ARM_DHCSR_C_DEBUGEN) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /*
   * 6. wait for either:
   *      - DHCSR read fails  : OBL_LAUNCH-induced chip reset broke SWD (success)
   *      - DHCSR.S_HALT set  : loader hit BKPT (also success, reset just delayed)
   *      - timeout
   */
  tickstart = HAL_GetTick();
  for (;;)
  {
    if (swd_host_read_u32(ARM_DHCSR, &dhcsr) != SWD_HOST_OK)
    {
      return SWD_HOST_OK;
    }
    if ((dhcsr & ARM_DHCSR_S_HALT) != 0U)
    {
      return SWD_HOST_OK;
    }
    if ((HAL_GetTick() - tickstart) > 500U)
    {
      return SWD_HOST_TIMEOUT;
    }
  }
}

static swd_host_status_t stm32l1_reconnect_for_level1_access(void)
{
  swd_host_config_t saved_config = g_swd_host_config;
  swd_host_status_t status;

  g_swd_host_config.connect_mode = SWD_HOST_CONNECT_UNDER_RESET;
  g_swd_host_config.speed = SWD_HOST_SPEED_VERY_LOW;

  status = swd_host_connect();

  g_swd_host_config = saved_config;
  return status;
}

static uint8_t stm32l1_get_rdp_byte(stm32l1_rdp_level_t level)
{
  if (level == STM32L1_RDP_LEVEL_0)
  {
    return STM32L1_OB_RDP_LEVEL0_BYTE;
  }

  if (level == STM32L1_RDP_LEVEL_2)
  {
    return STM32L1_OB_RDP_LEVEL2_BYTE;
  }

  return STM32L1_OB_RDP_LEVEL1_BYTE;
}

/*
 * Run the embedded STM32L1 mini flashloader on the target. Identical in flow
 * to stm32l0_run_ob_loader() — only the loader payload (L1 register base) and
 * the OB-word parameter differ. The DHCSR halt / core-register helpers are
 * target-agnostic ARM debug operations, so they are shared with the L0 path.
 */
static swd_host_status_t stm32l1_run_ob_loader(uint32_t ob_word_value)
{
  uint32_t offset;
  uint32_t word;
  uint32_t tickstart;
  uint32_t dhcsr;

  /* 1. halt the target CPU */
  if (stm32l0_cpu_halt(100U) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 2. upload the loader binary, 4 bytes at a time, little-endian */
  for (offset = 0U; offset < L1_OB_LOADER_BLOB_SIZE; offset += 4U)
  {
    word =  (uint32_t)l1_ob_loader_blob[offset]
         | ((uint32_t)l1_ob_loader_blob[offset + 1U] << 8)
         | ((uint32_t)l1_ob_loader_blob[offset + 2U] << 16)
         | ((uint32_t)l1_ob_loader_blob[offset + 3U] << 24);

    if (swd_host_write_u32(L1_OB_LOADER_LOAD_ADDRESS + offset, word) != SWD_HOST_OK)
    {
      return SWD_HOST_ERROR;
    }
  }

  /* 3. parameter: the 32-bit option byte word the loader will write to OB */
  if (swd_host_write_u32(L1_OB_LOADER_PARAM_ADDRESS, ob_word_value) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 4. set SP, PC, xPSR.T so the core resumes at the loader entry in Thumb mode */
  if (stm32l0_write_core_register(ARM_CORE_REG_SP, L1_OB_LOADER_STACK_TOP) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }
  if (stm32l0_write_core_register(ARM_CORE_REG_PC, L1_OB_LOADER_ENTRY_ADDRESS) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }
  if (stm32l0_write_core_register(ARM_CORE_REG_XPSR, ARM_XPSR_THUMB) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 5. release C_HALT to let the loader run */
  if (swd_host_write_u32(ARM_DHCSR,
                         ARM_DHCSR_DBGKEY | ARM_DHCSR_C_DEBUGEN) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 6. wait for OBL_LAUNCH-induced reset (SWD breaks) or loader BKPT (S_HALT) */
  tickstart = HAL_GetTick();
  for (;;)
  {
    if (swd_host_read_u32(ARM_DHCSR, &dhcsr) != SWD_HOST_OK)
    {
      return SWD_HOST_OK;
    }
    if ((dhcsr & ARM_DHCSR_S_HALT) != 0U)
    {
      return SWD_HOST_OK;
    }
    if ((HAL_GetTick() - tickstart) > 500U)
    {
      return SWD_HOST_TIMEOUT;
    }
  }
}

static swd_host_status_t stm32g4_reconnect_for_level1_access(void)
{
  swd_host_config_t saved_config = g_swd_host_config;
  swd_host_status_t status;

  g_swd_host_config.connect_mode = SWD_HOST_CONNECT_UNDER_RESET;
  g_swd_host_config.speed = SWD_HOST_SPEED_VERY_LOW;

  status = swd_host_connect();

  g_swd_host_config = saved_config;
  return status;
}

static uint8_t stm32g4_get_rdp_byte(stm32g4_rdp_level_t level)
{
  if (level == STM32G4_RDP_LEVEL_0)
  {
    return STM32G4_OB_RDP_LEVEL0_BYTE;
  }

  if (level == STM32G4_RDP_LEVEL_2)
  {
    return STM32G4_OB_RDP_LEVEL2_BYTE;
  }

  return STM32G4_OB_RDP_LEVEL1_BYTE;
}

/*
 * Run the embedded STM32G4 mini flashloader on the target. Identical in flow
 * to stm32g0_run_ob_loader() (the flash IP is the same); only the loader
 * payload differs. Does a read-modify-write of the RDP byte in OPTR.
 */
static swd_host_status_t stm32g4_run_ob_loader(uint32_t rdp_byte_value)
{
  uint32_t offset;
  uint32_t word;
  uint32_t tickstart;
  uint32_t dhcsr;

  /* 1. halt the target CPU */
  if (stm32l0_cpu_halt(100U) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 2. upload the loader binary, 4 bytes at a time, little-endian */
  for (offset = 0U; offset < G4_OB_LOADER_BLOB_SIZE; offset += 4U)
  {
    word =  (uint32_t)g4_ob_loader_blob[offset]
         | ((uint32_t)g4_ob_loader_blob[offset + 1U] << 8)
         | ((uint32_t)g4_ob_loader_blob[offset + 2U] << 16)
         | ((uint32_t)g4_ob_loader_blob[offset + 3U] << 24);

    if (swd_host_write_u32(G4_OB_LOADER_LOAD_ADDRESS + offset, word) != SWD_HOST_OK)
    {
      return SWD_HOST_ERROR;
    }
  }

  /* 3. parameter: the desired RDP byte the loader merges into OPTR[7:0] */
  if (swd_host_write_u32(G4_OB_LOADER_PARAM_ADDRESS, rdp_byte_value & STM32G4_OPTR_RDP_MASK) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 4. set SP, PC, xPSR.T so the core resumes at the loader entry in Thumb mode */
  if (stm32l0_write_core_register(ARM_CORE_REG_SP, G4_OB_LOADER_STACK_TOP) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }
  if (stm32l0_write_core_register(ARM_CORE_REG_PC, G4_OB_LOADER_ENTRY_ADDRESS) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }
  if (stm32l0_write_core_register(ARM_CORE_REG_XPSR, ARM_XPSR_THUMB) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 5. release C_HALT to let the loader run */
  if (swd_host_write_u32(ARM_DHCSR,
                         ARM_DHCSR_DBGKEY | ARM_DHCSR_C_DEBUGEN) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 6. wait for OBL_LAUNCH-induced reset (SWD breaks) or loader BKPT (S_HALT) */
  tickstart = HAL_GetTick();
  for (;;)
  {
    if (swd_host_read_u32(ARM_DHCSR, &dhcsr) != SWD_HOST_OK)
    {
      return SWD_HOST_OK;
    }
    if ((dhcsr & ARM_DHCSR_S_HALT) != 0U)
    {
      return SWD_HOST_OK;
    }
    if ((HAL_GetTick() - tickstart) > 500U)
    {
      return SWD_HOST_TIMEOUT;
    }
  }
}

static swd_host_status_t stm32f0_reconnect_for_level1_access(void)
{
  swd_host_config_t saved_config = g_swd_host_config;
  swd_host_status_t status;

  g_swd_host_config.connect_mode = SWD_HOST_CONNECT_UNDER_RESET;
  g_swd_host_config.speed = SWD_HOST_SPEED_VERY_LOW;

  status = swd_host_connect();

  g_swd_host_config = saved_config;
  return status;
}

static uint8_t stm32f0_get_rdp_byte(stm32f0_rdp_level_t level)
{
  if (level == STM32F0_RDP_LEVEL_0)
  {
    return STM32F0_OB_RDP_LEVEL0_BYTE;
  }

  if (level == STM32F0_RDP_LEVEL_2)
  {
    return STM32F0_OB_RDP_LEVEL2_BYTE;
  }

  return STM32F0_OB_RDP_LEVEL1_BYTE;
}

/*
 * Run the embedded STM32F0 mini flashloader on the target. The flashloader is
 * the Thumb binary in f0_ob_loader_blob[] (built from flashloader/f0_ob_loader.c).
 * It runs on the F0 CPU itself, which lets it write KEYR / OPTKEYR / the option
 * bytes as the native bus master — these writes are accepted under RDP Level 1,
 * unlike MEM-AP debug-master writes.
 *
 * The loader erases the option bytes, programs the RDP halfword, then sets
 * FLASH_CR.OBL_LAUNCH to reload the option bytes and self-reset (a plain nRST
 * would not reload them on F0 — only POR or OBL_LAUNCH does). The wait below
 * therefore succeeds on either the OBL_LAUNCH-induced SWD break or the loader
 * BKPT, exactly like the G0 path. The DHCSR halt / core-register helpers are
 * target-agnostic ARM debug operations, so they are shared with the L0 path.
 */
static swd_host_status_t stm32f0_run_ob_loader(uint32_t rdp_byte_value)
{
  uint32_t offset;
  uint32_t word;
  uint32_t tickstart;
  uint32_t dhcsr;

  /* 1. halt the target CPU */
  if (stm32l0_cpu_halt(100U) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 2. upload the loader binary, 4 bytes at a time, little-endian */
  for (offset = 0U; offset < F0_OB_LOADER_BLOB_SIZE; offset += 4U)
  {
    word =  (uint32_t)f0_ob_loader_blob[offset]
         | ((uint32_t)f0_ob_loader_blob[offset + 1U] << 8)
         | ((uint32_t)f0_ob_loader_blob[offset + 2U] << 16)
         | ((uint32_t)f0_ob_loader_blob[offset + 3U] << 24);

    if (swd_host_write_u32(F0_OB_LOADER_LOAD_ADDRESS + offset, word) != SWD_HOST_OK)
    {
      return SWD_HOST_ERROR;
    }
  }

  /* 3. parameter: the desired RDP byte the loader writes to the OB halfword */
  if (swd_host_write_u32(F0_OB_LOADER_PARAM_ADDRESS, rdp_byte_value & 0xFFUL) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 4. set SP, PC, xPSR.T so the core resumes at the loader entry in Thumb mode */
  if (stm32l0_write_core_register(ARM_CORE_REG_SP, F0_OB_LOADER_STACK_TOP) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }
  if (stm32l0_write_core_register(ARM_CORE_REG_PC, F0_OB_LOADER_ENTRY_ADDRESS) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }
  if (stm32l0_write_core_register(ARM_CORE_REG_XPSR, ARM_XPSR_THUMB) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 5. release C_HALT to let the loader run */
  if (swd_host_write_u32(ARM_DHCSR,
                         ARM_DHCSR_DBGKEY | ARM_DHCSR_C_DEBUGEN) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 6. wait for OBL_LAUNCH-induced reset (SWD breaks) or loader BKPT (S_HALT) */
  tickstart = HAL_GetTick();
  for (;;)
  {
    if (swd_host_read_u32(ARM_DHCSR, &dhcsr) != SWD_HOST_OK)
    {
      return SWD_HOST_OK;
    }
    if ((dhcsr & ARM_DHCSR_S_HALT) != 0U)
    {
      return SWD_HOST_OK;
    }
    if ((HAL_GetTick() - tickstart) > 500U)
    {
      return SWD_HOST_TIMEOUT;
    }
  }
}

static swd_host_status_t stm32u0_reconnect_for_level1_access(void)
{
  swd_host_config_t saved_config = g_swd_host_config;
  swd_host_status_t status;

  g_swd_host_config.connect_mode = SWD_HOST_CONNECT_UNDER_RESET;
  g_swd_host_config.speed = SWD_HOST_SPEED_VERY_LOW;

  status = swd_host_connect();

  g_swd_host_config = saved_config;
  return status;
}

static uint8_t stm32u0_get_rdp_byte(stm32u0_rdp_level_t level)
{
  if (level == STM32U0_RDP_LEVEL_0)
  {
    return STM32U0_OB_RDP_LEVEL0_BYTE;
  }

  if (level == STM32U0_RDP_LEVEL_2)
  {
    return STM32U0_OB_RDP_LEVEL2_BYTE;
  }

  return STM32U0_OB_RDP_LEVEL1_BYTE;
}

/*
 * Run the embedded STM32U0 mini flashloader on the target. Identical in flow to
 * stm32g0_run_ob_loader() (the flash IP is the same); only the loader payload
 * differs. The loader read-modify-writes only the RDP byte in OPTR, then
 * triggers OBL_LAUNCH which resets the chip.
 */
static swd_host_status_t stm32u0_run_ob_loader(uint32_t rdp_byte_value)
{
  uint32_t offset;
  uint32_t word;
  uint32_t tickstart;
  uint32_t dhcsr;

  /* 1. halt the target CPU */
  if (stm32l0_cpu_halt(100U) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 2. upload the loader binary, 4 bytes at a time, little-endian */
  for (offset = 0U; offset < U0_OB_LOADER_BLOB_SIZE; offset += 4U)
  {
    word =  (uint32_t)u0_ob_loader_blob[offset]
         | ((uint32_t)u0_ob_loader_blob[offset + 1U] << 8)
         | ((uint32_t)u0_ob_loader_blob[offset + 2U] << 16)
         | ((uint32_t)u0_ob_loader_blob[offset + 3U] << 24);

    if (swd_host_write_u32(U0_OB_LOADER_LOAD_ADDRESS + offset, word) != SWD_HOST_OK)
    {
      return SWD_HOST_ERROR;
    }
  }

  /* 3. parameter: the desired RDP byte the loader merges into OPTR[7:0] */
  if (swd_host_write_u32(U0_OB_LOADER_PARAM_ADDRESS, rdp_byte_value & STM32U0_OPTR_RDP_MASK) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 4. set SP, PC, xPSR.T so the core resumes at the loader entry in Thumb mode */
  if (stm32l0_write_core_register(ARM_CORE_REG_SP, U0_OB_LOADER_STACK_TOP) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }
  if (stm32l0_write_core_register(ARM_CORE_REG_PC, U0_OB_LOADER_ENTRY_ADDRESS) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }
  if (stm32l0_write_core_register(ARM_CORE_REG_XPSR, ARM_XPSR_THUMB) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 5. release C_HALT to let the loader run */
  if (swd_host_write_u32(ARM_DHCSR,
                         ARM_DHCSR_DBGKEY | ARM_DHCSR_C_DEBUGEN) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 6. wait for OBL_LAUNCH-induced reset (SWD breaks) or loader BKPT (S_HALT) */
  tickstart = HAL_GetTick();
  for (;;)
  {
    if (swd_host_read_u32(ARM_DHCSR, &dhcsr) != SWD_HOST_OK)
    {
      return SWD_HOST_OK;
    }
    if ((dhcsr & ARM_DHCSR_S_HALT) != 0U)
    {
      return SWD_HOST_OK;
    }
    if ((HAL_GetTick() - tickstart) > 500U)
    {
      return SWD_HOST_TIMEOUT;
    }
  }
}

static swd_host_status_t stm32l4_reconnect_for_level1_access(void)
{
  swd_host_config_t saved_config = g_swd_host_config;
  swd_host_status_t status;

  g_swd_host_config.connect_mode = SWD_HOST_CONNECT_UNDER_RESET;
  g_swd_host_config.speed = SWD_HOST_SPEED_VERY_LOW;

  status = swd_host_connect();

  g_swd_host_config = saved_config;
  return status;
}

static uint8_t stm32l4_get_rdp_byte(stm32l4_rdp_level_t level)
{
  if (level == STM32L4_RDP_LEVEL_0)
  {
    return STM32L4_OB_RDP_LEVEL0_BYTE;
  }

  if (level == STM32L4_RDP_LEVEL_2)
  {
    return STM32L4_OB_RDP_LEVEL2_BYTE;
  }

  return STM32L4_OB_RDP_LEVEL1_BYTE;
}

/*
 * Run the embedded STM32L4 mini flashloader on the target. Identical in flow to
 * stm32g4_run_ob_loader() (the flash IP is the same); only the loader payload
 * differs. Does a read-modify-write of the RDP byte in OPTR, then OBL_LAUNCH.
 */
static swd_host_status_t stm32l4_run_ob_loader(uint32_t rdp_byte_value)
{
  uint32_t offset;
  uint32_t word;
  uint32_t tickstart;
  uint32_t dhcsr;

  /* 1. halt the target CPU */
  if (stm32l0_cpu_halt(100U) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 2. upload the loader binary, 4 bytes at a time, little-endian */
  for (offset = 0U; offset < L4_OB_LOADER_BLOB_SIZE; offset += 4U)
  {
    word =  (uint32_t)l4_ob_loader_blob[offset]
         | ((uint32_t)l4_ob_loader_blob[offset + 1U] << 8)
         | ((uint32_t)l4_ob_loader_blob[offset + 2U] << 16)
         | ((uint32_t)l4_ob_loader_blob[offset + 3U] << 24);

    if (swd_host_write_u32(L4_OB_LOADER_LOAD_ADDRESS + offset, word) != SWD_HOST_OK)
    {
      return SWD_HOST_ERROR;
    }
  }

  /* 3. parameter: the desired RDP byte the loader merges into OPTR[7:0] */
  if (swd_host_write_u32(L4_OB_LOADER_PARAM_ADDRESS, rdp_byte_value & STM32L4_OPTR_RDP_MASK) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 4. set SP, PC, xPSR.T so the core resumes at the loader entry in Thumb mode */
  if (stm32l0_write_core_register(ARM_CORE_REG_SP, L4_OB_LOADER_STACK_TOP) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }
  if (stm32l0_write_core_register(ARM_CORE_REG_PC, L4_OB_LOADER_ENTRY_ADDRESS) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }
  if (stm32l0_write_core_register(ARM_CORE_REG_XPSR, ARM_XPSR_THUMB) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 5. release C_HALT to let the loader run */
  if (swd_host_write_u32(ARM_DHCSR,
                         ARM_DHCSR_DBGKEY | ARM_DHCSR_C_DEBUGEN) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  /* 6. wait for OBL_LAUNCH-induced reset (SWD breaks) or loader BKPT (S_HALT) */
  tickstart = HAL_GetTick();
  for (;;)
  {
    if (swd_host_read_u32(ARM_DHCSR, &dhcsr) != SWD_HOST_OK)
    {
      return SWD_HOST_OK;
    }
    if ((dhcsr & ARM_DHCSR_S_HALT) != 0U)
    {
      return SWD_HOST_OK;
    }
    if ((HAL_GetTick() - tickstart) > 500U)
    {
      return SWD_HOST_TIMEOUT;
    }
  }
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

swd_host_status_t stm32l0_read_u32(uint32_t address, uint32_t *value)
{
  swd_host_status_t status = swd_host_connect();

  if (status != SWD_HOST_OK)
  {
    return status;
  }

  return swd_host_read_u32(address, value);
}

swd_host_status_t stm32l0_write_u32(uint32_t address, uint32_t value)
{
  swd_host_status_t status = swd_host_connect();

  if (status != SWD_HOST_OK)
  {
    return status;
  }

  return swd_host_write_u32(address, value);
}

swd_host_status_t stm32l0_get_rdp_level(stm32l0_rdp_level_t *level)
{
  uint32_t obr;
  uint8_t rdp_byte;
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

  /*
   * Read the *active* RDP byte from FLASH_OBR (0x4002201C), not the raw
   * option-byte flash at 0x1FF80000. OBR reflects what the hardware
   * actually loaded into the OB latches after the last system reset, so
   * it is the ground truth — exactly what ST-Link queries in its captures.
   * Reading raw OB flash can show "what we wrote" even when the OB
   * verify failed and the chip is still at the previous RDP level.
   */
  status = swd_host_read_u32(STM32L0_FLASH_OBR, &obr);
  if (status != SWD_HOST_OK)
  {
    /*
     * If even OBR is unreadable, the SWD connection lost AP access; treat
     * as "protected" so the caller can drive the L1 -> L0 recovery flow.
     */
    *level = STM32L0_RDP_LEVEL_1;
    return SWD_HOST_OK;
  }

  rdp_byte = (uint8_t)(obr & 0xFFU);

  if (rdp_byte == STM32L0_OB_RDP_LEVEL0_BYTE)
  {
    *level = STM32L0_RDP_LEVEL_0;
  }
  else if (rdp_byte == STM32L0_OB_RDP_LEVEL2_BYTE)
  {
    *level = STM32L0_RDP_LEVEL_2;
  }
  else
  {
    *level = STM32L0_RDP_LEVEL_1;
  }

  return SWD_HOST_OK;
}

swd_host_status_t stm32l0_set_rdp_level(stm32l0_rdp_level_t level)
{
  swd_host_status_t status;
  stm32l0_rdp_level_t current_level;
  uint8_t rdp_byte;
  uint8_t user_byte = 0x00U;
  uint32_t ob_word;
  uint32_t settle_ms;

  status = stm32l0_get_rdp_level(&current_level);
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  if (current_level == level)
  {
    return SWD_HOST_OK;
  }

  if (level == STM32L0_RDP_LEVEL_2)
  {
    return SWD_HOST_TARGET_LOCKED;
  }

  if (current_level == STM32L0_RDP_LEVEL_1)
  {
    status = stm32l0_reconnect_for_level1_access();
    if (status != SWD_HOST_OK)
    {
      return status;
    }
  }

  /*
   * STM32L0 option byte word at 0x1FF80000 — halfword-complement layout
   * (matches ST HAL's FLASH_OB_RDPConfig):
   *   bits [15:0]  = data = (USER << 8) | RDP
   *   bits [31:16] = ~data (16-bit complement, verified by hardware)
   * Byte-wise complement is WRONG and gets rejected by the OB verify
   * stage at OBL_LAUNCH, leaving the chip stuck at the previous RDP level.
   * USER bits are written as factory default (0x00).
   */
  rdp_byte = stm32l0_get_rdp_byte(level);
  {
    uint16_t data = ((uint16_t)user_byte << 8) | (uint16_t)rdp_byte;
    ob_word = ((uint32_t)(uint16_t)~data << 16) | (uint32_t)data;
  }

  /*
   * Run the embedded mini flashloader on the target. It performs
   *   PEKEYR unlock -> OPTKEYR unlock -> wait BSY -> write OB word -> wait BSY
   *   -> PECR.OBL_LAUNCH = 1
   * all from the L0 CPU itself (native bus master), then the chip resets.
   */
  status = stm32l0_run_ob_loader(ob_word);
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  /*
   * OBL_LAUNCH already triggered the chip's internal NRST_OUT. Wait long
   * enough for OB reload to complete. The L1 -> L0 path additionally
   * mass-erases user flash, so allow significantly more time there.
   */
  settle_ms = (level == STM32L0_RDP_LEVEL_0) ? STM32L0_RDP_LEVEL1_TO_0_RELEASE_DELAY_MS : 100U;
  HAL_Delay(settle_ms);
  swd_host_disconnect();
  return SWD_HOST_OK;
}

swd_host_status_t stm32g0_read_u32(uint32_t address, uint32_t *value)
{
  swd_host_status_t status = swd_host_connect();

  if (status != SWD_HOST_OK)
  {
    return status;
  }

  return swd_host_read_u32(address, value);
}

swd_host_status_t stm32g0_write_u32(uint32_t address, uint32_t value)
{
  swd_host_status_t status = swd_host_connect();

  if (status != SWD_HOST_OK)
  {
    return status;
  }

  return swd_host_write_u32(address, value);
}

swd_host_status_t stm32g0_get_rdp_level(stm32g0_rdp_level_t *level)
{
  uint32_t optr;
  uint8_t rdp_byte;
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

  /*
   * The active RDP level is OPTR[7:0] (0x40022020). OPTR reflects what the
   * hardware loaded from the option bytes at the last system reset, so it is
   * the ground truth. If OPTR is unreadable the SWD connection lost AP access
   * under protection, so report Level 1 to drive the L1 -> L0 recovery flow.
   */
  status = swd_host_read_u32(STM32G0_FLASH_OPTR, &optr);
  if (status != SWD_HOST_OK)
  {
    *level = STM32G0_RDP_LEVEL_1;
    return SWD_HOST_OK;
  }

  rdp_byte = (uint8_t)(optr & STM32G0_OPTR_RDP_MASK);

  if (rdp_byte == STM32G0_OB_RDP_LEVEL0_BYTE)
  {
    *level = STM32G0_RDP_LEVEL_0;
  }
  else if (rdp_byte == STM32G0_OB_RDP_LEVEL2_BYTE)
  {
    *level = STM32G0_RDP_LEVEL_2;
  }
  else
  {
    *level = STM32G0_RDP_LEVEL_1;
  }

  return SWD_HOST_OK;
}

swd_host_status_t stm32g0_set_rdp_level(stm32g0_rdp_level_t level)
{
  swd_host_status_t status;
  stm32g0_rdp_level_t current_level;
  uint8_t rdp_byte;
  uint32_t settle_ms;

  status = stm32g0_get_rdp_level(&current_level);
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  if (current_level == level)
  {
    return SWD_HOST_OK;
  }

  if (level == STM32G0_RDP_LEVEL_2)
  {
    return SWD_HOST_TARGET_LOCKED;
  }

  if (current_level == STM32G0_RDP_LEVEL_1)
  {
    status = stm32g0_reconnect_for_level1_access();
    if (status != SWD_HOST_OK)
    {
      return status;
    }
  }

  /*
   * Run the embedded mini flashloader on the target. It performs
   *   wait BSY -> KEYR unlock -> OPTKEYR unlock -> read-modify-write OPTR[7:0]
   *   -> CR.OPTSTRT -> wait BSY -> CR.OBL_LAUNCH
   * all from the G0 CPU itself (native bus master), then the chip resets.
   * Only the RDP byte is changed; all other option bits are preserved.
   */
  rdp_byte = stm32g0_get_rdp_byte(level);
  status = stm32g0_run_ob_loader((uint32_t)rdp_byte);
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  /*
   * OBL_LAUNCH already triggered the chip's internal NRST_OUT. Wait long
   * enough for OB reload to complete. The L1 -> L0 path additionally
   * mass-erases user flash, so allow significantly more time there.
   */
  settle_ms = (level == STM32G0_RDP_LEVEL_0) ? STM32G0_RDP_LEVEL1_TO_0_RELEASE_DELAY_MS : 100U;
  HAL_Delay(settle_ms);
  swd_host_disconnect();
  return SWD_HOST_OK;
}

swd_host_status_t stm32l1_read_u32(uint32_t address, uint32_t *value)
{
  swd_host_status_t status = swd_host_connect();

  if (status != SWD_HOST_OK)
  {
    return status;
  }

  return swd_host_read_u32(address, value);
}

swd_host_status_t stm32l1_write_u32(uint32_t address, uint32_t value)
{
  swd_host_status_t status = swd_host_connect();

  if (status != SWD_HOST_OK)
  {
    return status;
  }

  return swd_host_write_u32(address, value);
}

swd_host_status_t stm32l1_get_rdp_level(stm32l1_rdp_level_t *level)
{
  uint32_t obr;
  uint8_t rdp_byte;
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

  /*
   * Read the active RDP byte from FLASH_OBR (0x40023C1C) — the loaded option
   * latches, i.e. the ground truth. If OBR is unreadable the SWD connection
   * lost AP access under protection, so report Level 1 to drive recovery.
   */
  status = swd_host_read_u32(STM32L1_FLASH_OBR, &obr);
  if (status != SWD_HOST_OK)
  {
    *level = STM32L1_RDP_LEVEL_1;
    return SWD_HOST_OK;
  }

  rdp_byte = (uint8_t)(obr & 0xFFU);

  if (rdp_byte == STM32L1_OB_RDP_LEVEL0_BYTE)
  {
    *level = STM32L1_RDP_LEVEL_0;
  }
  else if (rdp_byte == STM32L1_OB_RDP_LEVEL2_BYTE)
  {
    *level = STM32L1_RDP_LEVEL_2;
  }
  else
  {
    *level = STM32L1_RDP_LEVEL_1;
  }

  return SWD_HOST_OK;
}

swd_host_status_t stm32l1_set_rdp_level(stm32l1_rdp_level_t level)
{
  swd_host_status_t status;
  stm32l1_rdp_level_t current_level;
  uint8_t rdp_byte;
  uint8_t user_byte = 0x00U;
  uint32_t ob_word;
  uint32_t settle_ms;

  status = stm32l1_get_rdp_level(&current_level);
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  if (current_level == level)
  {
    return SWD_HOST_OK;
  }

  if (level == STM32L1_RDP_LEVEL_2)
  {
    return SWD_HOST_TARGET_LOCKED;
  }

  if (current_level == STM32L1_RDP_LEVEL_1)
  {
    status = stm32l1_reconnect_for_level1_access();
    if (status != SWD_HOST_OK)
    {
      return status;
    }
  }

  /*
   * STM32L1 option byte word at 0x1FF80000 — 16-bit halfword-complement
   * layout, identical to L0 and to ST HAL FLASH_OB_RDPConfig:
   *   bits [15:0]  = data = (USER << 8) | RDP   (USER = factory default 0x00)
   *   bits [31:16] = ~data (16-bit complement, verified by hardware)
   */
  rdp_byte = stm32l1_get_rdp_byte(level);
  {
    uint16_t data = ((uint16_t)user_byte << 8) | (uint16_t)rdp_byte;
    ob_word = ((uint32_t)(uint16_t)~data << 16) | (uint32_t)data;
  }

  status = stm32l1_run_ob_loader(ob_word);
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  settle_ms = (level == STM32L1_RDP_LEVEL_0) ? STM32L1_RDP_LEVEL1_TO_0_RELEASE_DELAY_MS : 100U;
  HAL_Delay(settle_ms);
  swd_host_disconnect();
  return SWD_HOST_OK;
}

swd_host_status_t stm32g4_read_u32(uint32_t address, uint32_t *value)
{
  swd_host_status_t status = swd_host_connect();

  if (status != SWD_HOST_OK)
  {
    return status;
  }

  return swd_host_read_u32(address, value);
}

swd_host_status_t stm32g4_write_u32(uint32_t address, uint32_t value)
{
  swd_host_status_t status = swd_host_connect();

  if (status != SWD_HOST_OK)
  {
    return status;
  }

  return swd_host_write_u32(address, value);
}

swd_host_status_t stm32g4_get_rdp_level(stm32g4_rdp_level_t *level)
{
  uint32_t optr;
  uint8_t rdp_byte;
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

  /*
   * The active RDP level is OPTR[7:0] (0x40022020), identical to G0. If OPTR
   * is unreadable the SWD connection lost AP access under protection, so
   * report Level 1 to drive the L1 -> L0 recovery flow.
   */
  status = swd_host_read_u32(STM32G4_FLASH_OPTR, &optr);
  if (status != SWD_HOST_OK)
  {
    *level = STM32G4_RDP_LEVEL_1;
    return SWD_HOST_OK;
  }

  rdp_byte = (uint8_t)(optr & STM32G4_OPTR_RDP_MASK);

  if (rdp_byte == STM32G4_OB_RDP_LEVEL0_BYTE)
  {
    *level = STM32G4_RDP_LEVEL_0;
  }
  else if (rdp_byte == STM32G4_OB_RDP_LEVEL2_BYTE)
  {
    *level = STM32G4_RDP_LEVEL_2;
  }
  else
  {
    *level = STM32G4_RDP_LEVEL_1;
  }

  return SWD_HOST_OK;
}

swd_host_status_t stm32g4_set_rdp_level(stm32g4_rdp_level_t level)
{
  swd_host_status_t status;
  stm32g4_rdp_level_t current_level;
  uint8_t rdp_byte;
  uint32_t settle_ms;

  status = stm32g4_get_rdp_level(&current_level);
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  if (current_level == level)
  {
    return SWD_HOST_OK;
  }

  if (level == STM32G4_RDP_LEVEL_2)
  {
    return SWD_HOST_TARGET_LOCKED;
  }

  if (current_level == STM32G4_RDP_LEVEL_1)
  {
    status = stm32g4_reconnect_for_level1_access();
    if (status != SWD_HOST_OK)
    {
      return status;
    }
  }

  /*
   * Run the embedded mini flashloader on the target (same flow as G0). Only
   * the RDP byte in OPTR is changed; all other option bits are preserved.
   */
  rdp_byte = stm32g4_get_rdp_byte(level);
  status = stm32g4_run_ob_loader((uint32_t)rdp_byte);
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  settle_ms = (level == STM32G4_RDP_LEVEL_0) ? STM32G4_RDP_LEVEL1_TO_0_RELEASE_DELAY_MS : 100U;
  HAL_Delay(settle_ms);
  swd_host_disconnect();
  return SWD_HOST_OK;
}

swd_host_status_t stm32f0_read_u32(uint32_t address, uint32_t *value)
{
  swd_host_status_t status = swd_host_connect();

  if (status != SWD_HOST_OK)
  {
    return status;
  }

  return swd_host_read_u32(address, value);
}

swd_host_status_t stm32f0_write_u32(uint32_t address, uint32_t value)
{
  swd_host_status_t status = swd_host_connect();

  if (status != SWD_HOST_OK)
  {
    return status;
  }

  return swd_host_write_u32(address, value);
}

swd_host_status_t stm32f0_get_rdp_level(stm32f0_rdp_level_t *level)
{
  uint32_t obr;
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

  /*
   * Read the active RDP level from FLASH_OBR (0x4002201C) — the loaded option
   * latches, i.e. the ground truth, exactly like the L0/G0 paths. The RDP
   * level is encoded by the RDPRT1/RDPRT2 bits (not a byte): RDPRT2 -> Level 2,
   * RDPRT1 -> Level 1, neither -> Level 0. If OBR is unreadable the SWD
   * connection lost AP access under protection, so report Level 1 to drive the
   * L1 -> L0 recovery flow.
   */
  status = swd_host_read_u32(STM32F0_FLASH_OBR, &obr);
  if (status != SWD_HOST_OK)
  {
    *level = STM32F0_RDP_LEVEL_1;
    return SWD_HOST_OK;
  }

  if ((obr & STM32F0_OBR_RDPRT2) != 0U)
  {
    *level = STM32F0_RDP_LEVEL_2;
  }
  else if ((obr & STM32F0_OBR_RDPRT1) != 0U)
  {
    *level = STM32F0_RDP_LEVEL_1;
  }
  else
  {
    *level = STM32F0_RDP_LEVEL_0;
  }

  return SWD_HOST_OK;
}

swd_host_status_t stm32f0_set_rdp_level(stm32f0_rdp_level_t level)
{
  swd_host_status_t status;
  stm32f0_rdp_level_t current_level;
  uint8_t rdp_byte;
  uint32_t settle_ms;

  status = stm32f0_get_rdp_level(&current_level);
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  if (current_level == level)
  {
    return SWD_HOST_OK;
  }

  if (level == STM32F0_RDP_LEVEL_2)
  {
    return SWD_HOST_TARGET_LOCKED;
  }

  if (current_level == STM32F0_RDP_LEVEL_1)
  {
    status = stm32f0_reconnect_for_level1_access();
    if (status != SWD_HOST_OK)
    {
      return status;
    }
  }

  /*
   * Run the embedded mini flashloader on the target. It erases the option
   * bytes, programs the new RDP byte, and sets FLASH_CR.OBL_LAUNCH from the F0
   * CPU itself (native bus master) to reload the option bytes and self-reset.
   * Using OBL_LAUNCH is required on F0: a plain nRST would leave the new RDP
   * level unapplied until a manual power-on reset.
   */
  rdp_byte = stm32f0_get_rdp_byte(level);
  status = stm32f0_run_ob_loader((uint32_t)rdp_byte);
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  /*
   * OBL_LAUNCH already triggered the chip's internal reset and OB reload. Wait
   * long enough for it to complete. A Level 1 -> Level 0 regression additionally
   * mass-erases user flash, so allow significantly more time there.
   */
  settle_ms = (level == STM32F0_RDP_LEVEL_0) ? STM32F0_RDP_LEVEL1_TO_0_RELEASE_DELAY_MS : 100U;
  HAL_Delay(settle_ms);
  swd_host_disconnect();
  return SWD_HOST_OK;
}

swd_host_status_t stm32u0_read_u32(uint32_t address, uint32_t *value)
{
  swd_host_status_t status = swd_host_connect();

  if (status != SWD_HOST_OK)
  {
    return status;
  }

  return swd_host_read_u32(address, value);
}

swd_host_status_t stm32u0_write_u32(uint32_t address, uint32_t value)
{
  swd_host_status_t status = swd_host_connect();

  if (status != SWD_HOST_OK)
  {
    return status;
  }

  return swd_host_write_u32(address, value);
}

swd_host_status_t stm32u0_get_rdp_level(stm32u0_rdp_level_t *level)
{
  uint32_t optr;
  uint8_t rdp_byte;
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

  /*
   * The active RDP level is OPTR[7:0] (0x40022020), identical to G0. If OPTR
   * is unreadable the SWD connection lost AP access under protection, so
   * report Level 1 to drive the L1 -> L0 recovery flow.
   */
  status = swd_host_read_u32(STM32U0_FLASH_OPTR, &optr);
  if (status != SWD_HOST_OK)
  {
    *level = STM32U0_RDP_LEVEL_1;
    return SWD_HOST_OK;
  }

  rdp_byte = (uint8_t)(optr & STM32U0_OPTR_RDP_MASK);

  if (rdp_byte == STM32U0_OB_RDP_LEVEL0_BYTE)
  {
    *level = STM32U0_RDP_LEVEL_0;
  }
  else if (rdp_byte == STM32U0_OB_RDP_LEVEL2_BYTE)
  {
    *level = STM32U0_RDP_LEVEL_2;
  }
  else
  {
    *level = STM32U0_RDP_LEVEL_1;
  }

  return SWD_HOST_OK;
}

swd_host_status_t stm32u0_set_rdp_level(stm32u0_rdp_level_t level)
{
  swd_host_status_t status;
  stm32u0_rdp_level_t current_level;
  uint8_t rdp_byte;
  uint32_t settle_ms;

  status = stm32u0_get_rdp_level(&current_level);
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  if (current_level == level)
  {
    return SWD_HOST_OK;
  }

  if (level == STM32U0_RDP_LEVEL_2)
  {
    return SWD_HOST_TARGET_LOCKED;
  }

  if (current_level == STM32U0_RDP_LEVEL_1)
  {
    status = stm32u0_reconnect_for_level1_access();
    if (status != SWD_HOST_OK)
    {
      return status;
    }
  }

  /*
   * Run the embedded mini flashloader on the target (same flow as G0). Only
   * the RDP byte in OPTR is changed; all other option bits are preserved.
   */
  rdp_byte = stm32u0_get_rdp_byte(level);
  status = stm32u0_run_ob_loader((uint32_t)rdp_byte);
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  settle_ms = (level == STM32U0_RDP_LEVEL_0) ? STM32U0_RDP_LEVEL1_TO_0_RELEASE_DELAY_MS : 100U;
  HAL_Delay(settle_ms);
  swd_host_disconnect();
  return SWD_HOST_OK;
}

swd_host_status_t stm32l4_read_u32(uint32_t address, uint32_t *value)
{
  swd_host_status_t status = swd_host_connect();

  if (status != SWD_HOST_OK)
  {
    return status;
  }

  return swd_host_read_u32(address, value);
}

swd_host_status_t stm32l4_write_u32(uint32_t address, uint32_t value)
{
  swd_host_status_t status = swd_host_connect();

  if (status != SWD_HOST_OK)
  {
    return status;
  }

  return swd_host_write_u32(address, value);
}

swd_host_status_t stm32l4_get_rdp_level(stm32l4_rdp_level_t *level)
{
  uint32_t optr;
  uint8_t rdp_byte;
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

  /*
   * The active RDP level is OPTR[7:0] (0x40022020), identical to G0/G4. If
   * OPTR is unreadable the SWD connection lost AP access under protection, so
   * report Level 1 to drive the L1 -> L0 recovery flow.
   */
  status = swd_host_read_u32(STM32L4_FLASH_OPTR, &optr);
  if (status != SWD_HOST_OK)
  {
    *level = STM32L4_RDP_LEVEL_1;
    return SWD_HOST_OK;
  }

  rdp_byte = (uint8_t)(optr & STM32L4_OPTR_RDP_MASK);

  if (rdp_byte == STM32L4_OB_RDP_LEVEL0_BYTE)
  {
    *level = STM32L4_RDP_LEVEL_0;
  }
  else if (rdp_byte == STM32L4_OB_RDP_LEVEL2_BYTE)
  {
    *level = STM32L4_RDP_LEVEL_2;
  }
  else
  {
    *level = STM32L4_RDP_LEVEL_1;
  }

  return SWD_HOST_OK;
}

swd_host_status_t stm32l4_set_rdp_level(stm32l4_rdp_level_t level)
{
  swd_host_status_t status;
  stm32l4_rdp_level_t current_level;
  uint8_t rdp_byte;
  uint32_t settle_ms;

  status = stm32l4_get_rdp_level(&current_level);
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  if (current_level == level)
  {
    return SWD_HOST_OK;
  }

  if (level == STM32L4_RDP_LEVEL_2)
  {
    return SWD_HOST_TARGET_LOCKED;
  }

  if (current_level == STM32L4_RDP_LEVEL_1)
  {
    status = stm32l4_reconnect_for_level1_access();
    if (status != SWD_HOST_OK)
    {
      return status;
    }
  }

  /*
   * Run the embedded mini flashloader on the target (same flow as G4). Only
   * the RDP byte in OPTR is changed; all other option bits are preserved.
   */
  rdp_byte = stm32l4_get_rdp_byte(level);
  status = stm32l4_run_ob_loader((uint32_t)rdp_byte);
  if (status != SWD_HOST_OK)
  {
    return status;
  }

  settle_ms = (level == STM32L4_RDP_LEVEL_0) ? STM32L4_RDP_LEVEL1_TO_0_RELEASE_DELAY_MS : 100U;
  HAL_Delay(settle_ms);
  swd_host_disconnect();
  return SWD_HOST_OK;
}
