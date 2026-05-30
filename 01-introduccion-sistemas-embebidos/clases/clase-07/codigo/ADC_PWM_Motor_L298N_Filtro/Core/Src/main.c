/* USER CODE BEGIN Header */
/**
  ************************************************************************************
  * @file           : main.c
  * @brief          : El proyecto ee un potenciómetro conectado a A2 / PA4 
  * mediante ADC1_IN4, aplica un filtro de promedio móvil para estabilizar la 
  * medición y usa el valor filtrado para modificar el duty cycle de una señal 
  * PWM de 1 kHz generada con TIM2_CH1 sobre PA5 / D13.
  * La señal PWM se aplica al pin ENA del driver L298N para controlar la velocidad
  * de un motor DC, mientras que dos salidas digitales del STM32, PB5 / D4 y PB10 / D6,
  * controlan el sentido de giro mediante las entradas IN1 e IN2 del driver.
  * Además, el proyecto envía por USART2 el valor ADC filtrado, la tensión estimada,
  * el valor de CCR1, el duty cycle y el sentido de giro configurado. El objetivo 
  * didáctico es integrar ADC, filtrado digital, PWM por timer, GPIO, comunicación
  * serie y control básico de motores DC con driver L298N.
  **************************************************************************************
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
#define ADC_MAX_VALUE          4095UL
#define PWM_PERIOD_COUNTS      1000UL   /* ARR + 1 */
#define ADC_FILTER_SIZE        16U      /* Cantidad de muestras del promedio móvil */

#define L298N_IN1_PIN          5U       /* PB5  - D4 */
#define L298N_IN2_PIN          10U      /* PB10 - D6 */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static const char *motor_direction_text = "STOP";
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void ADC1_PA4_Init_ByRegisters(void);
static uint32_t ADC1_ReadRaw(void);
static uint32_t ADC1_ReadMovingAverage(void);

static void TIM2_CH1_PA5_PWM_Init_ByRegisters(void);
static void PWM_SetDutyFromADC(uint32_t adc_raw);

static void L298N_DirectionPins_Init_ByRegisters(void);
static void L298N_MotorForward(void);
static void L298N_MotorReverse(void);
static void L298N_MotorStop(void);

static void USART2_SendADC_PWM_Values(uint32_t adc_value);
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
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;

    GPIOA->MODER |= (3UL << (4U * 2U));
    GPIOA->PUPDR &= ~(3UL << (4U * 2U));

    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->APB2ENR;

    /*
     * PCLK2 = 84 MHz
     * ADC clock = PCLK2 / 4 = 21 MHz
     */
    ADC->CCR &= ~(3UL << 16U);
    ADC->CCR |=  (1UL << 16U);

    /*
     * CR1 = 0:
     * - Resolución 12 bits
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
     * Tiempo de muestreo del canal 4:
     * SMP4 = 100 -> 84 ciclos de ADC.
     */
    ADC1->SMPR2 &= ~(7UL << (4U * 3U));
    ADC1->SMPR2 |=  (4UL << (4U * 3U));

    /*
     * Secuencia regular de una sola conversión.
     */
    ADC1->SQR1 &= ~(0xFUL << 20U);

    /*
     * Primera conversión: canal 4.
     */
    ADC1->SQR3 &= ~(0x1FUL << 0U);
    ADC1->SQR3 |=  (4UL << 0U);

    ADC1->CR2 &= ~ADC_CR2_ALIGN;
    ADC1->CR2 &= ~ADC_CR2_CONT;
    ADC1->CR2 &= ~ADC_CR2_EXTEN;

    ADC1->CR2 |= ADC_CR2_ADON;

    HAL_Delay(1);
}


static uint32_t ADC1_ReadRaw(void)
{
    ADC1->SR &= ~ADC_SR_EOC;

    ADC1->CR2 |= ADC_CR2_SWSTART;

    while ((ADC1->SR & ADC_SR_EOC) == 0U)
    {
        /* Polling */
    }

    return (ADC1->DR & 0xFFFFUL);
}


/* -------------------------------------------------------------
 * Promedio móvil para estabilizar la lectura del ADC
 * ------------------------------------------------------------- */
static uint32_t ADC1_ReadMovingAverage(void)
{
    static uint32_t buffer[ADC_FILTER_SIZE] = {0U};
    static uint32_t index = 0U;
    static uint32_t sum = 0U;
    static uint32_t count = 0U;

    uint32_t new_sample;
    uint32_t average;

    new_sample = ADC1_ReadRaw();

    if (count < ADC_FILTER_SIZE)
    {
        buffer[index] = new_sample;
        sum += new_sample;
        count++;
    }
    else
    {
        sum -= buffer[index];
        buffer[index] = new_sample;
        sum += new_sample;
    }

    index++;

    if (index >= ADC_FILTER_SIZE)
    {
        index = 0U;
    }

    average = sum / count;

    return average;
}


/* -------------------------------------------------------------
 * PWM en PA5 / D13 / TIM2_CH1
 * -------------------------------------------------------------
 * PA5 está conectado a D13 y al LED LD2.
 * PA5 puede funcionar como TIM2_CH1 usando AF1.
 */
static void TIM2_CH1_PA5_PWM_Init_ByRegisters(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;

    /*
     * PA5 como función alternativa.
     */
    GPIOA->MODER &= ~(3UL << (5U * 2U));
    GPIOA->MODER |=  (2UL << (5U * 2U));

    GPIOA->OTYPER &= ~(1UL << 5U);

    GPIOA->OSPEEDR &= ~(3UL << (5U * 2U));
    GPIOA->OSPEEDR |=  (2UL << (5U * 2U));

    GPIOA->PUPDR &= ~(3UL << (5U * 2U));

    /*
     * AF1 en PA5 corresponde a TIM2_CH1.
     */
    GPIOA->AFR[0] &= ~(0xFUL << (5U * 4U));
    GPIOA->AFR[0] |=  (1UL   << (5U * 4U));

    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;

    TIM2->CR1 &= ~TIM_CR1_CEN;

    /*
     * f_TIM2 = 84 MHz
     * PSC = 83  -> f_CNT = 1 MHz
     * ARR = 999 -> f_PWM = 1 kHz
     */
    TIM2->PSC = 83U;
    TIM2->ARR = 999U;
    TIM2->CNT = 0U;

    TIM2->CCR1 = 0U;

    /*
     * Canal 1 como salida.
     */
    TIM2->CCMR1 &= ~TIM_CCMR1_CC1S;

    /*
     * PWM mode 1:
     * salida activa mientras CNT < CCR1.
     */
    TIM2->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM2->CCMR1 |=  (6UL << 4U);

    /*
     * Preload de CCR1.
     */
    TIM2->CCMR1 |= TIM_CCMR1_OC1PE;

    /*
     * Polaridad activa en alto y salida CH1 habilitada.
     */
    TIM2->CCER &= ~TIM_CCER_CC1P;
    TIM2->CCER |=  TIM_CCER_CC1E;

    /*
     * Preload de ARR.
     */
    TIM2->CR1 |= TIM_CR1_ARPE;

    /*
     * Conteo ascendente, edge-aligned.
     */
    TIM2->CR1 &= ~TIM_CR1_CMS;
    TIM2->CR1 &= ~TIM_CR1_DIR;

    /*
     * Cargar PSC, ARR y CCR.
     */
    TIM2->EGR |= TIM_EGR_UG;

    /*
     * Arrancar TIM2.
     */
    TIM2->CR1 |= TIM_CR1_CEN;
}


static void PWM_SetDutyFromADC(uint32_t adc_raw)
{
    uint32_t ccr_value;

    if (adc_raw > ADC_MAX_VALUE)
    {
        adc_raw = ADC_MAX_VALUE;
    }

    /*
     * ADC: 0 a 4095
     * PWM: 0 a 1000 cuentas
     */
    ccr_value = (adc_raw * PWM_PERIOD_COUNTS) / ADC_MAX_VALUE;

    TIM2->CCR1 = ccr_value;
}


/* -------------------------------------------------------------
 * Control de dirección para motor DC con L298N
 * -------------------------------------------------------------
 * ENA recibe la PWM desde PA5 / D13 / TIM2_CH1.
 *
 * IN1 -> PB5  / D4
 * IN2 -> PB10 / D6
 */
static void L298N_DirectionPins_Init_ByRegisters(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    (void)RCC->AHB1ENR;

    /*
     * PB5 y PB10 como salidas digitales.
     */
    GPIOB->MODER &= ~((3UL << (L298N_IN1_PIN * 2U)) |
                      (3UL << (L298N_IN2_PIN * 2U)));

    GPIOB->MODER |=  ((1UL << (L298N_IN1_PIN * 2U)) |
                      (1UL << (L298N_IN2_PIN * 2U)));

    /*
     * Push-pull.
     */
    GPIOB->OTYPER &= ~((1UL << L298N_IN1_PIN) |
                       (1UL << L298N_IN2_PIN));

    /*
     * Velocidad baja.
     */
    GPIOB->OSPEEDR &= ~((3UL << (L298N_IN1_PIN * 2U)) |
                        (3UL << (L298N_IN2_PIN * 2U)));

    /*
     * Sin pull-up ni pull-down.
     */
    GPIOB->PUPDR &= ~((3UL << (L298N_IN1_PIN * 2U)) |
                      (3UL << (L298N_IN2_PIN * 2U)));

    L298N_MotorStop();
}


static void L298N_MotorForward(void)
{
    /*
     * IN1 = 1
     * IN2 = 0
     */
    GPIOB->BSRR = (1UL << L298N_IN1_PIN) |
                  (1UL << (L298N_IN2_PIN + 16U));

    motor_direction_text = "FORWARD";
}


static void L298N_MotorReverse(void)
{
    /*
     * IN1 = 0
     * IN2 = 1
     */
    GPIOB->BSRR = (1UL << (L298N_IN1_PIN + 16U)) |
                  (1UL << L298N_IN2_PIN);

    motor_direction_text = "REVERSE";
}


static void L298N_MotorStop(void)
{
    /*
     * IN1 = 0
     * IN2 = 0
     */
    GPIOB->BSRR = (1UL << (L298N_IN1_PIN + 16U)) |
                  (1UL << (L298N_IN2_PIN + 16U));

    motor_direction_text = "STOP";
}


/* -------------------------------------------------------------
 * Envío por USART2
 * ------------------------------------------------------------- */
static void USART2_SendADC_PWM_Values(uint32_t adc_value)
{
    char tx_buffer[160];

    uint32_t voltage_mv;
    uint32_t ccr_value;
    uint32_t duty_x10;
    int len;

    if (adc_value > ADC_MAX_VALUE)
    {
        adc_value = ADC_MAX_VALUE;
    }

    voltage_mv = (adc_value * 3300UL) / ADC_MAX_VALUE;

    ccr_value = (adc_value * PWM_PERIOD_COUNTS) / ADC_MAX_VALUE;

    duty_x10 = (ccr_value * 1000UL) / PWM_PERIOD_COUNTS;

    len = snprintf(tx_buffer,
                   sizeof(tx_buffer),
                   "ADC_filtrado=%4lu | V=%lu.%03lu V | CCR1=%4lu | Duty=%lu.%lu %% | Motor=%s\r\n",
                   (unsigned long)adc_value,
                   (unsigned long)(voltage_mv / 1000UL),
                   (unsigned long)(voltage_mv % 1000UL),
                   (unsigned long)ccr_value,
                   (unsigned long)(duty_x10 / 10UL),
                   (unsigned long)(duty_x10 % 10UL),
                   motor_direction_text);

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
  /* USER CODE BEGIN 2 */
  ADC1_PA4_Init_ByRegisters();
  TIM2_CH1_PA5_PWM_Init_ByRegisters();

  L298N_DirectionPins_Init_ByRegisters();

  /*
   * Sentido inicial del motor.
   * La velocidad queda controlada por la PWM en ENA.
   */
  //L298N_MotorForward();
  L298N_MotorReverse();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  uint32_t adc_filtered = ADC1_ReadMovingAverage();

	  PWM_SetDutyFromADC(adc_filtered);

	  /*
	   * Enviar datos por USART2 cada 250 ms.
	   * La PWM se sigue actualizando cada 10 ms.
	   */
	  static uint32_t last_uart_tick = 0U;
	  uint32_t current_tick = HAL_GetTick();

	  if ((current_tick - last_uart_tick) >= 250U)
	  {
	      last_uart_tick = current_tick;

	      USART2_SendADC_PWM_Values(adc_filtered);
	  }

	  /*
	   * Esta demora no genera la PWM.
	   * La PWM la genera TIM2 por hardware.
	   * Esta demora solo define cada cuánto se toma una nueva muestra
	   * y se actualiza el duty cycle.
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
