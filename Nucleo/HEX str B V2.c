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
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define timeSET 10000U
#define PKT_SYNC   0xAA
#define ID_OBC     0x01
#define ID_ANT     0x02
#define BURN_MIN_S  1U
#define BURN_MAX_S  30U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
IWDG_HandleTypeDef hiwdg;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
struct Packet
{
	uint8_t src;
	uint8_t dest;
	uint8_t cmd;
	uint8_t data;
};

uint8_t uartRxByte;
static struct Packet rxPkt; 
//
static volatile int pktReady = 0;
static volatile int statusRequest = 0;
static volatile int deployCommand = 0;
static volatile int EMGCommand = 0;
static volatile int unknowCMD = 0;
static volatile int heaterStarted = 0;
//
#define LINE_MAX 16
static char lineBuf[LINE_MAX];
static uint8_t lineLen = 0;
//
static uint32_t startTime = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_IWDG_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
static void onChar(uint8_t c);
static void sendTM(uint8_t dest, uint8_t cmd, uint8_t data);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void onChar(uint8_t c)
{
	if (c == '\r' || c == '\n')
	{

		if (lineLen == 8 && pktReady == 0)
		{
			unsigned int src, dest, cmd, data;
			lineBuf[8] = '\0';
			if (sscanf(lineBuf, "%2x%2x%2x%2x", &src, &dest, &cmd, &data) == 4)
			{
				rxPkt.src = (uint8_t) src;
				rxPkt.dest = (uint8_t) dest;
				rxPkt.cmd = (uint8_t) cmd;
				rxPkt.data = (uint8_t) data;
				pktReady = 1;
			}
		}
		lineLen = 0;
	}
	else
	{

		if (lineLen < LINE_MAX)
		{
			lineBuf[lineLen] = (char) c;
			lineLen++;
		}
	}
}

static void sendTM(uint8_t dest, uint8_t cmd, uint8_t data)
{
	char out[12];
	int n = snprintf(out, sizeof(out), "%02X%02X%02X%02X\r\n", ID_ANT, dest,
			cmd, data);
	HAL_UART_Transmit(&huart1, (uint8_t*) out, (uint16_t) n, 10);
	HAL_UART_Transmit(&huart2, (uint8_t*) "TX ", 3, 10);
	HAL_UART_Transmit(&huart2, (uint8_t*) out, (uint16_t) n, 10);
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
	MX_USART2_UART_Init();
	MX_IWDG_Init();
	MX_USART1_UART_Init();
	/* USER CODE BEGIN 2 */
	statusRequest = 0;
	deployCommand = 0;
	EMGCommand = 0;
	unknowCMD = 0;
	heaterStarted = 0;
	HAL_Delay(10);
	HAL_UART_Transmit(&huart1, (uint8_t*) "\r\n", 2, 10);
	sendTM(ID_OBC, 0x00, 0x01);    // BOOT
	if (HAL_UART_Receive_IT(&huart1, &uartRxByte, 1) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE END 2 */
	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{

		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
		HAL_IWDG_Refresh(&hiwdg);
		if (pktReady == 1)
		{
			struct Packet p = rxPkt;  
			pktReady = 0;
			char dbg[24];
			int dn = snprintf(dbg, sizeof(dbg), "RX %02X%02X%02X%02X\r\n",
					p.src, p.dest, p.cmd, p.data);
			HAL_UART_Transmit(&huart2, (uint8_t*) dbg, (uint16_t) dn, 10);
			if (p.dest == ID_ANT)
			{
				if (p.src != ID_OBC)
				{

					sendTM(p.src, p.cmd, 0x13);
				}
				else
				{
					switch (p.cmd)
					{
					case 0x3C:
						sendTM(p.src, 0x3C, 0x30);
						statusRequest = 1;

						break;
					case 0xA5:
						if (p.data >= BURN_MIN_S && p.data <= BURN_MAX_S)
						{
							deployCommand = 1;
						}
						else
						{
							sendTM(p.src, 0xA5, 0x13);
						}
						break;
					case 0x5A:
						EMGCommand = 1;
						break;
					default:
						sendTM(p.src, p.cmd, 0xEE);
						unknowCMD = 1;
						break;
					}

				}
			}
		}
		int limitLow = (HAL_GPIO_ReadPin(Limit_GPIO_Port, Limit_Pin) == GPIO_PIN_RESET);


if (deployCommand == 1)
{
    if (heaterStarted == 1)
        sendTM(ID_OBC, 0xA5, 0x11);        // BUSY
    else if (limitLow == 0)
        sendTM(ID_OBC, 0xA5, 0x12);        // ALREADY-DEPLOYED
    else
    {
        startTime = HAL_GetTick();
        HAL_GPIO_WritePin(Heater_GPIO_Port, Heater_Pin, GPIO_PIN_SET);
        heaterStarted = 1;
        sendTM(ID_OBC, 0xA5, 0x10);        // HEATER-ON
    }
    deployCommand = 0;
}

if (limitLow == 0)
{
    HAL_GPIO_WritePin(Heater_GPIO_Port, Heater_Pin, GPIO_PIN_RESET);
    if (heaterStarted == 1)
    {
        heaterStarted = 0;
        sendTM(ID_OBC, 0xA5, 0x20);        // OFF-LIMIT
    }
}


if (heaterStarted == 1 && (HAL_GetTick() - startTime) >= timeSET)
{
    HAL_GPIO_WritePin(Heater_GPIO_Port, Heater_Pin, GPIO_PIN_RESET);
    heaterStarted = 0;
    sendTM(ID_OBC, 0xA5, 0x21);            // OFF-TIMEOUT
}


if (EMGCommand == 1)
{
    HAL_GPIO_WritePin(Heater_GPIO_Port, Heater_Pin, GPIO_PIN_RESET);
    heaterStarted = 0;
    deployCommand = 0;
    EMGCommand = 0;
    sendTM(ID_OBC, 0x5A, 0x22);            // OFF-EMG
}


if (statusRequest == 1)
{
    if (heaterStarted == 1)
        sendTM(ID_OBC, 0x3C, 0x31);        // BURNING
    else if (limitLow == 0)
        sendTM(ID_OBC, 0x3C, 0x32);        // DEPLOYED
    else
        sendTM(ID_OBC, 0x3C, 0x30);        // IDLE
    statusRequest = 0;
}

	}

	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct =
	{ 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct =
	{ 0 };

	/** Configure the main internal regulator output voltage
	 */
	HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI
			| RCC_OSCILLATORTYPE_LSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.LSIState = RCC_LSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
 * @brief IWDG Initialization Function
 * @param None
 * @retval None
 */
static void MX_IWDG_Init(void)
{

	/* USER CODE BEGIN IWDG_Init 0 */

	/* USER CODE END IWDG_Init 0 */

	/* USER CODE BEGIN IWDG_Init 1 */

	/* USER CODE END IWDG_Init 1 */
	hiwdg.Instance = IWDG;
	hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
	hiwdg.Init.Window = 4095;
	hiwdg.Init.Reload = 999;
	if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN IWDG_Init 2 */

	/* USER CODE END IWDG_Init 2 */

}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void)
{

	/* USER CODE BEGIN USART1_Init 0 */

	/* USER CODE END USART1_Init 0 */

	/* USER CODE BEGIN USART1_Init 1 */

	/* USER CODE END USART1_Init 1 */
	huart1.Instance = USART1;
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
	huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart1) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8)
			!= HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8)
			!= HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN USART1_Init 2 */

	/* USER CODE END USART1_Init 2 */

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
	if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8)
			!= HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8)
			!= HAL_OK)
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
	GPIO_InitTypeDef GPIO_InitStruct =
	{ 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */
	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOA_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(Heater_GPIO_Port, Heater_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : Heater_Pin */
	GPIO_InitStruct.Pin = Heater_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(Heater_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : Limit_Pin */
	GPIO_InitStruct.Pin = Limit_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(Limit_GPIO_Port, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */
	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART1)
	{
		onChar(uartRxByte);
		HAL_UART_Receive_IT(&huart1, &uartRxByte, 1);
	}
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART1)
	{
		HAL_UART_Receive_IT(&huart1, &uartRxByte, 1);
	}
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	HAL_GPIO_WritePin(Heater_GPIO_Port, Heater_Pin, GPIO_PIN_RESET);
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
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
