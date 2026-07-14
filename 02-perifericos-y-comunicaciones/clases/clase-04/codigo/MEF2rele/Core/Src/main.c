#include "main.h"
#include "usart.h"
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

typedef enum {
    DIRECTION_1,
    DIRECTION_2,
    STOPPED
} motorDirection_t;

static motorDirection_t motorDirection = STOPPED;
static motorDirection_t motorState = STOPPED;
/* USER CODE END PV */

/* USER CODE BEGIN PFP */
void SystemClock_Config(void);
void motorControlUpdate(void);
void processInputs(void);
bool isButtonPressed(void);
bool debounceButtonUpdate(void);
/* USER CODE END PFP */

int main(void) {
	  HAL_Init();
	  SystemClock_Config();
	  MX_GPIO_Init();
	  MX_USART2_UART_Init();

    // Estado inicial: Alta impedancia (motor apagado)
    HAL_GPIO_WritePin(releD7_GPIO_Port, releD7_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(releD8_GPIO_Port, releD8_Pin, GPIO_PIN_SET);

    while (1) {

    	debounceButtonUpdate();
    	motorControlUpdate();
        HAL_Delay(300);

    }
}

void motorControlUpdate(void) {
    static uint32_t lastTick = 0;

    if ((HAL_GetTick() - lastTick) >= 100) {
        lastTick = HAL_GetTick();

        switch (motorState) {
            case DIRECTION_1:
                if (motorDirection != DIRECTION_1) {

                	HAL_GPIO_WritePin(releD7_GPIO_Port, releD7_Pin, GPIO_PIN_SET);
//                	HAL_GPIO_WritePin(M1_GPIO_Port, M1_Pin, GPIO_PIN_SET);
                    motorState = STOPPED;
                }
                break;

            case DIRECTION_2:
                if (motorDirection != DIRECTION_2) {

                	HAL_GPIO_WritePin(releD8_GPIO_Port, releD8_Pin, GPIO_PIN_SET);
                	//HAL_GPIO_WritePin(M2_GPIO_Port, M2_Pin, GPIO_PIN_SET);
                    motorState = STOPPED;
                }
                break;

            case STOPPED:
            default:
                if (motorDirection == DIRECTION_1) {
                	HAL_GPIO_WritePin(releD7_GPIO_Port, releD7_Pin, GPIO_PIN_RESET);
                	//HAL_GPIO_WritePin(M1_GPIO_Port, M1_Pin, GPIO_PIN_RESET); // A masa
                    motorState = DIRECTION_1;
                } else if (motorDirection == DIRECTION_2) {
                	HAL_GPIO_WritePin(releD8_GPIO_Port, releD8_Pin, GPIO_PIN_RESET);
                	//HAL_GPIO_WritePin(M2_GPIO_Port, M2_Pin, GPIO_PIN_RESET); // A masa
                    motorState = DIRECTION_2;
                }
                break;
        }
    }
}
bool isButtonPressed(void) {
	bool bandera=0;
	//if (!HAL_GPIO_ReadPin(botonD2_GPIO_Port,botonD2_Pin) && !HAL_GPIO_ReadPin(botonD3_GPIO_Port,botonD3_Pin)){
	if (HAL_GPIO_ReadPin(botonD2_GPIO_Port,botonD2_Pin) && !HAL_GPIO_ReadPin(botonD3_GPIO_Port,botonD3_Pin)){
		bandera=1;
		motorDirection = DIRECTION_1;

	}else if(!HAL_GPIO_ReadPin(botonD2_GPIO_Port,botonD2_Pin) && HAL_GPIO_ReadPin(botonD3_GPIO_Port,botonD3_Pin)){
//	}else if(HAL_GPIO_ReadPin(botonD2_GPIO_Port,botonD2_Pin) && HAL_GPIO_ReadPin(botonD3_GPIO_Port,botonD3_Pin)){
		bandera=1;

		motorDirection = DIRECTION_2;
	}else{
		bandera=0;
		motorDirection = STOPPED;
	}
	return (bandera);
}

bool debounceButtonUpdate(void) {
    bool eventReleased = false;

    switch (buttonState) {
        case BUTTON_UP:
            if (isButtonPressed()) {
                buttonState = BUTTON_FALLING;
                accumulatedDebounceTime = 0;
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
