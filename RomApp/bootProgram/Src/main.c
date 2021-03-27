#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "main.h"
#include "hex_parse.h"

void boot_clockInit(void)
{
    RCC_APB2PeriphClockCmd(
        UART_CLOCK | LED_CLOCK | RCC_APB2Periph_AFIO
        | BUTTON_CLOCK, ENABLE);
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
    NVIC_SetPriority(UART_IRQ, 0);
    NVIC_EnableIRQ(UART_IRQ);

    /* Enable uart1 */
    USART_Cmd(BOOT_UART, ENABLE);
}

int main(void)
{
    uint32_t *ptrVector;
    void (*ptrReset)(void);
    uint8_t isBoot = 1;
    boot_clockInit();
    boot_gpioInit();
    boot_UartInit();

    if (GPIO_ReadInputDataBit(BUTTON_PORT, BUTTON_PIN) == RESET)
    {
        /* Enter bootloader mode, load program to ram and run to program */
        while (isBoot)
        {
            isBoot = Hex_receive();
        }
    }

    /* Start the app */
    // GPIO_SetBits(LED_PORT, LED_PIN);
    NVIC_DisableIRQ(UART_IRQ);
    USART_ITConfig(BOOT_UART, USART_IT_RXNE, DISABLE);
    USART_Cmd(BOOT_UART, DISABLE);
    USART_DeInit(BOOT_UART);
    GPIO_DeInit(LED_PORT);
    GPIO_DeInit(BUTTON_PORT);
    GPIO_AFIODeInit();
    RCC_APB2PeriphClockCmd(
        UART_CLOCK | LED_CLOCK | RCC_APB2Periph_AFIO
        | BUTTON_CLOCK, DISABLE);

    ptrVector = &_APP_VECTOR_TABLE;
    ptrReset = (void (*)(void))(ptrVector[1]);    // reset handler address
    ptrReset();         // start application
    while (1)
    {
    }

    return 0;
}
