/**
  ******************************************************************************
  * @file    app_logger.h
  * @brief   Data Logger Application Layer Header
  ******************************************************************************
  */
#ifndef APP_LOGGER_H
#define APP_LOGGER_H

#include <stdint.h>
#include <stdbool.h>
#include "cli_task.h" // 需要引入 CLI_LogRecord_t 結構

/* Public Variables ----------------------------------------------------------*/
extern uint32_t flash_record_count; // 開放給 main.c 開機搜尋時寫入

/* Public Function Prototypes ------------------------------------------------*/
void Action_ReadTemperature(void);
void vSensorTask(void *pvParameters);
void vStorageTask(void *pvParameters);
uint32_t CLI_PortLogCount(void);
bool CLI_PortLogRead(uint32_t idx, CLI_LogRecord_t *rec);
uint32_t Logger_QueueDropped(void);
uint32_t Logger_FlashWriteErrors(void);

#endif /* APP_LOGGER_H */
