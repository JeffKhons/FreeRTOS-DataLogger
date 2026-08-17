#ifndef CLI_TASK_H
#define CLI_TASK_H

#include "ring_buffer.h"

/* --- Link-time Hooks (需在 porting 層實作) --- */
extern void CLI_Write(const char *str);
extern int32_t CLI_PortReadTempX100(void);
extern uint32_t CLI_PortLogCount(void);
extern bool CLI_PortLogRead(uint32_t idx, void *rec); // rec 型別依日誌結構而定

void CLI_Init(void);
void CLI_Update(RingBuffer_t *rx_buf);
void CLI_Printf(const char *format, ...);

/* 提供給 stat 查閱的內部計數器 */
extern uint32_t cli_overflow_count;

#endif /* CLI_TASK_H */