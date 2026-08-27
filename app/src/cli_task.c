#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "cli_task.h"

#define MAX_LINE_LEN 63
#define MAX_ARGS 5

static char line_buf[MAX_LINE_LEN + 1];
static uint32_t line_len = 0;
static bool line_overflow = false;

uint32_t cli_overflow_count = 0;

/* 封裝 vsnprintf 搭配 static buffer，將格式化字串透過硬體層 (Port) 輸出 */
void CLI_Printf(const char *format, ...) {
    static char print_buf[128];
    va_list args;
    va_start(args, format);
    vsnprintf(print_buf, sizeof(print_buf), format, args);
    va_end(args);
    CLI_Write(print_buf);
}

/* 將字串安全轉換為無號 32 位元整數，並嚴格攔截非法字元與乘法溢位 */
static bool ParseU32(const char *str, uint32_t *out_val) {
    if (!str || *str == '\0') return false;
    
    uint32_t val = 0;
    while (*str) {
        if (*str < '0' || *str > '9') return false;
        uint32_t digit = (uint32_t)(*str - '0');
        
        /* 偵測 4294967296 溢位: val * 10 + digit > UINT32_MAX */
        if (val > (4294967295U - digit) / 10) return false;
        
        val = val * 10 + digit;
        str++;
    }
    *out_val = val;
    return true;
}

/* 根據解析完畢的 argc 與 argv，比對並執行對應的系統指令邏輯 */
static void CLI_Execute(int argc, char *argv[], RingBuffer_t *rx_buf) {
    if (argc == 0) return;

    if (strcmp(argv[0], "help") == 0) {
        // 更新 help 說明，將 read temp 改為 read
        CLI_Printf("Commands: help, read, dump [n], stat\r\n");
    } 
    else if (strcmp(argv[0], "read") == 0) {
        // 只要輸入 read，就直接觸發讀取溫度並寫入 Flash 的動作！
        Action_Read_And_Save();
    } 
    else if (strcmp(argv[0], "dump") == 0) {
        uint32_t total = CLI_PortLogCount();
        
        /* dump : total == 0 要先印 "no record" 就 return，避免無號數環繞 */
        if (total == 0) {
            CLI_Printf("no record\r\n");
            return;
        }

        uint32_t n = 20; // 預設 20 筆
        if (argc >= 2) {
            /* dump 0 要拒絕: ParseU32 失敗或 n==0 都報錯 */
            if (!ParseU32(argv[1], &n) || n == 0) {
                CLI_Printf("Invalid number\r\n");
                return;
            }
        }
        if (n > total) n = total;
        
        uint32_t first = total - n;
        CLI_Printf("Dumping logs %u to %u...\r\n", first, total - 1);
        
        /* 實際呼叫 CLI_PortLogRead 逐筆撈取 */
        for (uint32_t i = first; i < total; i++) {
            CLI_LogRecord_t rec;
            if (CLI_PortLogRead(i, &rec)) {
                uint32_t abs_temp = (rec.temp_x100 < 0) ? (0u - (uint32_t)rec.temp_x100) : (uint32_t)rec.temp_x100;
                // 簡化 dump 輸出格式，因為目前還沒實作 RTC，所以 timestamp 和 seq 先忽略，專注看溫度
                CLI_Printf("[%u] Temp: %s%u.%02u C\r\n",
                           i, (rec.temp_x100 < 0) ? "-" : "", abs_temp / 100, abs_temp % 100);
            } else {
                CLI_Printf("[%u] Read error\r\n", i);
            }
        }
    } 
    else if (strcmp(argv[0], "stat") == 0) {
        /* stat 印 rx.count / rx.dropped / cli.overflow / log.records */
        CLI_Printf("rx.count: %u\r\n", RingBuffer_Count(rx_buf));
        CLI_Printf("rx.dropped: %u\r\n", RingBuffer_Dropped(rx_buf));
        CLI_Printf("cli.overflow: %u\r\n", cli_overflow_count);
        CLI_Printf("log.records: %u\r\n", CLI_PortLogCount());
    } 
    else {
        CLI_Printf("Unknown command\r\n");
    }
}

/* 自定義 Tokenizer：以空白與 Tab 為分隔符切分指令行，並轉交給執行器 */
static void CLI_ParseAndExecute(char *line, RingBuffer_t *rx_buf) {
    char *argv[MAX_ARGS];
    int argc = 0;
    char *p = line;

    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        
        if (argc < MAX_ARGS) argv[argc++] = p;
        
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) {
            *p = '\0';
            p++;
        }
    }
    CLI_Execute(argc, argv, rx_buf);
}

/* 初始化 CLI 接收狀態機，清空字串緩衝區與溢位計數 */
void CLI_Init(void) {
    line_len = 0;
    line_overflow = false;
    cli_overflow_count = 0;
}

/* 週期性從 Ring Buffer 提取字元以組合完整指令，並負責攔截單行字元數溢位 */
void CLI_Update(RingBuffer_t *rx_buf) {
    rb_item_t c;
    while (RingBuffer_Get(rx_buf, &c)) {
        /* 遇到 \r 或 \n 視為行尾，且只執行一次（過濾連續 \r\n\r\n） */
        if (c == '\r' || c == '\n') {
            if (line_overflow) {
                line_overflow = false; // 拋棄完畢，重置狀態
                line_len = 0;
            } else if (line_len > 0) {
                line_buf[line_len] = '\0';
                CLI_ParseAndExecute(line_buf, rx_buf);
                line_len = 0;
            }
        } else {
            /* 處理正常字元 */
            if (!line_overflow) {
                if (line_len < MAX_LINE_LEN) {
                    line_buf[line_len++] = (char)c;
                } else {
                    /* 一行超過 63 字元: 整行丟棄並計數 */
                    line_overflow = true;
                    cli_overflow_count++;
                    line_len = 0;
                }
            }
        }
    }
}