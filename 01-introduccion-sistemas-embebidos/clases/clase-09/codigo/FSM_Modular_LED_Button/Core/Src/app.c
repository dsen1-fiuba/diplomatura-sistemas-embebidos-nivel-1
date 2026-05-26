/*
 * app.c
 *
 *  Created on: May 24, 2026
 *      Author: mariano
 */


#include "app.h"
#include "led_fsm.h"
#include "bsp_led.h"
#include "bsp_button.h"
#include "main.h"

#define DEBOUNCE_TIME_MS 40U

static bsp_button_state_t button_last_raw_state;
static bsp_button_state_t button_stable_state;
static uint32_t button_last_change_ms;

static evento_t app_get_button_event(void);

void app_init(void)
{
    bsp_led_init();
    led_fsm_init();

    button_last_raw_state = bsp_button_read();
    button_stable_state = button_last_raw_state;
    button_last_change_ms = HAL_GetTick();
}

void app_loop(void)
{
    evento_t evento;

    evento = app_get_button_event();

    if (evento != EVENTO_NINGUNO)
    {
        led_fsm_procesar_evento(evento);
    }

    led_fsm_actualizar();
}

static evento_t app_get_button_event(void)
{
    bsp_button_state_t button_raw_state;
    uint32_t tiempo_actual_ms;

    button_raw_state = bsp_button_read();
    tiempo_actual_ms = HAL_GetTick();

    if (button_raw_state != button_last_raw_state)
    {
        button_last_raw_state = button_raw_state;
        button_last_change_ms = tiempo_actual_ms;
    }

    if ((tiempo_actual_ms - button_last_change_ms) >= DEBOUNCE_TIME_MS)
    {
        if (button_raw_state != button_stable_state)
        {
            button_stable_state = button_raw_state;

            if (button_stable_state == BSP_BUTTON_PRESSED)
            {
                return EVENTO_BOTON_PRESIONADO;
            }
        }
    }

    return EVENTO_NINGUNO;
}
