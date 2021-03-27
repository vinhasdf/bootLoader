#ifndef MAIN_H
#define MAIN_H

#define LED_PIN                     GPIO_Pin_13
#define LED_PORT                    GPIOC
#define BUTTON_PIN                  GPIO_Pin_0
#define BUTTON_PORT                 GPIOA
#define BOOT_UART_PORT              GPIOA
#define BOOT_UART_TX                GPIO_Pin_9
#define BOOT_UART_RX                GPIO_Pin_10
#define BOOT_UART                   USART1

/* clock name */
#define UART_CLOCK                  RCC_APB2Periph_USART1
#define LED_CLOCK                   RCC_APB2Periph_GPIOC
#define BUTTON_CLOCK                RCC_APB2Periph_GPIOA
#define UART_IRQ                    USART1_IRQn

extern uint32_t                     _APP_VECTOR_TABLE;
extern uint32_t                     _APP_ROM_SIZE;

#endif  /* MAIN_H */