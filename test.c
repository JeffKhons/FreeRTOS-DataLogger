#include <stdio.h>
#include <string.h>
#include "ring_buffer.h"
#include "cli_task.h"

int main() {
    RingBuffer_t uart_rx_buffer;
    RingBuffer_Init(&uart_rx_buffer);
    CLI_Init();

    printf("=== UART CLI 模擬測試 (輸入 'exit' 離開) ===\n");

    char keyboard_input[100];
    
    // 模擬 Super Loop (主迴圈)
    while (1) {
        printf("UART>> ");
        
        // 這裡會卡住等待鍵盤輸入，但在真實 MCU 中不會卡，MCU 會一直跑 while(1)
        if (fgets(keyboard_input, sizeof(keyboard_input), stdin) == NULL) break;
        
        if (strncmp(keyboard_input, "exit", 4) == 0) break;

        /* 
         * 🔥 模擬 UART ISR 行為：
         * 假設硬體中斷把字元一個一個敲進來，我們將字串轉成單個字元 Push 進 Ring Buffer。
         * 注意：我們連同 fgets 抓到的 '\n' 也一起存進去，讓 CLI_Update 來判斷斷句。
         */
        for (int i = 0; keyboard_input[i] != '\0'; i++) {
            RingBuffer_Push(&uart_rx_buffer, (rb_item_t)keyboard_input[i]);
        }

        /* 
         * 🔥 模擬主程式 (或是未來的 CLI_Task)：
         * 呼叫 Update，它會去把 Buffer 裡面的字元全撈出來，拼成字串後執行。
         */
        CLI_Update(&uart_rx_buffer);
    }
    
    return 0;
}