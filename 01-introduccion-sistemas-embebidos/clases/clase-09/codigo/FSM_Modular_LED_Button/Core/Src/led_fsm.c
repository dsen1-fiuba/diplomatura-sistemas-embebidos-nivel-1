/*
 * led_fsm.c
 *
 *  Created on: May 24, 2026
 *      Author: mariano
 */


#include "led_fsm.h"
#include "bsp_led.h"
#include "main.h"

typedef enum
{
    ESTADO_APAGADO = 0,
    ESTADO_PARPADEO_LENTO,
    ESTADO_PARPADEO_RAPIDO
} estado_led_t;

static estado_led_t estado_actual;
static uint32_t ultimo_cambio_ms;

static void led_fsm_entrar_estado(estado_led_t nuevo_estado);

void led_fsm_init(void)
{
    led_fsm_entrar_estado(ESTADO_APAGADO);
}

void led_fsm_procesar_evento(evento_t evento)
{
    if (evento != EVENTO_BOTON_PRESIONADO)
    {
        return;
    }

    switch (estado_actual)
    {
        case ESTADO_APAGADO:
            led_fsm_entrar_estado(ESTADO_PARPADEO_LENTO);
            break;

        case ESTADO_PARPADEO_LENTO:
            led_fsm_entrar_estado(ESTADO_PARPADEO_RAPIDO);
            break;

        case ESTADO_PARPADEO_RAPIDO:
            led_fsm_entrar_estado(ESTADO_APAGADO);
            break;

        default:
            led_fsm_entrar_estado(ESTADO_APAGADO);
            break;
    }
}

void led_fsm_actualizar(void)
{
    uint32_t tiempo_actual_ms;
    uint32_t periodo_ms;

    tiempo_actual_ms = HAL_GetTick();

    switch (estado_actual)
    {
        case ESTADO_APAGADO:
            bsp_led_apagar();
            break;

        case ESTADO_PARPADEO_LENTO:
            periodo_ms = 1000U;

            if ((tiempo_actual_ms - ultimo_cambio_ms) >= periodo_ms)
            {
                ultimo_cambio_ms = tiempo_actual_ms;
                bsp_led_toggle();
            }
            break;

        case ESTADO_PARPADEO_RAPIDO:
            periodo_ms = 200U;

            if ((tiempo_actual_ms - ultimo_cambio_ms) >= periodo_ms)
            {
                ultimo_cambio_ms = tiempo_actual_ms;
                bsp_led_toggle();
            }
            break;

        default:
            led_fsm_entrar_estado(ESTADO_APAGADO);
            break;
    }
}

static void led_fsm_entrar_estado(estado_led_t nuevo_estado)
{
    estado_actual = nuevo_estado;
    ultimo_cambio_ms = HAL_GetTick();

    switch (estado_actual)
    {
        case ESTADO_APAGADO:
            bsp_led_apagar();
            break;

        case ESTADO_PARPADEO_LENTO:
        case ESTADO_PARPADEO_RAPIDO:
            bsp_led_encender();
            break;

        default:
            estado_actual = ESTADO_APAGADO;
            bsp_led_apagar();
            break;
    }
}
