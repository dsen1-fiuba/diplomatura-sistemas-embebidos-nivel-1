#include "main.h"
#include "gpio.h"
#include <stdbool.h>
#define DEBOUNCE_TIME 50
#define TIME_INCREMENT 100

typedef enum {
    BUTTON_UP,
    BUTTON_FALLING,
    BUTTON_DOWN,
    BUTTON_RISING
} buttonState_t;

buttonState_t buttonState = BUTTON_UP;

buttonState_t enterButtonState = BUTTON_UP;

int accumulatedDebounceTime = 0;
void SystemClock_Config(void);
bool isButtonPressed(void);
bool debounceButtonUpdate(void);

int main(void){
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  while (1){
	  if (debounceButtonUpdate()) {
  		 HAL_GPIO_WritePin(salidaD8_GPIO_Port, salidaD8_Pin, GPIO_PIN_SET);
  	      HAL_Delay(TIME_INCREMENT);
		  HAL_GPIO_TogglePin(salidaD7_GPIO_Port, salidaD7_Pin);

	  	 }

	  	     HAL_Delay(TIME_INCREMENT);

  }

}

bool isButtonPressed(void) {
    return (HAL_GPIO_ReadPin(botonD2_GPIO_Port, botonD2_Pin) == GPIO_PIN_RESET);
}

bool debounceButtonUpdate(void) {
    bool eventReleased = false;

    switch (buttonState) {
        case BUTTON_UP:
            if (isButtonPressed()) {
                buttonState = BUTTON_FALLING;
                accumulatedDebounceTime = 0;//0
            }
            break;

        case BUTTON_FALLING:
            if (accumulatedDebounceTime >= DEBOUNCE_TIME) {
                if (isButtonPressed()) {
                    buttonState = BUTTON_DOWN;
                } else {
                    buttonState = BUTTON_UP;
                }
            }
            accumulatedDebounceTime += TIME_INCREMENT;
            break;

        case BUTTON_DOWN:
            if (!isButtonPressed()) {
                buttonState = BUTTON_RISING;
                accumulatedDebounceTime = 0;
            }
            break;

        case BUTTON_RISING:
            if (accumulatedDebounceTime >= DEBOUNCE_TIME) {
                if (!isButtonPressed()) {
                    buttonState = BUTTON_UP;
                    eventReleased = true;
                } else {
                    buttonState = BUTTON_DOWN;
                }
            }
            accumulatedDebounceTime += TIME_INCREMENT;
            break;
    }
    return eventReleased;
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
