/**
 * \file            main.c
 * \brief           Main file
 */

/*
 * Copyright (c) 2019 Tilen MAJERLE
 *  
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software, 
 * and to permit persons to whom the Software is furnished to do so, 
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Author:          Tilen MAJERLE <tilen@majerle.eu>
 * Version:         $_version_$
 */
#include "main.h"
#include "cmsis_os.h"

#include "esp/esp.h"
#include "station_manager.h"
#include "netconn_server.h"

static void LL_Init(void);
void SystemClock_Config(void);
static void USART_Printf_Init(void);

static void init_thread(void* arg);

static espr_t esp_callback_func(esp_evt_t* evt);

/**
 * \brief           Program entry point
 */
int
main(void) {
    LL_Init();                                  /* Reset of all peripherals, initializes the Flash interface and the Systick. */
    SystemClock_Config();                       /* Configure the system clock */
    USART_Printf_Init();                        /* Init USART for printf */
    
    printf("Application running on STM32F723E-Discovery!\r\n");
    
    /* Initialize, create first thread and start kernel */
    osKernelInitialize();
    const osThreadAttr_t attr = {
            .stack_size = 512
    };
    osThreadNew(init_thread, NULL, &attr);
    osKernelStart();
    
    while (1) {}
}

/**
 * \brief           Initialization thread
 * \param[in]       arg: Thread argument
 */
static void
init_thread(void* arg) {
    /* Initialize ESP with default callback function */
    printf("Initializing ESP-AT Lib\r\n");
    if (esp_init(esp_callback_func, 1) != espOK) {
        printf("Cannot initialize ESP-AT Lib!\r\n");
    } else {
        printf("ESP-AT Lib initialized!\r\n");
    }

    /*
     * Connect to access point.
     *
     * Try unlimited time until access point accepts up.
     * Check for station_manager.c to define preferred access points ESP should connect to
     */
    connect_to_preferred_access_point(1);
    
    /*
     * Create a Netconn based server in separated thread
     *
     * Process all the incoming connections with separate thread
     */
    esp_sys_thread_create(NULL, "netconn_server", (esp_sys_thread_fn)netconn_server_thread, NULL, 512, ESP_SYS_THREAD_PRIO);
	osThreadExit();
}

/**
 * \brief           Event callback function for ESP stack
 * \param[in]       evt: Event information with data
 * \return          espOK on success, member of \ref espr_t otherwise
 */
static espr_t
esp_callback_func(esp_evt_t* evt) {
    switch (esp_evt_get_type(evt)) {
        case ESP_EVT_AT_VERSION_NOT_SUPPORTED: {
            esp_sw_version_t v_min, v_curr;

            esp_get_min_at_fw_version(&v_min);
            esp_get_current_at_fw_version(&v_curr);

            printf("Current ESP8266 AT version is not supported by library!\r\n");
            printf("Minimum required AT version is: %d.%d.%d\r\n", (int)v_min.major, (int)v_min.minor, (int)v_min.patch);
            printf("Current AT version is: %d.%d.%d\r\n", (int)v_curr.major, (int)v_curr.minor, (int)v_curr.patch);
            break;
        }
        case ESP_EVT_INIT_FINISH: {
            printf("Library initialized!\r\n");
            break;
        }
        case ESP_EVT_RESET_DETECTED: {
            printf("Device reset detected!\r\n");
            break;
        }
        default: break;
    }
    return espOK;
}

/**
 * \brief           Low-Layer initialization
 */
static void
LL_Init(void) {
    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
    NVIC_SetPriority(MemoryManagement_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
    NVIC_SetPriority(BusFault_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
    NVIC_SetPriority(UsageFault_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
    NVIC_SetPriority(SVCall_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
    NVIC_SetPriority(DebugMonitor_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
    NVIC_SetPriority(PendSV_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
    NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));
}

/**
 * \brief           System clock configuration
 */
void
SystemClock_Config(void) {
    /* Configure flash latency */
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_7);
    if (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_7) {
        while (1) {}
    }
    
    /* Set voltage scaling */
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    LL_PWR_EnableOverDriveMode();
    
    /* Enable HSI */
    LL_RCC_HSI_SetCalibTrimming(16);
    LL_RCC_HSI_Enable();
    while (LL_RCC_HSI_IsReady() != 1) {}
    
    /* Configure PLL */
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_8, 216, LL_RCC_PLLP_DIV_2);
    LL_RCC_PLL_Enable();
    while (LL_RCC_PLL_IsReady() != 1) {}
    
    /* Set prescalers */
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
    
    /* Configure system clock */
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL) {}
    
    /* Configure systick */
    LL_Init1msTick(216000000);
    LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
    LL_SYSTICK_EnableIT();
    LL_SetSystemCoreClock(216000000);

    /* SysTick_IRQn interrupt configuration */
    NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));
    LL_RCC_SetUSARTClockSource(LL_RCC_USART1_CLKSOURCE_PCLK2);
}

/**
 * \brief           Init USART6 for printf output
 */
static void
USART_Printf_Init(void) {
    LL_USART_InitTypeDef USART_InitStruct;
    LL_GPIO_InitTypeDef GPIO_InitStruct;

    /* Peripheral clock enable */
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART6);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
    
    /*
     * USART6 GPIO Configuration  
     *
     * PC6  ------> USART6_TX
     * PC7  ------> USART6_RX
     */
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_8;
    
    GPIO_InitStruct.Pin = LL_GPIO_PIN_6;
    LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LL_GPIO_PIN_7;
    LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    USART_InitStruct.BaudRate = 921600;
    USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
    USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
    USART_InitStruct.Parity = LL_USART_PARITY_NONE;
    USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
    USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
    USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
    LL_USART_Init(USART6, &USART_InitStruct);

    LL_USART_ConfigAsyncMode(USART6);           /* Configure USART in async mode */
    LL_USART_Enable(USART6);                    /* Enable USART */
}

/**
 * \brief           Printf character handler
 * \param[in]       ch: Character to send
 * \param[in]       f: File pointer
 * \return          Written character
 */
#ifdef __GNUC__
int __io_putchar(int ch) {
#else
int fputc(int ch, FILE* fil) {
#endif
    LL_USART_TransmitData8(USART6, (uint8_t)ch);
    while (!LL_USART_IsActiveFlag_TXE(USART6)) {}
    return ch;
}
