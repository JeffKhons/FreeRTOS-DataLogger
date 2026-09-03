#ifndef APP_RESOURCES_H
#define APP_RESOURCES_H

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

/*
 * Application-owned serialisation for I2C1 (LM75 and SSD1306).
 * Device tasks take this mutex around a complete device transaction.
 */
extern SemaphoreHandle_t xI2CMutex;
extern SemaphoreHandle_t xFlashMutex;
extern QueueHandle_t xTemperatureQueue;

#endif /* APP_RESOURCES_H */
