#include "main.h"
#include "adc.h"
#include "fatfs.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

uint32_t adc_mq;
char msg[30];
float MQ;
float Rs;
float ppm;
float R0=8800.0f;
int ppm_node;


void SystemClock_Config(void);
void SD_Card_Test(void);
int __io_putchar(int ch);
void ppm_dat(void);

int main(void)
{

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_FATFS_Init();

  ppm_dat();
  HAL_Delay(1000);
  SD_Card_Test();

  while (1) {

  }

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
void SD_Card_Test(void){

	printf("Start FATFS System\n");
    FATFS FatFs;        // FAT file system object (required by FatFs)
    FIL Fil;            // File object (used to open/read/write files)
    FRESULT FR_Status;  // Stores return status of FatFs functions
    UINT WWC;      // RWC = Read byte count, WWC = Written byte count
    char RW_Buffer[200]; // Buffer used for both read and write operations


    FR_Status = f_mount(&FatFs, "", 1);
    if (FR_Status != FR_OK)
    {

        printf("[ERROR] SD Card mount failed! Code = %d\r\n", FR_Status);
        return;
    }


    printf("[INFO] SD Card mounted successfully\r\n\n");

    FR_Status = f_open(&Fil,
                       "STM_FILE.txt",
                       FA_OPEN_EXISTING | FA_WRITE);
    if (FR_Status != FR_OK)
    {
        printf("[ERROR] File open for update failed\r\n");
        return;
    }


    f_lseek(&Fil, f_size(&Fil));

    snprintf(RW_Buffer, sizeof(RW_Buffer), "[SD] Dato ppm: %d\r\n", ppm_node);

    f_write(&Fil, RW_Buffer, strlen(RW_Buffer), &WWC);
    printf("[SD] Appended new data to file\r\n");


    f_close(&Fil);

    FR_Status = f_mount(NULL, "", 0);
    if (FR_Status != FR_OK)
        printf("[ERROR] SD Card unmount failed! Code = %d\r\n", FR_Status);
    else
        printf("[INFO] SD Card unmounted safely\r\n");
}


int __io_putchar(int ch)
{
	HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
	return ch;
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
       	     len = sprintf(msg, "ppm: %d\r\n",ppm_node);
		     HAL_UART_Transmit(&huart2, (uint8_t*)msg, len, 100);
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
