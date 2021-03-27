#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "main.h"
#include "hex_parse.h"

void boot_clockInit(void)
{
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO
        | RCC_APB2Periph_GPIOC, ENABLE);
}

void boot_gpioInit(void)
{
    /* Init button pin */
    GPIO_InitTypeDef boot_initstr;

    boot_initstr.GPIO_Pin = BUTTON_PIN;
    boot_initstr.GPIO_Mode = GPIO_Mode_IPU;
    boot_initstr.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BUTTON_PORT, &boot_initstr);

    boot_initstr.GPIO_Pin = LED_PIN;
    boot_initstr.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(LED_PORT, &boot_initstr);

    /* Init uart pins */
    boot_initstr.GPIO_Pin = BOOT_UART_TX;
    boot_initstr.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(BOOT_UART_PORT, &boot_initstr);

    boot_initstr.GPIO_Pin = BOOT_UART_RX;
    boot_initstr.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(BOOT_UART_PORT, &boot_initstr);
}

void boot_UartInit(void)
{
    USART_InitTypeDef boot_initstr;

    /* Init uart */
    boot_initstr.USART_BaudRate = 9600;
    boot_initstr.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    boot_initstr.USART_Parity = USART_Parity_No;
    boot_initstr.USART_StopBits = USART_StopBits_1;
    boot_initstr.USART_WordLength = USART_WordLength_8b;
    boot_initstr.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(BOOT_UART, &boot_initstr);

    /* Init nvic and enable interrupt */
    USART_ITConfig(BOOT_UART, USART_IT_RXNE, ENABLE);
    NVIC_SetPriority(USART1_IRQn, 0);
    NVIC_EnableIRQ(USART1_IRQn);

    /* Enable uart1 */
    USART_Cmd(BOOT_UART, ENABLE);
}

int main(void)
{
    boot_clockInit();
    boot_gpioInit();
    boot_UartInit();

    if (GPIO_ReadInputDataBit(BUTTON_PORT, BUTTON_PIN) == RESET)
    {
        /* Enter bootloader mode, load program to ram and run to program */
        while (1)
        {
            Hex_receive();
        }
    }

    /* Idle */
    GPIO_SetBits(LED_PORT, LED_PIN);
    // USART_SendData(BOOT_UART, 'a');
    // while (USART_GetFlagStatus(BOOT_UART, USART_FLAG_TXE) != 1);
    // USART_SendData(BOOT_UART, 'b');
    // while (USART_GetFlagStatus(BOOT_UART, USART_FLAG_TXE) != 1);
    // USART_SendData(BOOT_UART, 'c');
    // while (USART_GetFlagStatus(BOOT_UART, USART_FLAG_TXE) != 1);
    while (1)
    {
    }

    return 0;
}