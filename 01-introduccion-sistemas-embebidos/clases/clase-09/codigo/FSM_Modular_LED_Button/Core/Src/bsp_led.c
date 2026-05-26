/*
 * bsp_led.c
 *
 *  Created on: May 24, 2026
 *      Author: mariano
 */


#include "bsp_led.h"
#include "main.h"

void bsp_led_init(void)
{
    bsp_led_apagar();
}

void bsp_led_encender(void)
{
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
}

void bsp_led_apagar(void)
{
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
}

void bsp_led_toggle(void)
{
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
}
