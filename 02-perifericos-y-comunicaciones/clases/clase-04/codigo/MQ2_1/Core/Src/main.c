#include "main.h"
#include "adc.h"
#include "usart.h"
#include "gpio.h"
#include <stdio.h>
#include <math.h>

uint32_t adc_mq;
char msg[30];
float MQ;
float Rs;
float ppm;
float R0=8800.0f;
int ppm_node;

void SystemClock_Config(void);
void ppm_dat(void);

int main(void){

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_USART2_UART_Init();
  while (1){

	      ppm_dat();
	 	  HAL_Delay(5000);

  }

}

void ppm_dat(void){
	  int len;

	  HAL_ADC_Start(&hadc1);
		 	  if(HAL_ADC_PollForConversion(&hadc1, 10)==HAL_OK){
		 	  adc_mq= HAL_ADC_GetValue(&hadc1);
		 	  MQ = 2* adc_mq * 3.3 / 4095.0;
		 	  Rs = ((5.0 / MQ) - 1.0f) * 1000.0;
		 	  ppm = 600.0 * pow(Rs/R0, -2.2f);//Constantes p/ Alcohol (Etanol)
		 	  ppm_node = (int)(ppm + 0.5);

		 	  if(HAL_GPIO_ReadPin(ALARM_D2_GPIO_Port, ALARM_D2_Pin)){
		 	     len = sprintf(msg, "ppm: %d\r\n",ppm_node);
		         HAL_UART_Transmit(&huart2, (uint8_t*)msg, len, 100);
		 	  }else{
		 		 len = sprintf(msg, "ALARMA ppm: %d\r\n",ppm_node);
		 		 HAL_UART_Transmit(&huart2, (uint8_t*)msg, len, 100);
		 	  }
		 	  }
}
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
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 90;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
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
