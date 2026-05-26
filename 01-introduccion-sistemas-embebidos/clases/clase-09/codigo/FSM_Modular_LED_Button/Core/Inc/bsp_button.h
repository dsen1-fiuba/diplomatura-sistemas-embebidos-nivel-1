/*
 * bsp_button.h
 *
 *  Created on: May 24, 2026
 *      Author: mariano
 */

#ifndef INC_BSP_BUTTON_H_
#define INC_BSP_BUTTON_H_

typedef enum
{
    BSP_BUTTON_RELEASED = 0,
    BSP_BUTTON_PRESSED
} bsp_button_state_t;

bsp_button_state_t bsp_button_read(void);

#endif /* INC_BSP_BUTTON_H_ */
