#include <stdio.h>
#include <string.h>
#include "ring_buffer.h"
#include "cli_task.h"

// 實體化一個全域的 Ring Buffer 模擬 UART 接收緩衝區
RingBuffer_t uart_rx_buffer;

/* 模擬硬體 UART 中斷 ISR：將字串逐一寫入 Buffer */
void Mock_UART_ISR_ReceiveString(const char *str) {
    while (*str) {
        RingBuffer_Push(&uart_rx_buffer, (rb_item_t)(*str));
        str++;
    }
}

int main() {
    // 系統初始化
    RingBuffer_Init(&uart_rx_buffer);
    CLI_Init();

    printf("===========================================\n");
    printf("  FreeRTOS 車內監控系統 - CLI 離線測試平台 \n");
    printf("===========================================\n");
    printf("請直接輸入指令 (例如 'help', 'dump', 'read temp')，輸入 'exit' 離開。\n");

    char input_line[256];

    // 無窮迴圈，模擬系統運行
    while (1) {
        printf("\nUART_Mock> ");
        if (fgets(input_line, sizeof(input_line), stdin) == NULL) {
            break;
        }

        // 去除 fgets 讀入的換行符號 (可選，但為了精準模擬我們自己補上 \n)
        input_line[strcspn(input_line, "\n")] = 0;

        if (strcmp(input_line, "exit") == 0) {
            printf("結束測試平台。\n");
            break;
        }

        // 1. 模擬 ISR 收到資料：將終端機輸入的字串加上換行符號推入 Ring Buffer
        Mock_UART_ISR_ReceiveString(input_line);
        Mock_UART_ISR_ReceiveString("\n"); // 補上 \n 觸發 CLI 解析

        // 2. 模擬 FreeRTOS Task 運行：呼叫 CLI_Update 處理 Buffer 內的資料
        CLI_Update(&uart_rx_buffer);
    }

    return 0;
}