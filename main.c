/**
 * @brief Example usage of GPIO peripheral. Three LEDs are toggled using 
 * GPIO functionality. A hardware-to-software interrupt is set up and 
 * triggered by a button switch.
 *
 * The tsb0 board has three LEDs (red, green, blue) connected to ports
 * PB11, PB12, PA5 respectively. The button switch is connected to port
 * PF4. LED and button locations (pin and port numbers) can be found from 
 * the tsb0 board wiring schematics.
 *
 * EFR32MG12 Wireless Gecko Reference Manual (GPIO p1105)
 * https://www.silabs.com/documents/public/reference-manuals/efr32xg12-rm.pdf
 *
 * GPIO API documentation 
 * https://docs.silabs.com/mcu/latest/efr32mg12/group-GPIO
 * 
 * ARM RTOS API
 * https://arm-software.github.io/CMSIS_5/RTOS2/html/group__CMSIS__RTOS.html
 * 
 * Copyright Thinnect Inc. 2019
 * Copyright ProLab TTÜ 2022
 * @license MIT
 * @author Johannes Ehala
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "retargetserial.h"
#include "cmsis_os2.h"
#include "platform.h"

#include "SignatureArea.h"
#include "DeviceSignature.h"

#include "loggers_ext.h"
#include "logger_fwrite.h"

#include "em_cmu.h"
#include "em_gpio.h"

#include "loglevels.h"
#define __MODUUL__ "main"
#define __LOG_LEVEL__ (LOG_LEVEL_main & BASE_LOG_LEVEL)
#include "log.h"

// Include the information header binary
#include "incbin.h"
INCBIN(Header, "header.bin");

#define ESWGPIO_LED0_PORT       gpioPortB
#define ESWGPIO_LED1_PORT       gpioPortB
#define ESWGPIO_LED2_PORT       gpioPortA
#define ESWGPIO_BUTTON_PORT     gpioPortF

#define ESWGPIO_LED0_PIN        11     // Red
#define ESWGPIO_LED1_PIN        12     // Green
#define ESWGPIO_LED2_PIN        5      // Blue
#define ESWGPIO_BUTTON_PIN      4

#define ESWGPIO_LED0_DELAY      333   // OS ticks
#define ESWGPIO_LED1_DELAY      1000  // OS ticks
#define ESWGPIO_LED2_DELAY      1500  // OS ticks

#define ESWGPIO_EXTI_INDEX      4 // External interrupt number 4.
#define ESWGPIO_EXTI_IF         0x00000010UL // Interrupt flag for external interrupt number 4.


static void led0_loop (void *args);
static void led1_loop (void *args);
static void led2_loop (void *args);
static void button_loop (void *args);
static osThreadId_t buttonThreadId;
static const uint32_t buttonExtIntThreadFlag = 0x00000001;

static void gpio_external_interrupt_init (GPIO_Port_TypeDef port, uint32_t pin, uint32_t exti_if, uint16_t exti_num);
static void gpio_external_interrupt_enable (uint32_t if_exti);

// Heartbeat thread, initialize GPIO and print heartbeat messages.
void hp_loop ()
{
    #define ESWGPIO_HB_DELAY 10 // Heartbeat message delay, seconds
    
    // Initialize GPIO peripheral.
    CMU_ClockEnable(cmuClock_GPIO, true);
    
    // Configure LED pins as push-pull output pins.
    GPIO_PinModeSet(ESWGPIO_LED0_PORT, ESWGPIO_LED0_PIN, gpioModePushPull, 0);
    GPIO_PinModeSet(ESWGPIO_LED1_PORT, ESWGPIO_LED1_PIN, gpioModePushPull, 0);
    GPIO_PinModeSet(ESWGPIO_LED2_PORT, ESWGPIO_LED2_PIN, gpioModePushPull, 0);
    
    // Create threads to toggle each LED.
    const osThreadAttr_t led0_thread_attr = { .name = "led0" };
    osThreadNew(led0_loop, NULL, &led0_thread_attr);
    const osThreadAttr_t led1_thread_attr = { .name = "led1" };
    osThreadNew(led1_loop, NULL, &led1_thread_attr);
    const osThreadAttr_t led2_thread_attr = { .name = "led2" };
    osThreadNew(led2_loop, NULL, &led2_thread_attr);
    
    // Configure button pin for external interrupts. 
    gpio_external_interrupt_init(ESWGPIO_BUTTON_PORT, ESWGPIO_BUTTON_PIN, ESWGPIO_EXTI_IF, ESWGPIO_EXTI_INDEX);
    
    // Create thread for handling external interrupts.
    const osThreadAttr_t button_thread_attr = { .name = "button" };
    buttonThreadId = osThreadNew(button_loop, NULL, &button_thread_attr);
    
    // Enanble external interrupts from button.
    gpio_external_interrupt_enable(ESWGPIO_EXTI_IF);
    
    for (;;)
    {
        osDelay(ESWGPIO_HB_DELAY*osKernelGetTickFreq());
        info1("Heartbeat");
    }
}

// LED toggle threads.
static void led0_loop (void *args)
{
    uint32_t led_cnt = 0;
    for (;;)
    {
        osDelay(ESWGPIO_LED0_DELAY);
        
        if(led_cnt%2)GPIO_PinOutClear(ESWGPIO_LED0_PORT, ESWGPIO_LED0_PIN);
        else GPIO_PinOutSet(ESWGPIO_LED0_PORT, ESWGPIO_LED0_PIN);
        
        led_cnt++;
    }
}

static void led1_loop (void *args)
{
    for (;;)
    {
        osDelay(ESWGPIO_LED1_DELAY);
        GPIO_PinOutToggle(ESWGPIO_LED1_PORT, ESWGPIO_LED1_PIN);
    }
}

static void led2_loop (void *args)
{
    for (;;)
    {
        osDelay(ESWGPIO_LED2_DELAY);
        if(GPIO_PinOutGet(ESWGPIO_LED2_PORT, ESWGPIO_LED2_PIN))GPIO_PinOutSet(ESWGPIO_LED2_PORT, ESWGPIO_LED2_PIN);
        else GPIO_PinOutClear(ESWGPIO_LED2_PORT, ESWGPIO_LED2_PIN);
    }
}

static void button_loop (void *args)
{
    for (;;)
    {
        // Wait for external interrupt signal from button.
        osThreadFlagsClear(buttonExtIntThreadFlag);
        osThreadFlagsWait(buttonExtIntThreadFlag, osFlagsWaitAny, osWaitForever); // Flags are automatically cleared
        info1("Button");
    }
}

int logger_fwrite_boot (const char *ptr, int len)
{
    fwrite(ptr, len, 1, stdout);
    fflush(stdout);
    return len;
}

int main ()
{
    PLATFORM_Init();

    // Configure log message output
    RETARGET_SerialInit();
    log_init(BASE_LOG_LEVEL, &logger_fwrite_boot, NULL);

    info1("ESW-GPIO "VERSION_STR" (%d.%d.%d)", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

    // Initialize OS kernel.
    osKernelInitialize();

    // Create a thread.
    const osThreadAttr_t hp_thread_attr = { .name = "hb" };
    osThreadNew(hp_loop, NULL, &hp_thread_attr);

    if (osKernelReady == osKernelGetState())
    {
        // Switch to a thread-safe logger
        logger_fwrite_init();
        log_init(BASE_LOG_LEVEL, &logger_fwrite, NULL);

        // Start the kernel
        osKernelStart();
    }
    else
    {
        err1("!osKernelReady");
    }

    for(;;);
}

/**
 * @brief Initialize GPIO port and pin for external interrupts and enable 
 * GPIO hardware interrupts (NVIC). 
 */
static void gpio_external_interrupt_init (GPIO_Port_TypeDef port, uint32_t pin, uint32_t exti_if, uint16_t exti_num)
{
    // Configure pin. Input with glitch filtering and pull-up.
    GPIO_PinModeSet(port, pin, gpioModeInputPullFilter, 1);
    
    // Configure external interrupts.
    GPIO_IntDisable(exti_if); // Disable before config to avoid unwanted interrupt triggering.
    GPIO_ExtIntConfig(port, pin, exti_num, false, true, false); // Port, pin, EXTI number, rising edge, falling edge, enabled.
    GPIO_InputSenseSet(GPIO_INSENSE_INT, GPIO_INSENSE_INT);
}

static void gpio_external_interrupt_enable (uint32_t exti_if)
{
    GPIO_IntClear(exti_if);
    
    NVIC_EnableIRQ(GPIO_EVEN_IRQn);
    NVIC_SetPriority(GPIO_EVEN_IRQn, 3);

    GPIO_IntEnable(exti_if);
}

void GPIO_EVEN_IRQHandler (void)
{
    // Get all pending and enabled interrupts.
    uint32_t pending = GPIO_IntGetEnabled();
    
    // Check if button interrupt is enabled
    if (pending & ESWGPIO_EXTI_IF)
    {
        // Clear interrupt flag.
        GPIO_IntClear(ESWGPIO_EXTI_IF);

        // Trigger button thread to resume.
        osThreadFlagsSet(buttonThreadId, buttonExtIntThreadFlag);
    }
    else ; // This was not a button interrupt.
}
