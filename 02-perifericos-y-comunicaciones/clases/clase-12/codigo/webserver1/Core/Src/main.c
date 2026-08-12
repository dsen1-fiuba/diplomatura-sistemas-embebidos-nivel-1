#include "main.h"
#include "adc.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"
#include "stdio.h"


uint8_t rxBuffer[512] = {0};
uint8_t ATisOK;
int channel;
int onoff;
int led = 1;
char ATcommand[64];
char ATcommandB[1024];
char ATcommandN[100];
char ATcommandF[100];
char ATcommandT[16];

void SystemClock_Config(void);

int main(void){

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_UART4_Init();
  MX_USART2_UART_Init();

  sprintf(ATcommandB,"<!DOCTYPE html><html>\n<head>\n\
  <title>STM32 - ESP8266</title>\n<link href=\"data:image/x-icon;base64,\
  A\" rel=\"icon\" type=\"image/x-icon\"><style>\nhtml {\
  display: inline-block; margin: 0px auto; text-align: center;}\n\
  body{margin-top: 50px;}\n.button {display: block;\n\
  width: 70px;\nbackground-color: #008000;\nborder: none;\ncolor: white;\n\
  padding: 14px 28px;\ntext-decoration: none;\nfont-size: 24px;\n\
  margin: 0px auto 36px; \nborder-radius: 5px;}\n\
  .button-on {background-color: #008000;}\n.button-on:active\
  {background-color: #008000;}\n.button-off {background-color: #808080;}\n\
  .button-off:active {background-color: #808080;}\n\
  p {font-size: 14px;color: #808080;margin-bottom: 20px;}\n\
  </style>\n</head>\n<body>\n<h1>STM32 - ESP8266</h1>");
  sprintf(ATcommandN,"<p>Light is currently on\
  </p><a class=\"button button-off\" href=\"/lightoff\">OFF</a>");
  sprintf(ATcommandF,"<p>Light is currently off\
  </p><a class=\"button button-on\" href=\"/lighton\">ON</a>");
  sprintf(ATcommandT,"</body></html>");
  int countB = strlen(ATcommandB);
  int countN = strlen(ATcommandN);
  int countF = strlen(ATcommandF);
  int countT = strlen(ATcommandT);

  sprintf(ATcommand,"AT+RST\r\n");
  memset(rxBuffer,0,sizeof(rxBuffer));
  HAL_UART_Transmit(&huart4,(uint8_t *)ATcommand,strlen(ATcommand),1000);
  HAL_UART_Receive (&huart4, rxBuffer, 512, 100);
  HAL_Delay(500);

  ATisOK = 0;
  while(!ATisOK){
    sprintf(ATcommand,"AT+CWMODE_CUR=2\r\n");
      memset(rxBuffer,0,sizeof(rxBuffer));
      HAL_UART_Transmit(&huart4,(uint8_t *)ATcommand,strlen(ATcommand),1000);
      HAL_UART_Receive (&huart4, rxBuffer, 512, 1000);
    if(strstr((char *)rxBuffer,"OK")){
      ATisOK = 1;
    }
    HAL_Delay(500);
  }

  ATisOK = 0;
  while(!ATisOK){
    sprintf(ATcommand,"AT+CWSAP_CUR=\"STM32\",\"12345678\",1,3,4,0\r\n");
      memset(rxBuffer,0,sizeof(rxBuffer));
      HAL_UART_Transmit(&huart4,(uint8_t *)ATcommand,strlen(ATcommand),1000);
      HAL_UART_Receive (&huart4, rxBuffer, 512, 1000);
    if(strstr((char *)rxBuffer,"OK")){
      ATisOK = 1;
    }
    HAL_Delay(500);
  }

  ATisOK = 0;
  while(!ATisOK){
    sprintf(ATcommand,"AT+CIPAP_CUR=\"192.168.51.1\"\r\n");
    memset(rxBuffer,0,sizeof(rxBuffer));
    HAL_UART_Transmit(&huart4,(uint8_t *)ATcommand,strlen(ATcommand),1000);
    HAL_UART_Receive (&huart4, rxBuffer, 512, 1000);
    if(strstr((char *)rxBuffer,"OK")){
      ATisOK = 1;
    }
    HAL_Delay(500);
  }

  ATisOK = 0;
  while(!ATisOK){
    sprintf(ATcommand,"AT+CIPMUX=1\r\n");
      memset(rxBuffer,0,sizeof(rxBuffer));
      HAL_UART_Transmit(&huart4,(uint8_t *)ATcommand,strlen(ATcommand),1000);
      HAL_UART_Receive (&huart4, rxBuffer, 512, 1000);
      if(strstr((char *)rxBuffer,"OK")){
        ATisOK = 1;
      }
      HAL_Delay(500);
  }

  ATisOK = 0;
  while(!ATisOK){
    sprintf(ATcommand,"AT+CIPSERVER=1,80\r\n");
    memset(rxBuffer,0,sizeof(rxBuffer));
    HAL_UART_Transmit(&huart4,(uint8_t *)ATcommand,strlen(ATcommand),1000);
    HAL_UART_Receive (&huart4, rxBuffer, 512, 1000);
    if(strstr((char *)rxBuffer,"OK")){
        ATisOK = 1;
    }
    HAL_Delay(500);
  }



  while (1){
	   memset(rxBuffer,0,sizeof(rxBuffer));
	    HAL_UART_Receive (&huart4, rxBuffer, 512, 1000);
	    if(strstr((char *)rxBuffer,"+IPD,0")) channel = 0;
	    else if(strstr((char *)rxBuffer,"+IPD,1")) channel = 1;
	    else if(strstr((char *)rxBuffer,"+IPD,2")) channel = 2;
	    else if(strstr((char *)rxBuffer,"+IPD,3")) channel = 3;
	    else if(strstr((char *)rxBuffer,"+IPD,4")) channel = 4;
	    else if(strstr((char *)rxBuffer,"+IPD,5")) channel = 5;
	    else if(strstr((char *)rxBuffer,"+IPD,6")) channel = 6;
	    else if(strstr((char *)rxBuffer,"+IPD,7")) channel = 7;
	    else channel = 100;

	    if(strstr((char *)rxBuffer,"GET /lighton")) onoff = 0;
	    else if(strstr((char *)rxBuffer,"GET /lightoff")) onoff = 1;
	    else onoff = led;

	    if(channel<8 && onoff == 1)
	    {
	      HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
	      led = 1;
	      sprintf(ATcommand,"AT+CIPSEND=%d,%d\r\n",channel,countB+countF+countT);
	      memset(rxBuffer,0,sizeof(rxBuffer));
	      HAL_UART_Transmit(&huart4,(uint8_t *)ATcommand,strlen(ATcommand),1000);
	      HAL_UART_Receive (&huart4, rxBuffer, 512, 100);
	      if(strstr((char *)rxBuffer,">"))
	      {
	        memset(rxBuffer,0,sizeof(rxBuffer));
	          HAL_UART_Transmit(&huart4,(uint8_t *)ATcommandB,countB,1000);
	          HAL_UART_Transmit(&huart4,(uint8_t *)ATcommandF,countF,1000);
	          HAL_UART_Transmit(&huart4,(uint8_t *)ATcommandT,countT,1000);
	         HAL_UART_Receive (&huart4, rxBuffer, 512, 100);
	      }
	      sprintf(ATcommand,"AT+CIPCLOSE=%d\r\n",channel);
	      memset(rxBuffer,0,sizeof(rxBuffer));
	      HAL_UART_Transmit(&huart4,(uint8_t *)ATcommand,strlen(ATcommand),1000);
	      HAL_UART_Receive (&huart4, rxBuffer, 512, 100);
	      channel=100;
	    }
	    else if(channel<8 && onoff == 0)
	    {
	      HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
	      led = 0;
	      sprintf(ATcommand,"AT+CIPSEND=%d,%d\r\n",channel,countB+countN+countT);
	      memset(rxBuffer,0,sizeof(rxBuffer));
	      HAL_UART_Transmit(&huart4,(uint8_t *)ATcommand,strlen(ATcommand),1000);
	      HAL_UART_Receive (&huart4, rxBuffer, 512, 100);
	      if(strstr((char *)rxBuffer,">"))
	      {
	        memset(rxBuffer,0,sizeof(rxBuffer));
	          HAL_UART_Transmit(&huart4,(uint8_t *)ATcommandB,countB,1000);
	          HAL_UART_Transmit(&huart4,(uint8_t *)ATcommandN,countN,1000);
	          HAL_UART_Transmit(&huart4,(uint8_t *)ATcommandT,countT,1000);
	          HAL_UART_Receive (&huart4, rxBuffer, 512, 100);
	      }
	      sprintf(ATcommand,"AT+CIPCLOSE=%d\r\n",channel);
	      memset(rxBuffer,0,sizeof(rxBuffer));
	      HAL_UART_Transmit(&huart4,(uint8_t *)ATcommand,strlen(ATcommand),1000);
	      HAL_UART_Receive (&huart4, rxBuffer, 512, 100);
	      channel=100;
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
