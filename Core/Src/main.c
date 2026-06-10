/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "swd_host.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TARGET_FLASH_READ_ADDRESS   (0x08000000UL)
#define TARGET_FLASH_READ_SIZE      (64U)
#define TARGET_CONNECT_STABILIZE_MS (10U)
#define TARGET_RDP_ACTION_NONE      (0U)
#define TARGET_RDP_ACTION_SET_L1    (1U)
#define TARGET_RDP_ACTION_SET_L0    (2U)
#define TARGET_RDP_ACTION           TARGET_RDP_ACTION_SET_L0

#define TARGET_FAMILY_STM32F1       (0U)
#define TARGET_FAMILY_STM32L0       (1U)
#define TARGET_FAMILY_STM32G0       (2U)
#define TARGET_FAMILY_STM32L1       (3U)
#define TARGET_FAMILY_STM32G4       (4U)
#define TARGET_FAMILY_STM32F0       (5U)
#define TARGET_FAMILY_STM32U0       (6U)
#define TARGET_FAMILY_STM32L4       (7U)
#define TARGET_FAMILY               TARGET_FAMILY_STM32U0

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t flash_data[TARGET_FLASH_READ_SIZE] = {0};
stm32f1_rdp_level_t rdp_level;

static swd_host_status_t STM32F1_ReadBlock(uint32_t address, uint8_t *buffer, uint32_t size)
{
  uint32_t index = 0U;
  uint32_t read_value = 0U;

  if ((buffer == NULL) || ((size % sizeof(uint32_t)) != 0U))
  {
    return SWD_HOST_ERROR;
  }

  for (index = 0U; index < size; index += sizeof(uint32_t))
  {
    if (swd_host_read_u32(address + index, &read_value) != SWD_HOST_OK)
    {
      return SWD_HOST_ERROR;
    }

    buffer[index] = (uint8_t)(read_value & 0xFFU);
    buffer[index + 1U] = (uint8_t)((read_value >> 8) & 0xFFU);
    buffer[index + 2U] = (uint8_t)((read_value >> 16) & 0xFFU);
    buffer[index + 3U] = (uint8_t)((read_value >> 24) & 0xFFU);
  }

  return SWD_HOST_OK;
}

static swd_host_status_t STM32F1_ChangeRdpLevel(uint32_t action)
{
  if (action == TARGET_RDP_ACTION_NONE)
  {
    return SWD_HOST_OK;
  }

  if (stm32f1_get_rdp_level(&rdp_level) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (action == TARGET_RDP_ACTION_SET_L1)
  {
    if (rdp_level != STM32F1_RDP_LEVEL_0)
    {
      return SWD_HOST_ERROR;
    }

    return stm32f1_set_rdp_level(STM32F1_RDP_LEVEL_1);
  }

  if (action == TARGET_RDP_ACTION_SET_L0)
  {
    if (rdp_level != STM32F1_RDP_LEVEL_1)
    {
      return SWD_HOST_ERROR;
    }

    return stm32f1_set_rdp_level(STM32F1_RDP_LEVEL_0);
  }

  return SWD_HOST_ERROR;
}

static void SWD_Test(void)
{
  if (swd_host_connect() != SWD_HOST_OK)
  {
    return;
  }

  HAL_Delay(TARGET_CONNECT_STABILIZE_MS);

  if (stm32f1_get_rdp_level(&rdp_level) != SWD_HOST_OK)
  {
    return;
  }

  if ((rdp_level != STM32F1_RDP_LEVEL_1) &&
      (STM32F1_ReadBlock(TARGET_FLASH_READ_ADDRESS, flash_data, TARGET_FLASH_READ_SIZE) != SWD_HOST_OK))
  {
    return;
  }

  if (STM32F1_ChangeRdpLevel(TARGET_RDP_ACTION) != SWD_HOST_OK)
  {
    return;
  }

}

static swd_host_status_t STM32L0_ChangeRdpLevel(uint32_t action)
{
  stm32l0_rdp_level_t l0_level;

  if (action == TARGET_RDP_ACTION_NONE)
  {
    return SWD_HOST_OK;
  }

  if (stm32l0_get_rdp_level(&l0_level) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (action == TARGET_RDP_ACTION_SET_L1)
  {
    if (l0_level != STM32L0_RDP_LEVEL_0)
    {
      return SWD_HOST_ERROR;
    }

    return stm32l0_set_rdp_level(STM32L0_RDP_LEVEL_1);
  }

  if (action == TARGET_RDP_ACTION_SET_L0)
  {
    if (l0_level != STM32L0_RDP_LEVEL_1)
    {
      return SWD_HOST_ERROR;
    }

    return stm32l0_set_rdp_level(STM32L0_RDP_LEVEL_0);
  }

  return SWD_HOST_ERROR;
}

static void SWD_Test_L0(void)
{
  stm32l0_rdp_level_t l0_level;

  if (swd_host_connect() != SWD_HOST_OK)
  {
    return;
  }

  HAL_Delay(TARGET_CONNECT_STABILIZE_MS);

  if (stm32l0_get_rdp_level(&l0_level) != SWD_HOST_OK)
  {
    return;
  }

  /*
   * STM32F1_ReadBlock is target-agnostic — it only uses swd_host_read_u32 —
   * so it is safe to reuse here for the STM32L0 flash read.
   */
#if 0
  if ((l0_level != STM32L0_RDP_LEVEL_1) &&
      (STM32F1_ReadBlock(TARGET_FLASH_READ_ADDRESS, flash_data, TARGET_FLASH_READ_SIZE) != SWD_HOST_OK))
  {
    return;
  }
#endif

  if (STM32L0_ChangeRdpLevel(TARGET_RDP_ACTION) != SWD_HOST_OK)
  {
    return;
  }
}

static swd_host_status_t STM32G0_ChangeRdpLevel(uint32_t action)
{
  stm32g0_rdp_level_t g0_level;

  if (action == TARGET_RDP_ACTION_NONE)
  {
    return SWD_HOST_OK;
  }

  if (stm32g0_get_rdp_level(&g0_level) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (action == TARGET_RDP_ACTION_SET_L1)
  {
    if (g0_level != STM32G0_RDP_LEVEL_0)
    {
      return SWD_HOST_ERROR;
    }

    return stm32g0_set_rdp_level(STM32G0_RDP_LEVEL_1);
  }

  if (action == TARGET_RDP_ACTION_SET_L0)
  {
    if (g0_level != STM32G0_RDP_LEVEL_1)
    {
      return SWD_HOST_ERROR;
    }

    return stm32g0_set_rdp_level(STM32G0_RDP_LEVEL_0);
  }

  return SWD_HOST_ERROR;
}

static void SWD_Test_G0(void)
{
  stm32g0_rdp_level_t g0_level;

  if (swd_host_connect() != SWD_HOST_OK)
  {
    return;
  }

  HAL_Delay(TARGET_CONNECT_STABILIZE_MS);

  if (stm32g0_get_rdp_level(&g0_level) != SWD_HOST_OK)
  {
    return;
  }

  /*
   * STM32F1_ReadBlock is target-agnostic — it only uses swd_host_read_u32 —
   * so it is safe to reuse here for the STM32G0 flash read.
   */
#if 0
  if ((g0_level != STM32G0_RDP_LEVEL_1) &&
      (STM32F1_ReadBlock(TARGET_FLASH_READ_ADDRESS, flash_data, TARGET_FLASH_READ_SIZE) != SWD_HOST_OK))
  {
    return;
  }
#endif

  if (STM32G0_ChangeRdpLevel(TARGET_RDP_ACTION) != SWD_HOST_OK)
  {
    return;
  }
}

static swd_host_status_t STM32L1_ChangeRdpLevel(uint32_t action)
{
  stm32l1_rdp_level_t l1_level;

  if (action == TARGET_RDP_ACTION_NONE)
  {
    return SWD_HOST_OK;
  }

  if (stm32l1_get_rdp_level(&l1_level) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (action == TARGET_RDP_ACTION_SET_L1)
  {
    if (l1_level != STM32L1_RDP_LEVEL_0)
    {
      return SWD_HOST_ERROR;
    }

    return stm32l1_set_rdp_level(STM32L1_RDP_LEVEL_1);
  }

  if (action == TARGET_RDP_ACTION_SET_L0)
  {
    if (l1_level != STM32L1_RDP_LEVEL_1)
    {
      return SWD_HOST_ERROR;
    }

    return stm32l1_set_rdp_level(STM32L1_RDP_LEVEL_0);
  }

  return SWD_HOST_ERROR;
}

static void SWD_Test_L1(void)
{
  stm32l1_rdp_level_t l1_level;

  if (swd_host_connect() != SWD_HOST_OK)
  {
    return;
  }

  HAL_Delay(TARGET_CONNECT_STABILIZE_MS);

  if (stm32l1_get_rdp_level(&l1_level) != SWD_HOST_OK)
  {
    return;
  }

  /*
   * STM32F1_ReadBlock is target-agnostic — it only uses swd_host_read_u32 —
   * so it is safe to reuse here for the STM32L1 flash read.
   */
#if 0
  if ((l1_level != STM32L1_RDP_LEVEL_1) &&
      (STM32F1_ReadBlock(TARGET_FLASH_READ_ADDRESS, flash_data, TARGET_FLASH_READ_SIZE) != SWD_HOST_OK))
  {
    return;
  }
#endif

  if (STM32L1_ChangeRdpLevel(TARGET_RDP_ACTION) != SWD_HOST_OK)
  {
    return;
  }
}

static swd_host_status_t STM32G4_ChangeRdpLevel(uint32_t action)
{
  stm32g4_rdp_level_t g4_level;

  if (action == TARGET_RDP_ACTION_NONE)
  {
    return SWD_HOST_OK;
  }

  if (stm32g4_get_rdp_level(&g4_level) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (action == TARGET_RDP_ACTION_SET_L1)
  {
    if (g4_level != STM32G4_RDP_LEVEL_0)
    {
      return SWD_HOST_ERROR;
    }

    return stm32g4_set_rdp_level(STM32G4_RDP_LEVEL_1);
  }

  if (action == TARGET_RDP_ACTION_SET_L0)
  {
    if (g4_level != STM32G4_RDP_LEVEL_1)
    {
      return SWD_HOST_ERROR;
    }

    return stm32g4_set_rdp_level(STM32G4_RDP_LEVEL_0);
  }

  return SWD_HOST_ERROR;
}

static void SWD_Test_G4(void)
{
  stm32g4_rdp_level_t g4_level;

  if (swd_host_connect() != SWD_HOST_OK)
  {
    return;
  }

  HAL_Delay(TARGET_CONNECT_STABILIZE_MS);

  if (stm32g4_get_rdp_level(&g4_level) != SWD_HOST_OK)
  {
    return;
  }

  /*
   * STM32F1_ReadBlock is target-agnostic — it only uses swd_host_read_u32 —
   * so it is safe to reuse here for the STM32G4 flash read.
   */
#if 0
  if ((g4_level != STM32G4_RDP_LEVEL_1) &&
      (STM32F1_ReadBlock(TARGET_FLASH_READ_ADDRESS, flash_data, TARGET_FLASH_READ_SIZE) != SWD_HOST_OK))
  {
    return;
  }
#endif

  if (STM32G4_ChangeRdpLevel(TARGET_RDP_ACTION) != SWD_HOST_OK)
  {
    return;
  }
}

static swd_host_status_t STM32F0_ChangeRdpLevel(uint32_t action)
{
  stm32f0_rdp_level_t f0_level;

  if (action == TARGET_RDP_ACTION_NONE)
  {
    return SWD_HOST_OK;
  }

  if (stm32f0_get_rdp_level(&f0_level) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (action == TARGET_RDP_ACTION_SET_L1)
  {
    if (f0_level != STM32F0_RDP_LEVEL_0)
    {
      return SWD_HOST_ERROR;
    }

    return stm32f0_set_rdp_level(STM32F0_RDP_LEVEL_1);
  }

  if (action == TARGET_RDP_ACTION_SET_L0)
  {
    if (f0_level != STM32F0_RDP_LEVEL_1)
    {
      return SWD_HOST_ERROR;
    }

    return stm32f0_set_rdp_level(STM32F0_RDP_LEVEL_0);
  }

  return SWD_HOST_ERROR;
}

static void SWD_Test_F0(void)
{
  stm32f0_rdp_level_t f0_level;

  if (swd_host_connect() != SWD_HOST_OK)
  {
    return;
  }

  HAL_Delay(TARGET_CONNECT_STABILIZE_MS);

  if (stm32f0_get_rdp_level(&f0_level) != SWD_HOST_OK)
  {
    return;
  }

  /*
   * STM32F1_ReadBlock is target-agnostic — it only uses swd_host_read_u32 —
   * so it is safe to reuse here for the STM32F0 flash read.
   */
#if 0
  if ((f0_level != STM32F0_RDP_LEVEL_1) &&
      (STM32F1_ReadBlock(TARGET_FLASH_READ_ADDRESS, flash_data, TARGET_FLASH_READ_SIZE) != SWD_HOST_OK))
  {
    return;
  }
#endif

  if (STM32F0_ChangeRdpLevel(TARGET_RDP_ACTION) != SWD_HOST_OK)
  {
    return;
  }
}

static swd_host_status_t STM32U0_ChangeRdpLevel(uint32_t action)
{
  stm32u0_rdp_level_t u0_level;

  if (action == TARGET_RDP_ACTION_NONE)
  {
    return SWD_HOST_OK;
  }

  if (stm32u0_get_rdp_level(&u0_level) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (action == TARGET_RDP_ACTION_SET_L1)
  {
    if (u0_level != STM32U0_RDP_LEVEL_0)
    {
      return SWD_HOST_ERROR;
    }

    return stm32u0_set_rdp_level(STM32U0_RDP_LEVEL_1);
  }

  if (action == TARGET_RDP_ACTION_SET_L0)
  {
    if (u0_level != STM32U0_RDP_LEVEL_1)
    {
      return SWD_HOST_ERROR;
    }

    return stm32u0_set_rdp_level(STM32U0_RDP_LEVEL_0);
  }

  return SWD_HOST_ERROR;
}

static void SWD_Test_U0(void)
{
  stm32u0_rdp_level_t u0_level;

  if (swd_host_connect() != SWD_HOST_OK)
  {
    return;
  }

  HAL_Delay(TARGET_CONNECT_STABILIZE_MS);

  if (stm32u0_get_rdp_level(&u0_level) != SWD_HOST_OK)
  {
    return;
  }

  /*
   * STM32F1_ReadBlock is target-agnostic — it only uses swd_host_read_u32 —
   * so it is safe to reuse here for the STM32U0 flash read.
   */
#if 0
  if ((u0_level != STM32U0_RDP_LEVEL_1) &&
      (STM32F1_ReadBlock(TARGET_FLASH_READ_ADDRESS, flash_data, TARGET_FLASH_READ_SIZE) != SWD_HOST_OK))
  {
    return;
  }
#endif

  if (STM32U0_ChangeRdpLevel(TARGET_RDP_ACTION) != SWD_HOST_OK)
  {
    return;
  }
}

static swd_host_status_t STM32L4_ChangeRdpLevel(uint32_t action)
{
  stm32l4_rdp_level_t l4_level;

  if (action == TARGET_RDP_ACTION_NONE)
  {
    return SWD_HOST_OK;
  }

  if (stm32l4_get_rdp_level(&l4_level) != SWD_HOST_OK)
  {
    return SWD_HOST_ERROR;
  }

  if (action == TARGET_RDP_ACTION_SET_L1)
  {
    if (l4_level != STM32L4_RDP_LEVEL_0)
    {
      return SWD_HOST_ERROR;
    }

    return stm32l4_set_rdp_level(STM32L4_RDP_LEVEL_1);
  }

  if (action == TARGET_RDP_ACTION_SET_L0)
  {
    if (l4_level != STM32L4_RDP_LEVEL_1)
    {
      return SWD_HOST_ERROR;
    }

    return stm32l4_set_rdp_level(STM32L4_RDP_LEVEL_0);
  }

  return SWD_HOST_ERROR;
}

static void SWD_Test_L4(void)
{
  stm32l4_rdp_level_t l4_level;

  if (swd_host_connect() != SWD_HOST_OK)
  {
    return;
  }

  HAL_Delay(TARGET_CONNECT_STABILIZE_MS);

  if (stm32l4_get_rdp_level(&l4_level) != SWD_HOST_OK)
  {
    return;
  }

  /*
   * STM32F1_ReadBlock is target-agnostic — it only uses swd_host_read_u32 —
   * so it is safe to reuse here for the STM32L4 flash read.
   */
#if 0
  if ((l4_level != STM32L4_RDP_LEVEL_1) &&
      (STM32F1_ReadBlock(TARGET_FLASH_READ_ADDRESS, flash_data, TARGET_FLASH_READ_SIZE) != SWD_HOST_OK))
  {
    return;
  }
#endif

  if (STM32L4_ChangeRdpLevel(TARGET_RDP_ACTION) != SWD_HOST_OK)
  {
    return;
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
/* USER CODE BEGIN 2 */
  swd_host_config_t swd_config =
  {
		  SWD_HOST_CONNECT_NORMAL,
		  SWD_HOST_SPEED_VERY_LOW
  };

  if (swd_host_init_with_config(&swd_config) != SWD_HOST_OK)
  {
    Error_Handler();
  }

#if (TARGET_FAMILY == TARGET_FAMILY_STM32L0)
  SWD_Test_L0();
#elif (TARGET_FAMILY == TARGET_FAMILY_STM32G0)
  SWD_Test_G0();
#elif (TARGET_FAMILY == TARGET_FAMILY_STM32L1)
  SWD_Test_L1();
#elif (TARGET_FAMILY == TARGET_FAMILY_STM32G4)
  SWD_Test_G4();
#elif (TARGET_FAMILY == TARGET_FAMILY_STM32F0)
  SWD_Test_F0();
#elif (TARGET_FAMILY == TARGET_FAMILY_STM32U0)
  SWD_Test_U0();
#elif (TARGET_FAMILY == TARGET_FAMILY_STM32L4)
  SWD_Test_L4();
#else
  SWD_Test();
#endif
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_GREEN_Pin */
  GPIO_InitStruct.Pin = LED_GREEN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LED_GREEN_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

static void SWD_Host_UserExample(void)
{
  uint8_t read_buffer[TARGET_FLASH_READ_SIZE] = {0};
  stm32f1_rdp_level_t rdp_level;

  /*
   * Connection mode selection:
   *   SWD_HOST_CONNECT_NORMAL
   *   SWD_HOST_CONNECT_UNDER_RESET
   *
   * Speed presets:
   *   SWD_HOST_SPEED_VERY_LOW
   *   SWD_HOST_SPEED_LOW
   *   SWD_HOST_SPEED_MEDIUM
   *   SWD_HOST_SPEED_HIGH
   *   SWD_HOST_SPEED_VERY_HIGH
   */
  (void)swd_host_set_connect_mode(SWD_HOST_CONNECT_UNDER_RESET);
  (void)swd_host_set_speed(SWD_HOST_SPEED_VERY_LOW);

  if (swd_host_connect() != SWD_HOST_OK)
  {
    return;
  }

  HAL_Delay(TARGET_CONNECT_STABILIZE_MS);

  if (STM32F1_ReadBlock(TARGET_FLASH_READ_ADDRESS, read_buffer, TARGET_FLASH_READ_SIZE) != SWD_HOST_OK)
  {
    return;
  }

  (void)read_buffer;

  if (stm32f1_get_rdp_level(&rdp_level) != SWD_HOST_OK)
  {
    return;
  }

  (void)rdp_level;

  /*
   * Example write:
   * (void)stm32f1_write_u32(0x20000000UL, 0x12345678UL);
   *
   * RDP Level 0 -> 1:
   * (void)stm32f1_set_rdp_level(STM32F1_RDP_LEVEL_1);
   *
   * RDP Level 1 -> 0:
   * (void)stm32f1_set_rdp_level(STM32F1_RDP_LEVEL_0);
   * Note: STM32F1 returns from Level 1 to Level 0 with a full target flash mass erase.
   */
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
