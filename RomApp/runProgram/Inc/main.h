#ifndef MAIN_H
#define MAIN_H

#include "stm32f10x.h"
#define LED_PIN                     GPIO_Pin_13
#define LED_PORT                    GPIOC

extern uint32_t                     _estack;
extern uint32_t                     _APP_VECTOR_TABLE;

#endif  /* MAIN_H */