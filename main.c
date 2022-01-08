/**
 * Basic Blink app for Silabs Wireless Gecko platforms.
 *
 * Configure platform, logging to UART, start kernel and keep blinking LEDs.
 *
 * Copyright Thinnect Inc. 2019
 * Copyright ProLab TTÜ 2022
 * @license MIT
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

#include "loglevels.h"
#define __MODUUL__ "main"
#define __LOG_LEVEL__ (LOG_LEVEL_main & BASE_LOG_LEVEL)
#include "log.h"

// Include the information header binary
#include "incbin.h"
INCBIN(Header, "header.bin");

// Heartbeat thread, initialize GPIO and print heartbeat messages.
void hp_loop ()
{
    #define ESW_GPIO_HB_DELAY 10 // Heartbeat message delay, seconds
    
    // TODO Initialize GPIO.
    
    for (;;)
    {
        osDelay(ESW_GPIO_HB_DELAY*osKernelGetTickFreq());
        info1("Heartbeat");
    }
}

// TODO LED toggle thread.

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
    const osThreadAttr_t hp_thread_attr = { .name = "app" };
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
