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
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define HCSR04_TRIG_PORT       GPIOA
#define HCSR04_TRIG_PIN        GPIO_PIN_9   /* D8 / PA9 */

#define HCSR04_TIMEOUT_MS      60U
//#define UART_REPORT_PERIOD_MS  300U
#define HCSR04_MEASUREMENT_PERIOD_MS  1000U
#define TIM1_MAX_COUNT         65535UL
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static volatile uint8_t  hcsr04_capture_state = 0U;
static volatile uint8_t  hcsr04_measurement_done = 0U;

static volatile uint32_t hcsr04_rising_edge = 0U;
static volatile uint32_t hcsr04_falling_edge = 0U;
static volatile uint32_t hcsr04_pulse_us = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */
static void HCSR04_DelayUs(uint32_t us);
static void HCSR04_Trigger(void);
static uint8_t HCSR04_ReadDistance_IC(uint32_t *pulse_us, uint32_t *distance_x10_cm);
static void USART2_SendDistance(uint8_t valid, uint32_t pulse_us, uint32_t distance_x10_cm);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void HCSR04_DelayUs(uint32_t us)
{
    uint32_t start_count;

    /*
     * TIM1 está configurado con:
     *
     * f_TIM1 = 84 MHz
     * PSC = 83
     *
     * Por lo tanto:
     * f_CNT = 1 MHz
     * 1 cuenta = 1 us
     */
    start_count = __HAL_TIM_GET_COUNTER(&htim1);

    while ((__HAL_TIM_GET_COUNTER(&htim1) - start_count) < us)
    {
        /* Espera activa */
    }
}


static void HCSR04_Trigger(void)
{
    /*
     * Reiniciar variables de medición.
     */
    hcsr04_capture_state = 0U;
    hcsr04_measurement_done = 0U;
    hcsr04_rising_edge = 0U;
    hcsr04_falling_edge = 0U;
    hcsr04_pulse_us = 0U;

    /*
     * Preparar TIM1_CH1 para capturar primero el flanco de subida.
     */
    __HAL_TIM_SET_CAPTUREPOLARITY(&htim1,
                                  TIM_CHANNEL_1,
                                  TIM_INPUTCHANNELPOLARITY_RISING);

    /*
     * Reiniciar contador antes de disparar el sensor.
     */
    __HAL_TIM_SET_COUNTER(&htim1, 0U);

    /*
     * Limpiar posible flag pendiente de captura.
     */
    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_CC1);

    /*
     * Generar pulso TRIG.
     *
     * El HC-SR04 requiere un pulso alto de al menos 10 us.
     */
    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
    HCSR04_DelayUs(2U);

    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_SET);
    HCSR04_DelayUs(10U);

    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
}


static uint8_t HCSR04_ReadDistance_IC(uint32_t *pulse_us, uint32_t *distance_x10_cm)
{
    uint32_t start_tick;

    if ((pulse_us == NULL) || (distance_x10_cm == NULL))
    {
        return 0U;
    }

    /*
     * Disparar medición.
     */
    HCSR04_Trigger();

    start_tick = HAL_GetTick();

    /*
     * Esperar hasta que el Input Capture detecte:
     *
     * 1. Flanco de subida de ECHO.
     * 2. Flanco de bajada de ECHO.
     *
     * La medición termina cuando el callback coloca
     * hcsr04_measurement_done = 1.
     */
    while (hcsr04_measurement_done == 0U)
    {
        if ((HAL_GetTick() - start_tick) > HCSR04_TIMEOUT_MS)
        {
            /*
             * Timeout:
             * no llegó eco o el objeto está fuera de rango.
             */
            hcsr04_capture_state = 0U;

            __HAL_TIM_SET_CAPTUREPOLARITY(&htim1,
                                          TIM_CHANNEL_1,
                                          TIM_INPUTCHANNELPOLARITY_RISING);

            return 0U;
        }
    }

    *pulse_us = hcsr04_pulse_us;

    /*
     * Cálculo de distancia.
     *
     * Velocidad del sonido aproximada:
     * v = 343 m/s = 0,0343 cm/us
     *
     * El pulso ECHO representa ida y vuelta.
     *
     * distancia_cm = pulse_us * 0,0343 / 2
     * distancia_cm = pulse_us * 0,01715
     *
     * Para evitar float:
     *
     * distancia_x10_cm = distancia_cm * 10
     * distancia_x10_cm = pulse_us * 0,1715
     * distancia_x10_cm = pulse_us * 1715 / 10000
     *
     * Se suma 5000 para redondear.
     */
    *distance_x10_cm = (((*pulse_us) * 1715UL) + 5000UL) / 10000UL;

    return 1U;
}


void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    uint32_t captured_value;

    if (htim->Instance == TIM1)
    {
        if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
        {
            /*
             * Leer el valor capturado en TIM1_CCR1.
             */
            captured_value = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

            /*
             * Estado 0:
             * se espera el flanco de subida de ECHO.
             */
            if (hcsr04_capture_state == 0U)
            {
                hcsr04_rising_edge = captured_value;
                hcsr04_capture_state = 1U;

                /*
                 * Una vez detectado el inicio del pulso,
                 * cambiamos la polaridad para capturar
                 * el flanco de bajada.
                 */
                __HAL_TIM_SET_CAPTUREPOLARITY(htim,
                                              TIM_CHANNEL_1,
                                              TIM_INPUTCHANNELPOLARITY_FALLING);
            }

            /*
             * Estado 1:
             * se espera el flanco de bajada de ECHO.
             */
            else if (hcsr04_capture_state == 1U)
            {
                hcsr04_falling_edge = captured_value;

                /*
                 * Calcular ancho del pulso ECHO.
                 *
                 * Caso normal:
                 * falling_edge >= rising_edge
                 *
                 * Caso con desborde:
                 * falling_edge < rising_edge
                 */
                if (hcsr04_falling_edge >= hcsr04_rising_edge)
                {
                    hcsr04_pulse_us = hcsr04_falling_edge - hcsr04_rising_edge;
                }
                else
                {
                    hcsr04_pulse_us = (TIM1_MAX_COUNT - hcsr04_rising_edge)
                                    + hcsr04_falling_edge
                                    + 1UL;
                }

                /*
                 * Marcar medición terminada.
                 */
                hcsr04_measurement_done = 1U;
                hcsr04_capture_state = 2U;

                /*
                 * Dejar el canal listo para la próxima medición:
                 * primero se capturará nuevamente flanco de subida.
                 */
                __HAL_TIM_SET_CAPTUREPOLARITY(htim,
                                              TIM_CHANNEL_1,
                                              TIM_INPUTCHANNELPOLARITY_RISING);
            }
        }
    }
}


static void USART2_SendDistance(uint8_t valid, uint32_t pulse_us, uint32_t distance_x10_cm)
{
    char tx_buffer[128];
    int len;

    if (valid != 0U)
    {
        len = snprintf(tx_buffer,
                       sizeof(tx_buffer),
                       "Echo=%5lu us | Distancia=%lu.%lu cm\r\n",
                       (unsigned long)pulse_us,
                       (unsigned long)(distance_x10_cm / 10UL),
                       (unsigned long)(distance_x10_cm % 10UL));
    }
    else
    {
        len = snprintf(tx_buffer,
                       sizeof(tx_buffer),
                       "Error: sin eco o fuera de rango\r\n");
    }

    if (len > 0)
    {
        HAL_UART_Transmit(&huart2,
                          (uint8_t *)tx_buffer,
                          (uint16_t)len,
                          HAL_MAX_DELAY);
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
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  /*
   * Iniciar TIM1 Channel 1 en modo Input Capture con interrupción.
   * ECHO está conectado a D7 / PA8 / TIM1_CH1.
   */
  HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);

  /*
   * Asegurar TRIG en bajo al inicio.
   * TRIG está conectado a D8 / PA9.
   */
  HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	 /* uint32_t pulse_us = 0U;
	  uint32_t distance_x10_cm = 0U;
	  uint8_t valid_measurement;

	  valid_measurement = HCSR04_ReadDistance_IC(&pulse_us, &distance_x10_cm);

	  USART2_SendDistance(valid_measurement, pulse_us, distance_x10_cm);

	  HAL_Delay(UART_REPORT_PERIOD_MS);*/

	  static uint32_t last_measurement_tick = 0U;
	      uint32_t current_tick = HAL_GetTick();

	      if ((current_tick - last_measurement_tick) >= HCSR04_MEASUREMENT_PERIOD_MS)
	      {
	          uint32_t pulse_us = 0U;
	          uint32_t distance_x10_cm = 0U;
	          uint8_t valid_measurement;

	          last_measurement_tick = current_tick;

	          valid_measurement = HCSR04_ReadDistance_IC(&pulse_us, &distance_x10_cm);

	          USART2_SendDistance(valid_measurement, pulse_us, distance_x10_cm);
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
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 83;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_IC_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim1, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

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
  HAL_GPIO_WritePin(GPIOA, LD2_Pin|HCSR04_TRIG_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD2_Pin HCSR04_TRIG_Pin */
  GPIO_InitStruct.Pin = LD2_Pin|HCSR04_TRIG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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
