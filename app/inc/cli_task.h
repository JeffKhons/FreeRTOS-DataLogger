#ifndef CLI_TASK_H
#define CLI_TASK_H

#include "ring_buffer.h"

/* 定義 CLI 任務所需的 API */
void CLI_Init(void);
void CLI_Update(RingBuffer_t *rx_buf); // 每次呼叫時，檢查 Buffer 並解析指令

#endif /* CLI_TASK_H */