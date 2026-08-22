#include <string.h>
#include <stdbool.h>
#include "main.h"
#include "usart.h"
#include "gpio.h"

void SystemClock_Config(void);
void uartTask(void);
void availableCommands(void);
bool alarmState=0;
int main(void){


  HAL_Init();

  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART2_UART_Init();

  HAL_GPIO_WritePin(alarmLed_GPIO_Port, alarmLed_Pin, GPIO_PIN_RESET);
  HAL_Delay(100);


  while (1){


	  if(HAL_GPIO_ReadPin(gasDetector_GPIO_Port,gasDetector_Pin)||HAL_GPIO_ReadPin(overTempDetector_GPIO_Port,overTempDetector_Pin)){
	  alarmState=1;
	  }
	  	HAL_GPIO_WritePin(alarmLed_GPIO_Port, alarmLed_Pin, alarmState);
	  if(HAL_GPIO_ReadPin(alarmoffButton_GPIO_Port, alarmoffButton_Pin)){
	  	alarmState=0;
	  }
	  uartTask();


  }
  /* USER CODE END 3 */
}

void uartTask(void){
uint8_t receivedChar = '\0';

if (HAL_UART_Receive(&huart2, &receivedChar, 1, 10) == HAL_OK){
	if (receivedChar == '1'){
		if (alarmState){
		char msg[] = "The alarm is activated\r\n";
		HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
		}else{
		char msg[] = "The alarm is not activated\r\n";
		HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
		}} else {
		availableCommands();
	} } }

void availableCommands(void){
char msg1[] = "Available command: Press '1' \r\n";

HAL_UART_Transmit(&huart2, (uint8_t*)msg1, strlen(msg1), 100);
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
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
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
