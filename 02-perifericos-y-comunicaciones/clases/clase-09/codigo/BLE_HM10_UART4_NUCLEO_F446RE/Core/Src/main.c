/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Programa principal para implementar una comunicación BLE
  *                   con STM32F446RE mediante un módulo HM-10.
  *
  *                   Este firmware utiliza USART2 para la comunicación con la PC
  *                   mediante una terminal serie, y UART4 para el intercambio de
  *                   datos con el módulo HM-10. El HM-10 actúa como puente entre
  *                   Bluetooth Low Energy y UART, permitiendo recibir comandos
  *                   desde un smartphone y procesarlos en el STM32.
  *
  *                   Los comandos recibidos permiten controlar el LED LD2,
  *                   consultar su estado y enviar respuestas tanto al smartphone
  *                   como a la terminal de depuración. Además, desde la PC se
  *                   pueden reenviar comandos AT al HM-10 para verificar o
  *                   configurar el módulo.
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
#include <string.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PC_RX_SIZE        64U
#define BLE_RX_SIZE       64U
#define TX_MSG_SIZE       128U

#define ADC_MAX_VALUE     4095U
#define ADC_VREF_MV       3300U


#define HM10_AT_EOL       "\r\n"
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart4;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static uint8_t pc_rx_buffer[PC_RX_SIZE];
static uint8_t ble_rx_buffer[BLE_RX_SIZE];

static char pc_command[PC_RX_SIZE];
static char ble_command[BLE_RX_SIZE];

static volatile uint8_t pc_command_ready = 0U;
static volatile uint8_t ble_command_ready = 0U;

static volatile uint8_t hm10_at_response_pending = 0U;

static char tx_msg[TX_MSG_SIZE];

static uint8_t led_state = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_UART4_Init(void);
/* USER CODE BEGIN PFP */
static void PC_Print(const char *msg);
static void BLE_Send(const char *msg);
static void HM10_SendAT(const char *cmd);

static void ProcessPcCommand(const char *cmd);
static void ProcessBleCommand(const char *cmd);
static void ProcessAppCommand(const char *cmd, uint8_t answer_to_ble);

static void TrimLine(char *str);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void PC_Print(const char *msg)
{
    HAL_UART_Transmit(&huart2,
                      (uint8_t *)msg,
                      strlen(msg),
                      HAL_MAX_DELAY);
}

static void BLE_Send(const char *msg)
{
    HAL_UART_Transmit(&huart4,
                      (uint8_t *)msg,
                      strlen(msg),
                      HAL_MAX_DELAY);
}

static void HM10_SendAT(const char *cmd)
{
    /*
     * Este modulo HM-10 compatible responde correctamente
     * cuando los comandos AT terminan con LF: '\n'.
     */
    BLE_Send(cmd);
    BLE_Send(HM10_AT_EOL);
}

static void TrimLine(char *str)
{
    size_t len = strlen(str);

    while ((len > 0U) &&
           ((str[len - 1U] == '\r') ||
            (str[len - 1U] == '\n') ||
            (str[len - 1U] == ' ')))
    {
        str[len - 1U] = '\0';
        len--;
    }
}


//Esta función se ejecuta cuando llega información por USART2 o por UART4.
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART2)
    {
        if (Size >= PC_RX_SIZE)
        {
            Size = PC_RX_SIZE - 1U;
        }

        memcpy(pc_command, pc_rx_buffer, Size);
        pc_command[Size] = '\0';
        TrimLine(pc_command);

        pc_command_ready = 1U;

        HAL_UARTEx_ReceiveToIdle_IT(&huart2,
                                    pc_rx_buffer,
                                    PC_RX_SIZE);
    }

    else if (huart->Instance == UART4)
    {
        if (Size >= BLE_RX_SIZE)
        {
            Size = BLE_RX_SIZE - 1U;
        }

        memcpy(ble_command, ble_rx_buffer, Size);
        ble_command[Size] = '\0';
        TrimLine(ble_command);

        ble_command_ready = 1U;

        HAL_UARTEx_ReceiveToIdle_IT(&huart4,
                                    ble_rx_buffer,
                                    BLE_RX_SIZE);
    }
}
//Con esto, desde Cutecom podremos mandar comandos AT al HM-10 y también probar localmente el parser del STM32
static void ProcessPcCommand(const char *cmd)
{
    if (strlen(cmd) == 0U)
    {
        return;
    }

    PC_Print("[PC] ");
    PC_Print(cmd);
    PC_Print("\r\n");

    if (strcmp(cmd, "HELP") == 0)
    {
    	PC_Print("\r\nComandos disponibles desde Cutecom:\r\n");
    	PC_Print("HELP\r\n");
    	PC_Print("AT\r\n");
    	PC_Print("AT+NAME?\r\n");
    	PC_Print("AT+BAUD?\r\n");
    	PC_Print("AT+ROLE?\r\n");
    	PC_Print("AT+RESET\r\n");
    	PC_Print("LOCAL LED_ON\r\n");
    	PC_Print("LOCAL LED_OFF\r\n");
    	PC_Print("LOCAL LED_TOGGLE\r\n");
    	PC_Print("LOCAL STATUS\r\n");
    	PC_Print("LOCAL ADC?\r\n");
    	PC_Print("BLE mensaje\r\n");
    	PC_Print("\r\n");
    }

    else if (strncmp(cmd, "AT", 2U) == 0)
    {
        PC_Print("[PC -> HM10 AT] ");
        PC_Print(cmd);
        PC_Print(" + CRLF\r\n");

        hm10_at_response_pending = 1U;
        HM10_SendAT(cmd);
    }
    else if (strncmp(cmd, "AT", 2U) == 0)
    {
        PC_Print("[PC -> HM10] ");
        PC_Print(cmd);
        PC_Print("\r\n");

        /*
         * En muchos HM-10 el comando AT se envia sin CR/LF.
         * Si tu modulo no responde, probar enviando tambien "\r\n".
         */
        BLE_Send(cmd);
    }

    else if (strncmp(cmd, "LOCAL ", 6U) == 0)
    {
        ProcessAppCommand(&cmd[6], 0U);
    }

    else if (strncmp(cmd, "BLE ", 4U) == 0)
    {
        PC_Print("[PC -> BLE] ");
        PC_Print(&cmd[4]);
        PC_Print("\r\n");

        BLE_Send(&cmd[4]);
        BLE_Send("\r\n");
    }

    else
    {
        PC_Print("Comando de PC no reconocido. Escriba HELP.\r\n");
    }
}
// Esta función procesa los datos recibidos desde HM-10
static void ProcessBleCommand(const char *cmd)
{
	if (strlen(cmd) == 0U)
	    {
	        return;
	    }

	    /*
	     * Si acabamos de enviar un comando AT al HM-10,
	     * mostramos la respuesta en Cutecom y liberamos el modo AT.
	     *
	     * En este modulo compatible algunas respuestas no incluyen OK,
	     * por ejemplo: +VERSION=... o +BAUD=4.
	     */
	    if (hm10_at_response_pending != 0U)
	    {
	        PC_Print("[HM10 AT -> PC] ");
	        PC_Print(cmd);
	        PC_Print("\r\n");

	        hm10_at_response_pending = 0U;

	        return;
	    }

	    /*
	     * Si no estamos esperando una respuesta AT,
	     * tratamos el dato como comando recibido desde el smartphone.
	     */
	    PC_Print("[HM10 -> STM32] ");
	    PC_Print(cmd);
	    PC_Print("\r\n");

	    ProcessAppCommand(cmd, 1U);
}
//parser de comandos de aplicación
static void ProcessAppCommand(const char *cmd, uint8_t answer_to_ble)
{
    if (strcmp(cmd, "LED_ON") == 0)
    {
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
        led_state = 1U;

        snprintf(tx_msg, sizeof(tx_msg), "OK LED_ON\r\n");
    }

    else if (strcmp(cmd, "LED_OFF") == 0)
    {
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
        led_state = 0U;

        snprintf(tx_msg, sizeof(tx_msg), "OK LED_OFF\r\n");
    }

    else if (strcmp(cmd, "LED_TOGGLE") == 0)
    {
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        led_state = !led_state;

        snprintf(tx_msg, sizeof(tx_msg), "OK LED_TOGGLE\r\n");
    }

    else if (strcmp(cmd, "STATUS") == 0)
    {
        snprintf(tx_msg,
                 sizeof(tx_msg),
                 "STATUS LED=%s\r\n",
                 (led_state != 0U) ? "ON" : "OFF");
    }

    else if (strcmp(cmd, "ADC?") == 0)
    {
        /*uint32_t adc_raw = ADC_ReadRaw();
        uint32_t adc_mv = (adc_raw * ADC_VREF_MV) / ADC_MAX_VALUE;

        snprintf(tx_msg,
                 sizeof(tx_msg),
                 "ADC=%lu V=%lu.%03lu\r\n",
                 adc_raw,
                 adc_mv / 1000U,
                 adc_mv % 1000U);*/
    	snprintf(tx_msg,
    	             sizeof(tx_msg),
    	             "ADC_SIM=2048 V=1.650\r\n");
    }

    else
    {
        snprintf(tx_msg,
                 sizeof(tx_msg),
                 "ERROR comando no reconocido: %s\r\n",
                 cmd);
    }

    PC_Print("[APP] ");
    PC_Print(tx_msg);

    if (answer_to_ble != 0U)
    {
        BLE_Send(tx_msg);
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
  MX_USART2_UART_Init();
  MX_UART4_Init();
  /* USER CODE BEGIN 2 */
  PC_Print("\r\n====================================\r\n");
  PC_Print("Practica BLE HM-10 + STM32F446RE\r\n");
  PC_Print("USART2: Cutecom / PC\r\n");
  PC_Print("UART4 : HM-10\r\n");
  PC_Print("====================================\r\n");

  PC_Print("Escriba HELP para ver comandos.\r\n\r\n");
  //Se habilitam la recepción por interrupción para UART 2 y 4
  HAL_UARTEx_ReceiveToIdle_IT(&huart2,
                              pc_rx_buffer,
                              PC_RX_SIZE);

  HAL_UARTEx_ReceiveToIdle_IT(&huart4,
                              ble_rx_buffer,
                              BLE_RX_SIZE);


  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if (pc_command_ready != 0U)
	  {
	      pc_command_ready = 0U;

	      ProcessPcCommand(pc_command);

	      memset(pc_command, 0, sizeof(pc_command));
	  }

	  if (ble_command_ready != 0U)
	  {
	      ble_command_ready = 0U;

	      ProcessBleCommand(ble_command);

	      memset(ble_command, 0, sizeof(ble_command));
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
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 9600;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */

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
  if (HAL_UART_Init(&huart2) != HAL_OK)
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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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

#ifdef  USE_FULL_ASSERT
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
