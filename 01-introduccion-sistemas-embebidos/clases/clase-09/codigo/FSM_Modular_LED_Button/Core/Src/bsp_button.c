/*
 * bsp_button.c
 *
 *  Created on: May 24, 2026
 *      Author: mariano
 */


#include "bsp_button.h"
#include "main.h"

bsp_button_state_t bsp_button_read(void)
{
    GPIO_PinState pin_state;

    pin_state = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);

    if (pin_state == GPIO_PIN_RESET)
    {
        return BSP_BUTTON_PRESSED;
    }

    return BSP_BUTTON_RELEASED;
}
