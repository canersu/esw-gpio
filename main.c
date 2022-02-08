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
 * EFR32 Application Note on GPIO
 * https://www.silabs.com/documents/public/application-notes/an0012-efm32-gpio.pdf
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

#include "em_gpio.h"
#include "em_cmu.h"


#include "loglevels.h"
#define __MODUUL__ "main"
#define __LOG_LEVEL__ (LOG_LEVEL_main & BASE_LOG_LEVEL)
#include "log.h"

// Include the information header binary
#include "incbin.h"
INCBIN(Header, "header.bin");

void led_one();
void buzzer_tone();

// Heartbeat thread, initialize GPIO and print heartbeat messages.
void hp_loop ()
{
    #define ESWGPIO_HB_DELAY 10 // Heartbeat message delay, seconds
    
    // TODO Initialize GPIO.
    CMU_ClockEnable(cmuClock_GPIO, true);
    GPIO_PinModeSet(gpioPortB, 12, gpioModePushPull, 0);


// TODO LED toggle thread.
    const osThreadAttr_t LED1_thread_attr = { .name = "LED1" };
    osThreadNew(led_one, NULL, &LED1_thread_attr);
    
    for (;;)
    {
        osDelay(ESWGPIO_HB_DELAY*osKernelGetTickFreq());
        info1("Heartbeat");
    }
}


void led_one()
{
    for(;;)
    {
        osDelay(300);
        GPIO_PinOutToggle(gpioPortB, 12);
    }
}

void ambulance_sound()
{
    for(int i=0; i<100; i++)
    {
        osDelay(2);
        GPIO_PinOutToggle(gpioPortA, 0);
    }

    osDelay(50);
    for(int i=0; i<50; i++)
    {
        osDelay(4);
        GPIO_PinOutToggle(gpioPortA, 0);
    }
    osDelay(50);
}


void buzzer_tone()
{
    for(;;)
    {

        if (GPIO_PinInGet(gpioPortF, 4) == 0)
        {
            // GPIO_PinOutToggle(gpioPortB, 11);
            // osDelay(200);
            ambulance_sound();
        }
    }
}

// TODO Button interrupt thread.

void buzzer_loop ()
{
    #define ESWGPIO_HB_DELAY 10 // Heartbeat message delay, seconds
    
    // TODO Initialize GPIO.
    CMU_ClockEnable(cmuClock_GPIO, true);
    // Buzzer
    GPIO_PinModeSet(gpioPortA, 0, gpioModePushPull, 0);
    // LED 1
    GPIO_PinModeSet(gpioPortB, 11, gpioModePushPull, 0);
    // Button
    GPIO_PinModeSet(gpioPortF, 4, gpioModeInputPull , 1);


// TODO Buzzer thread.
    const osThreadAttr_t BUZZER_thread_attr = { .name = "BUZZER" };
    osThreadNew(buzzer_tone, NULL, &BUZZER_thread_attr);
    
    for (;;)
    {
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
    const osThreadAttr_t hp_thread_attr = { .name = "hp" };
    osThreadNew(hp_loop, NULL, &hp_thread_attr);

    // Create a thread.
    const osThreadAttr_t buzzer_thread_attr = { .name = "buzzer_tone" };
    osThreadNew(buzzer_loop, NULL, &buzzer_thread_attr);

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
