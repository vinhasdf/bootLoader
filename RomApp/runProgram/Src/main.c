#include "main.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"

void app_clockInit(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
}

void app_gpioInit(void)
{
    /* Init button pin */
    GPIO_InitTypeDef boot_initstr;

    boot_initstr.GPIO_Pin = LED_PIN;
    boot_initstr.GPIO_Mode = GPIO_Mode_Out_PP;
    boot_initstr.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_PORT, &boot_initstr);
}

void main_app(void)
{
    app_clockInit();
    app_gpioInit();

    /* Init timer */
    SysTick_Config(SystemCoreClock / 1000);
    GPIO_SetBits(LED_PORT, LED_PIN);
    
    while (1)
    {
    }
}

int main(void)
{
    uint32_t sp_ptr = (uint32_t)(&_estack);
    /* set Vector table entry */
    SCB->VTOR = (uint32_t)(&_APP_VECTOR_TABLE);
    /* set stack pointer */
    __set_CONTROL(0);
    __set_MSP(sp_ptr);

    main_app();
    return 0;
}
