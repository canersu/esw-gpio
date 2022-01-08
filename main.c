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

#define ESWGPIO_LED0_PORT gpioPortB
#define ESWGPIO_LED1_PORT gpioPortB
#define ESWGPIO_LED2_PORT gpioPortA

#define ESWGPIO_LED0_PIN 11     // Red
#define ESWGPIO_LED1_PIN 12     // Green
#define ESWGPIO_LED2_PIN 5      // Blue

#define ESWGPIO_LED0_DELAY 333   // OS ticks
#define ESWGPIO_LED1_DELAY 1000  // OS ticks
#define ESWGPIO_LED2_DELAY 1500  // OS ticks

static void led0_loop (void *args);
static void led1_loop (void *args);
static void led2_loop (void *args);

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
        GPIO_PinOutToggle(ESWGPIO_LED2_PORT, ESWGPIO_LED2_PIN);
    }
}

// TODO Button interrupt thread.

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
