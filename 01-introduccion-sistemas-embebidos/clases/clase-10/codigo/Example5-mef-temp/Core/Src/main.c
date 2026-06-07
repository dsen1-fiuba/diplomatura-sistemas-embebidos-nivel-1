#include "main.h"
#include "adc.h"
#include "gpio.h"
#include <stdbool.h>
#define SP_H 90
#define SP_L 70

uint32_t adc_raw;
float Temp;

typedef enum {
    CALENTAR,
    APAGAR
} estados_sistema;

estados_sistema estado = CALENTAR;

bool TH, TL,led;
void SystemClock_Config(void);
void histeresis(void);
void medir(void);

int main(void){

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_ADC1_Init();
  while (1){
	  histeresis();
	  HAL_Delay(500);
  }
}

void medir(void) {
	 // uint32_t adc_raw;
	 // float Temp;

	HAL_ADC_Start(&hadc1);
	    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK){
	              adc_raw = HAL_ADC_GetValue(&hadc1);
	              Temp = adc_raw/10.0;

	              if (Temp > SP_H){
	            	TH=1;
	              }else{
	            	TH=0;
	              }

	              if (Temp > SP_L){
	            	TL=1;
	              }else{
	            	TL=0;
	              }
	          }
}



void histeresis(void){

	medir();
	switch (estado) {

        case CALENTAR:
        	HAL_GPIO_WritePin(led_GPIO_Port, led_Pin, GPIO_PIN_SET);
            if ((TL == 0 && TH==0)||(TL == 1 && TH==0)) {
                estado = CALENTAR;
            }else{
            	estado = APAGAR;
            }

            break;

        case APAGAR:
        	HAL_GPIO_WritePin(led_GPIO_Port, led_Pin, GPIO_PIN_RESET);
            if ((TL == 1 && TH==0)||(TL == 1 && TH==1)) {
                	estado = APAGAR;
             }else{
                   	estado = CALENTAR;
             }
            break;
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
