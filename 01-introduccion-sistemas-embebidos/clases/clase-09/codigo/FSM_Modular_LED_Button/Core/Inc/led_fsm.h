/*
 * led_fsm.h
 *
 *  Created on: May 24, 2026
 *      Author: mariano
 */

#ifndef INC_LED_FSM_H_
#define INC_LED_FSM_H_

typedef enum
{
    EVENTO_NINGUNO = 0,
    EVENTO_BOTON_PRESIONADO,
    EVENTO_TICK_TIMER
} evento_t;

void led_fsm_init(void);
void led_fsm_procesar_evento(evento_t evento);
void led_fsm_actualizar(void);

#endif /* INC_LED_FSM_H_ */
