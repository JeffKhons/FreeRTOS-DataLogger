#ifndef CLI_TASK_H
#define CLI_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "ring_buffer.h"

/* 定義日誌紀錄結構體 */
typedef struct {
    uint32_t seq;
    uint32_t timestamp;
    int32_t temp_x100;
    uint8_t flags;
} CLI_LogRecord_t;

/* --- Link-time Hooks (需在 porting 層實作) --- */
extern void CLI_Write(const char *str);
extern bool CLI_PortReadTempX100(int32_t *out_temp);         /* 支援錯誤回傳 */
extern uint32_t CLI_PortLogCount(void);
extern bool CLI_PortLogRead(uint32_t idx, CLI_LogRecord_t *rec); /* 換成強型別結構體指標 */

void CLI_Init(void);
void CLI_Update(RingBuffer_t *rx_buf);
void CLI_Printf(const char *format, ...);

/* 提供給 stat 查閱的內部計數器 */
extern uint32_t cli_overflow_count;

#endif /* CLI_TASK_H */