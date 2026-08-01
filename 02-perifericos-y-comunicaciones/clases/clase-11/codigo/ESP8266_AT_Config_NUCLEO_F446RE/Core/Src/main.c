/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Programa principal para configurar un módulo ESP8266
  *                   mediante comandos AT y crear un servidor web embebido
  *                   controlado por una placa STM32F446RE.
  *
  *                   El firmware utiliza USART2 para la comunicación con la PC
  *                   mediante una terminal serie, y UART4 para el intercambio
  *                   de comandos y datos con el ESP8266. El módulo ESP8266 se
  *                   configura como punto de acceso Wi-Fi y servidor TCP en el
  *                   puerto 80. Las solicitudes HTTP recibidas desde un navegador
  *                   son interpretadas por el STM32 para controlar el LED LD2 y
  *                   generar una respuesta HTML.
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
#define PC_RX_SIZE          128U
#define ESP_RX_SIZE         1024U
#define ESP_AT_EOL          "\r\n"

#define HTTP_BODY_SIZE        768U
#define HTTP_RESPONSE_SIZE    1200U
#define HTTP_CMD_SIZE         64U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart4;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static uint8_t pc_rx_buffer[PC_RX_SIZE];
static uint8_t esp_rx_buffer[ESP_RX_SIZE];

static char pc_command[PC_RX_SIZE];
static char esp_message[ESP_RX_SIZE];

static volatile uint8_t pc_command_ready = 0U;
static volatile uint8_t esp_message_ready = 0U;

static uint8_t led_state = 0U;

static char http_body[HTTP_BODY_SIZE];
static char http_response[HTTP_RESPONSE_SIZE];
static char http_cmd[HTTP_CMD_SIZE];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_UART4_Init(void);
/* USER CODE BEGIN PFP */
static void PC_Print(const char *msg);
static void ESP_SendRaw(const char *msg);
static void ESP_SendAT(const char *cmd);

static void ProcessPcCommand(const char *cmd);
static void ProcessEspMessage(const char *msg);

static void ESP8266_ConfigSoftAP(void);
static void ESP8266_ServerOn(void);
static void ESP8266_ServerOff(void);

static void ESP8266_HandleHttpRequest(const char *msg);
static void ESP8266_SendHttpResponse(uint8_t link_id, const char *body);
static void BuildHomePage(char *body, size_t size, const char *message);

static void TrimLine(char *str);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void PC_Print(const char *msg)
{
    HAL_UART_Transmit(&huart2,
                      (uint8_t *)msg,
                      strlen(msg),
                      HAL_MAX_DELAY);
}

static void ESP_SendRaw(const char *msg)
{
    HAL_UART_Transmit(&huart4,
                      (uint8_t *)msg,
                      strlen(msg),
                      HAL_MAX_DELAY);
}

static void ESP_SendAT(const char *cmd)
{
    ESP_SendRaw(cmd);
    ESP_SendRaw(ESP_AT_EOL);
}

static void TrimLine(char *str)
{
    size_t len = strlen(str);

    while ((len > 0U) &&
           ((str[len - 1U] == '\r') ||
            (str[len - 1U] == '\n') ||
            (str[len - 1U] == ' ')))
    {
        str[len - 1U] = '\0';
        len--;
    }
}

//Callback de recepción UART por interrupción
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART2)
    {
        if (Size >= PC_RX_SIZE)
        {
            Size = PC_RX_SIZE - 1U;
        }

        memcpy(pc_command, pc_rx_buffer, Size);
        pc_command[Size] = '\0';
        TrimLine(pc_command);

        pc_command_ready = 1U;

        HAL_UARTEx_ReceiveToIdle_IT(&huart2,
                                    pc_rx_buffer,
                                    PC_RX_SIZE);
    }

    else if (huart->Instance == UART4)
    {
        if (Size >= ESP_RX_SIZE)
        {
            Size = ESP_RX_SIZE - 1U;
        }

        memcpy(esp_message, esp_rx_buffer, Size);
        esp_message[Size] = '\0';

        esp_message_ready = 1U;

        HAL_UARTEx_ReceiveToIdle_IT(&huart4,
                                    esp_rx_buffer,
                                    ESP_RX_SIZE);
    }
}
//Procesamiento de comandos desde Cutecom
static void ProcessPcCommand(const char *cmd)
{
    if (strlen(cmd) == 0U)
    {
        return;
    }

    PC_Print("[PC] ");
    PC_Print(cmd);
    PC_Print("\r\n");

    if (strcmp(cmd, "HELP") == 0)
    {
        PC_Print("\r\nComandos disponibles:\r\n");
        PC_Print("HELP\r\n");
        PC_Print("AT\r\n");
        PC_Print("AT+GMR\r\n");
        PC_Print("AT+CWMODE?\r\n");
        PC_Print("AT+CIPAP?\r\n");
        PC_Print("WIFI_AP\r\n");
        PC_Print("SERVER_ON\r\n");
        PC_Print("SERVER_OFF\r\n");
        PC_Print("IP\r\n");
        PC_Print("\r\n");
        PC_Print("Tambien puede enviar cualquier comando AT directamente.\r\n");
        PC_Print("Ejemplo: AT+CWLAP\r\n\r\n");
    }

    else if (strcmp(cmd, "WIFI_AP") == 0)
    {
        ESP8266_ConfigSoftAP();
    }

    else if (strcmp(cmd, "SERVER_ON") == 0)
    {
        ESP8266_ServerOn();
    }

    else if (strcmp(cmd, "SERVER_OFF") == 0)
    {
        ESP8266_ServerOff();
    }

    else if (strcmp(cmd, "IP") == 0)
    {
        PC_Print("[STM32 -> ESP8266] AT+CIPAP?\r\n");
        ESP_SendAT("AT+CIPAP?");
    }

    else if (strncmp(cmd, "AT", 2U) == 0)
    {
        PC_Print("[STM32 -> ESP8266] ");
        PC_Print(cmd);
        PC_Print(" + CRLF\r\n");

        ESP_SendAT(cmd);
    }

    else
    {
        PC_Print("Comando no reconocido. Escriba HELP.\r\n");
    }
}

//Procesamiento de mensajes recibidos desde el ESP8266
static void ProcessEspMessage(const char *msg)
{
	 if (strlen(msg) == 0U)
	    {
	        return;
	    }

	    PC_Print("[ESP8266 -> STM32]\r\n");
	    PC_Print(msg);
	    PC_Print("\r\n");

	    /*
	     * Si el ESP8266 informa datos recibidos desde la red,
	     * el mensaje contiene +IPD.
	     *
	     * Ejemplo:
	     * +IPD,0,439:GET / HTTP/1.1
	     */
	    if (strstr(msg, "+IPD,") != NULL)
	    {
	        ESP8266_HandleHttpRequest(msg);
	    }
}

static void ESP8266_HandleHttpRequest(const char *msg)
{
    const char *ipd_ptr;
    const char *payload_ptr;
    int link_id = 0;
    int length = 0;

    ipd_ptr = strstr(msg, "+IPD,");

    if (ipd_ptr == NULL)
    {
        return;
    }

    if (sscanf(ipd_ptr, "+IPD,%d,%d:", &link_id, &length) != 2)
    {
        PC_Print("[ERROR] No se pudo interpretar el encabezado +IPD.\r\n");
        return;
    }

    payload_ptr = strchr(ipd_ptr, ':');

    if (payload_ptr == NULL)
    {
        PC_Print("[ERROR] No se encontro el inicio del payload HTTP.\r\n");
        return;
    }

    payload_ptr++;

    PC_Print("[HTTP] Solicitud recibida.\r\n");

    if (strstr(payload_ptr, "GET /on ") != NULL)
    {
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
        led_state = 1U;

        BuildHomePage(http_body,
                      sizeof(http_body),
                      "LED encendido");
    }

    else if (strstr(payload_ptr, "GET /off ") != NULL)
    {
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
        led_state = 0U;

        BuildHomePage(http_body,
                      sizeof(http_body),
                      "LED apagado");
    }

    else if (strstr(payload_ptr, "GET /toggle ") != NULL)
    {
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        led_state = !led_state;

        BuildHomePage(http_body,
                      sizeof(http_body),
                      "LED invertido");
    }

    else if (strstr(payload_ptr, "GET /status ") != NULL)
    {
        if (led_state != 0U)
        {
            BuildHomePage(http_body,
                          sizeof(http_body),
                          "Estado actual: LED encendido");
        }
        else
        {
            BuildHomePage(http_body,
                          sizeof(http_body),
                          "Estado actual: LED apagado");
        }
    }

    else
    {
        BuildHomePage(http_body,
                      sizeof(http_body),
                      "Servidor web STM32 + ESP8266");
    }

    ESP8266_SendHttpResponse((uint8_t)link_id, http_body);
}

static void BuildHomePage(char *body, size_t size, const char *message)
{
    const char *led_text;

    if (led_state != 0U)
    {
        led_text = "ENCENDIDO";
    }
    else
    {
        led_text = "APAGADO";
    }

    snprintf(body,
             size,
             "<!DOCTYPE html>"
             "<html>"
             "<head>"
             "<meta charset=\"UTF-8\">"
             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
             "<title>STM32 Web Server</title>"
             "<style>"
             "body{font-family:Arial;text-align:center;margin-top:40px;}"
             "a{display:block;margin:12px auto;padding:12px;width:200px;"
             "background:#eeeeee;text-decoration:none;color:#000;border-radius:8px;}"
             "</style>"
             "</head>"
             "<body>"
             "<h1>Servidor web embebido</h1>"
             "<h2>STM32F446RE + ESP8266</h2>"
             "<p><strong>%s</strong></p>"
             "<p>Estado del LED: <strong>%s</strong></p>"
             "<a href=\"/on\">Encender LED</a>"
             "<a href=\"/off\">Apagar LED</a>"
             "<a href=\"/toggle\">Invertir LED</a>"
             "<a href=\"/status\">Consultar estado</a>"
             "</body>"
             "</html>",
             message,
             led_text);
}

static void ESP8266_SendHttpResponse(uint8_t link_id, const char *body)
{
    int body_len;
    int response_len;

    body_len = strlen(body);

    response_len = snprintf(http_response,
                            sizeof(http_response),
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/html; charset=utf-8\r\n"
                            "Connection: close\r\n"
                            "Content-Length: %d\r\n"
                            "\r\n"
                            "%s",
                            body_len,
                            body);

    if ((response_len <= 0) ||
        (response_len >= (int)sizeof(http_response)))
    {
        PC_Print("[ERROR] La respuesta HTTP supera el buffer disponible.\r\n");
        return;
    }

    snprintf(http_cmd,
             sizeof(http_cmd),
             "AT+CIPSEND=%u,%d",
             link_id,
             response_len);

    PC_Print("[STM32 -> ESP8266] ");
    PC_Print(http_cmd);
    PC_Print("\r\n");

    ESP_SendAT(http_cmd);

    /*
     * Para esta primera version didactica usamos un retardo simple.
     * El ESP8266 normalmente responde con '>' indicando que esta listo
     * para recibir los datos.
     */
    HAL_Delay(300);

    PC_Print("[STM32 -> ESP8266] Enviando respuesta HTTP\r\n");

    ESP_SendRaw(http_response);

    HAL_Delay(500);

    snprintf(http_cmd,
             sizeof(http_cmd),
             "AT+CIPCLOSE=%u",
             link_id);

    PC_Print("[STM32 -> ESP8266] ");
    PC_Print(http_cmd);
    PC_Print("\r\n");

    ESP_SendAT(http_cmd);
}



//Configurar ESP8266 como Access Point
static void ESP8266_ConfigSoftAP(void)
{
    PC_Print("\r\nConfigurando ESP8266 como Access Point...\r\n");

    PC_Print("[STM32 -> ESP8266] AT\r\n");
    ESP_SendAT("AT");
    HAL_Delay(1000);

    PC_Print("[STM32 -> ESP8266] ATE0\r\n");
    ESP_SendAT("ATE0");
    HAL_Delay(1000);

    PC_Print("[STM32 -> ESP8266] AT+CWMODE=2\r\n");
    ESP_SendAT("AT+CWMODE=2");
    HAL_Delay(1500);

    PC_Print("[STM32 -> ESP8266] AT+CWSAP=\"STM32_WEB\",\"12345678\",5,3\r\n");
    ESP_SendAT("AT+CWSAP=\"STM32_WEB\",\"12345678\",5,3");
    HAL_Delay(2000);

    PC_Print("[STM32 -> ESP8266] AT+CIPAP?\r\n");
    ESP_SendAT("AT+CIPAP?");
    HAL_Delay(1000);

    PC_Print("\r\nRed Wi-Fi creada.\r\n");
    PC_Print("SSID: STM32_WEB\r\n");
    PC_Print("Password: 12345678\r\n");
    PC_Print("Conectar el smartphone a esta red.\r\n\r\n");
}
//Encender y apagar el servidor TCP
static void ESP8266_ServerOn(void)
{
    PC_Print("\r\nHabilitando servidor TCP en puerto 80...\r\n");

    PC_Print("[STM32 -> ESP8266] AT+CIPSERVER=0\r\n");
    ESP_SendAT("AT+CIPSERVER=0");
    HAL_Delay(1000);

    PC_Print("[STM32 -> ESP8266] AT+CIPMUX=1\r\n");
    ESP_SendAT("AT+CIPMUX=1");
    HAL_Delay(1000);

    PC_Print("[STM32 -> ESP8266] AT+CIPSERVER=1,80\r\n");
    ESP_SendAT("AT+CIPSERVER=1,80");
    HAL_Delay(1000);

    PC_Print("\r\nServidor TCP habilitado en puerto 80.\r\n");
    PC_Print("Conectar el smartphone a la red STM32_WEB.\r\n");
    PC_Print("Abrir desde el navegador: http://192.168.4.1/\r\n\r\n");
}

static void ESP8266_ServerOff(void)
{
    PC_Print("\r\nApagando servidor TCP...\r\n");

    PC_Print("[STM32 -> ESP8266] AT+CIPSERVER=0\r\n");
    ESP_SendAT("AT+CIPSERVER=0");
    HAL_Delay(1000);

    PC_Print("Servidor TCP apagado.\r\n\r\n");
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
  MX_UART4_Init();
  /* USER CODE BEGIN 2 */

  PC_Print("\r\n====================================\r\n");
  PC_Print("Practica ESP8266 + STM32F446RE\r\n");
  PC_Print("Servidor web embebido HTTP\r\n");
  PC_Print("USART2: Cutecom / PC\r\n");
  PC_Print("UART4 : ESP8266\r\n");
  PC_Print("====================================\r\n");
  PC_Print("Escriba HELP para ver comandos.\r\n\r\n");

  HAL_UARTEx_ReceiveToIdle_IT(&huart2,
                              pc_rx_buffer,
                              PC_RX_SIZE);

  HAL_UARTEx_ReceiveToIdle_IT(&huart4,
                              esp_rx_buffer,
                              ESP_RX_SIZE);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if (pc_command_ready != 0U)
	  {
	      pc_command_ready = 0U;

	      ProcessPcCommand(pc_command);

	      memset(pc_command, 0, sizeof(pc_command));
	  }

	  if (esp_message_ready != 0U)
	  {
	      esp_message_ready = 0U;

	      ProcessEspMessage(esp_message);

	      memset(esp_message, 0, sizeof(esp_message));
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
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */

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
