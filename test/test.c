#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "cli_task.h"

/* --- 實作 Link-time Hooks (Mocking Port) --- */
void CLI_Write(const char *str) {
    printf("%s", str); 
}

int32_t CLI_PortReadTempX100(void) {
    return -525; 
}

uint32_t CLI_PortLogCount(void) {
    return 100; 
}

bool CLI_PortLogRead(uint32_t idx, void *rec) {
    (void)idx; (void)rec;
    return true; 
}

/* ❌ 1. 刪除或註解掉對外部測試函式的宣告 */
// extern void Run_RingBuffer_Test(void);
// extern void Run_CLI_Test(void);

int main(void) {
    printf("=== 車內監控系統 CLI 互動測試平台 ===\n");
    
    /* ❌ 2. 刪除或註解掉呼叫這兩個函式的程式碼 */
    // Run_RingBuffer_Test();
    // Run_CLI_Test();
    
    /* 系統初始化 */
    RingBuffer_t uart_rx_buffer;
    RingBuffer_Init(&uart_rx_buffer);
    CLI_Init();

    printf("請直接輸入指令 (例如 'help', 'dump', 'read temp')，輸入 'exit' 離開。\n");

    char input_line[256];

    /* 進入 REPL (Read-Eval-Print Loop) 互動迴圈 */
    while (1) {
        printf("\nUART_Mock> ");
        if (fgets(input_line, sizeof(input_line), stdin) == NULL) {
            break;
        }

        // 去除 fgets 讀入的換行符號
        input_line[strcspn(input_line, "\n")] = 0;

        if (strcmp(input_line, "exit") == 0) {
            printf("結束測試平台。\n");
            break;
        }

        // 模擬 ISR 收到資料：將字串逐字元推進 Ring Buffer
        char *str = input_line;
        while (*str) {
            RingBuffer_Put(&uart_rx_buffer, (rb_item_t)(*str));
            str++;
        }
        // 補上 \n 觸發 CLI_Update 的行尾解析
        RingBuffer_Put(&uart_rx_buffer, '\n'); 

        // 模擬 FreeRTOS Task 運行
        CLI_Update(&uart_rx_buffer);
    }

    return 0;
}