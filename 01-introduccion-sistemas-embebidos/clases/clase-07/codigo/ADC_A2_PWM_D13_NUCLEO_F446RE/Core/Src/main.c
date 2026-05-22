/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  * Este programa lee un potenciómetro conectado a A2 / PA4 mediante ADC1_IN4.
  * La lectura del ADC se realiza por polling y entrega un valor entre 0 y 4095.
  * Ese valor se escala al rango 0 a 1000 y se escribe en TIM2->CCR1, que define
  * el duty cycle de una señal PWM de 1 kHz generada por TIM2_CH1. La señal PWM
  * sale por PA5 / D13 / LD2. Al mover el potenciómetro, la frecuencia se
  * mantiene fija y cambia el ancho del pulso en alto.
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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ADC_MAX_VALUE          4095UL
#define PWM_PERIOD_COUNTS      1000UL   /* ARR + 1 */
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
static void ADC1_PA4_Init_ByRegisters(void);
static uint32_t ADC1_ReadRaw(void);

static void TIM2_CH1_PA5_PWM_Init_ByRegisters(void);
static void PWM_SetDutyFromADC(uint32_t adc_raw);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* -------------------------------------------------------------
 * ADC1 en PA4 / A2
 * -------------------------------------------------------------
 * A2 en la NUCLEO-F446RE está conectado a PA4.
 * PA4 puede funcionar como ADC1_IN4.
 */
static void ADC1_PA4_Init_ByRegisters(void)
{
    /*
     * 1. Habilitar reloj de GPIOA
     */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;

    /*
     * 2. Configurar PA4 en modo analógico
     *
     * MODER4 = 11 → analog mode
     */
    GPIOA->MODER |= (3UL << (4U * 2U));

    /*
     * 3. Sin pull-up ni pull-down
     */
    GPIOA->PUPDR &= ~(3UL << (4U * 2U));

    /*
     * 4. Habilitar reloj de ADC1
     *
     * ADC1 pertenece al bus APB2.
     */
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->APB2ENR;

    /*
     * 5. Configurar prescaler común del ADC
     *
     * PCLK2 = 84 MHz
     * ADC clock = PCLK2 / 4 = 21 MHz
     *
     * En ADC->CCR, ADCPRE[1:0] está en bits 17:16.
     * 01 → PCLK2 dividido por 4.
     */
    ADC->CCR &= ~(3UL << 16U);
    ADC->CCR |=  (1UL << 16U);

    /*
     * 6. Configuración básica del ADC
     *
     * CR1 = 0:
     * - Resolución por defecto: 12 bits
     * - Scan mode deshabilitado
     *
     * CR2 = 0:
     * - Conversión continua deshabilitada
     * - Alineación a la derecha
     * - Disparo por software
     */
    ADC1->CR1 = 0U;
    ADC1->CR2 = 0U;

    /*
     * 7. Tiempo de muestreo para el canal 4
     *
     * Canal 4 usa SMP4 en ADC_SMPR2[14:12].
     * Valor 100 → 84 ciclos de ADC.
     */
    ADC1->SMPR2 &= ~(7UL << (4U * 3U));
    ADC1->SMPR2 |=  (4UL << (4U * 3U));

    /*
     * 8. Secuencia regular de una sola conversión
     *
     * L = 0000 → 1 conversión.
     */
    ADC1->SQR1 &= ~(0xFUL << 20U);

    /*
     * 9. Primera conversión de la secuencia: canal 4
     *
     * SQ1 = 4 → ADC_CHANNEL_4.
     */
    ADC1->SQR3 &= ~(0x1FUL << 0U);
    ADC1->SQR3 |=  (4UL << 0U);

    /*
     * 10. Asegurar alineación a la derecha,
     * conversión simple y disparo por software.
     */
    ADC1->CR2 &= ~ADC_CR2_ALIGN;
    ADC1->CR2 &= ~ADC_CR2_CONT;
    ADC1->CR2 &= ~ADC_CR2_EXTEN;

    /*
     * 11. Encender ADC1
     */
    ADC1->CR2 |= ADC_CR2_ADON;

    /*
     * Pequeña espera para estabilización.
     */
    HAL_Delay(1);
}


static uint32_t ADC1_ReadRaw(void)
{
    /*
     * Limpiar flag EOC antes de iniciar.
     */
    ADC1->SR &= ~ADC_SR_EOC;

    /*
     * Iniciar conversión regular por software.
     */
    ADC1->CR2 |= ADC_CR2_SWSTART;

    /*
     * Esperar hasta que termine la conversión.
     */
    while ((ADC1->SR & ADC_SR_EOC) == 0U)
    {
        /* Polling */
    }

    /*
     * Leer el registro de datos.
     * Para 12 bits, el valor útil va de 0 a 4095.
     */
    return (ADC1->DR & 0xFFFFUL);
}


/* -------------------------------------------------------------
 * PWM en PA5 / D13 / TIM2_CH1
 * -------------------------------------------------------------
 * PA5 está conectado a D13 y al LED LD2.
 * PA5 puede funcionar como TIM2_CH1 usando AF1.
 */
static void TIM2_CH1_PA5_PWM_Init_ByRegisters(void)
{
    /*
     * 1. Habilitar reloj de GPIOA
     */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;

    /*
     * 2. Configurar PA5 como función alternativa
     *
     * MODER5 = 10 → Alternate Function
     */
    GPIOA->MODER &= ~(3UL << (5U * 2U));
    GPIOA->MODER |=  (2UL << (5U * 2U));

    /*
     * 3. Salida push-pull
     */
    GPIOA->OTYPER &= ~(1UL << 5U);

    /*
     * 4. Velocidad alta para una señal PWM limpia
     */
    GPIOA->OSPEEDR &= ~(3UL << (5U * 2U));
    GPIOA->OSPEEDR |=  (2UL << (5U * 2U));

    /*
     * 5. Sin pull-up ni pull-down
     */
    GPIOA->PUPDR &= ~(3UL << (5U * 2U));

    /*
     * 6. Seleccionar AF1 para PA5
     *
     * AF1 corresponde a TIM2_CH1.
     * PA5 está en AFR[0].
     */
    GPIOA->AFR[0] &= ~(0xFUL << (5U * 4U));
    GPIOA->AFR[0] |=  (1UL   << (5U * 4U));

    /*
     * 7. Habilitar reloj de TIM2
     *
     * TIM2 pertenece a APB1.
     * En esta configuración:
     *
     * PCLK1 = 42 MHz
     * APB1 prescaler = /2
     * f_TIM2 = 2 x PCLK1 = 84 MHz
     */
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;

    /*
     * 8. Detener TIM2 durante la configuración.
     */
    TIM2->CR1 &= ~TIM_CR1_CEN;

    /*
     * Queremos PWM de 1 kHz.
     *
     * f_TIM2 = 84 MHz
     *
     * Elegimos:
     * PSC = 83
     *
     * Entonces:
     * f_CNT = 84 MHz / (83 + 1)
     * f_CNT = 1 MHz
     *
     * Luego:
     * ARR = 999
     *
     * Entonces:
     * f_PWM = 1 MHz / (999 + 1)
     * f_PWM = 1 kHz
     */
    TIM2->PSC = 83U;
    TIM2->ARR = 999U;
    TIM2->CNT = 0U;

    /*
     * Duty inicial: 0 %
     */
    TIM2->CCR1 = 0U;

    /*
     * 9. Canal 1 como salida
     *
     * CC1S = 00 → canal configurado como salida.
     */
    TIM2->CCMR1 &= ~TIM_CCMR1_CC1S;

    /*
     * 10. Configurar canal 1 en PWM mode 1
     *
     * OC1M = 110 → PWM mode 1
     *
     * En PWM mode 1:
     * La salida está activa mientras CNT < CCR1.
     */
    TIM2->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM2->CCMR1 |=  (6UL << 4U);

    /*
     * 11. Habilitar preload de CCR1
     *
     * Esto hace que los cambios de CCR1 se apliquen
     * de forma ordenada en eventos de actualización.
     */
    TIM2->CCMR1 |= TIM_CCMR1_OC1PE;

    /*
     * 12. Polaridad activa en alto y habilitar salida CH1
     */
    TIM2->CCER &= ~TIM_CCER_CC1P;
    TIM2->CCER |=  TIM_CCER_CC1E;

    /*
     * 13. Habilitar preload de ARR
     */
    TIM2->CR1 |= TIM_CR1_ARPE;

    /*
     * 14. Conteo ascendente, edge-aligned
     */
    TIM2->CR1 &= ~TIM_CR1_CMS;
    TIM2->CR1 &= ~TIM_CR1_DIR;

    /*
     * 15. Generar evento de actualización para cargar PSC, ARR y CCR.
     */
    TIM2->EGR |= TIM_EGR_UG;

    /*
     * 16. Arrancar TIM2
     */
    TIM2->CR1 |= TIM_CR1_CEN;
}


static void PWM_SetDutyFromADC(uint32_t adc_raw)
{
    uint32_t ccr_value;

    /*
     * Limitar por seguridad el valor leído.
     */
    if (adc_raw > ADC_MAX_VALUE)
    {
        adc_raw = ADC_MAX_VALUE;
    }

    /*
     * Escalado:
     *
     * ADC:  0 a 4095
     * PWM:  0 a 1000 cuentas
     *
     * Como ARR = 999, el período tiene 1000 cuentas.
     *
     * CCR1 = 0    → 0 %
     * CCR1 = 500  → 50 %
     * CCR1 = 1000 → 100 %
     */
    ccr_value = (adc_raw * PWM_PERIOD_COUNTS) / ADC_MAX_VALUE;

    /*
     * Actualizar duty cycle.
     * El hardware del timer genera la PWM automáticamente.
     */
    TIM2->CCR1 = ccr_value;
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
  /* USER CODE BEGIN 2 */
  ADC1_PA4_Init_ByRegisters();
  TIM2_CH1_PA5_PWM_Init_ByRegisters();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  uint32_t adc_raw = ADC1_ReadRaw();

	   PWM_SetDutyFromADC(adc_raw);

	   /*
	    * Esta demora no genera la PWM.
	    * Solo reduce la frecuencia con la que actualizamos CCR1.
	    */
	   HAL_Delay(10);

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
