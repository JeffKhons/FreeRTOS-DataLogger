#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

/* 定義 Buffer 大小，必須是 2 的次方！ */
#define RING_BUFFER_SIZE 256 
#define RING_BUFFER_MASK (RING_BUFFER_SIZE - 1) // 0xFF

/* 利用 Bitwise AND 檢查 RING_BUFFER_SIZE 是否為 2 的次方。 */
#if (RING_BUFFER_SIZE == 0) || ((RING_BUFFER_SIZE & (RING_BUFFER_SIZE - 1)) != 0)
    #error 
#endif

/* 
 * 假設目前用來存 UART 收到的字元，或是你要打包的 Sensor Data。
 * 若未來要存結構體，直接改這個 typedef 即可。
 */
typedef uint8_t rb_item_t; 

typedef struct {
    rb_item_t buffer[RING_BUFFER_SIZE];
    
    /* 
     * 面試考點 1：volatile！
     * 因為 head 在 ISR 中更新，tail 在 Task 中更新。
     * 沒加 volatile，編譯器 (開啟 -O2) 會把變數快取在暫存器，導致死迴圈。
     */
    volatile uint32_t head; 
    volatile uint32_t tail; 
} RingBuffer_t;

/* API 宣告 */
void RingBuffer_Init(RingBuffer_t *rb);
bool RingBuffer_Push(RingBuffer_t *rb, rb_item_t data); // ISR 呼叫 (Producer)
bool RingBuffer_Pop(RingBuffer_t *rb, rb_item_t *data); // Task 呼叫 (Consumer)
bool RingBuffer_IsEmpty(RingBuffer_t *rb);
bool RingBuffer_IsFull(RingBuffer_t *rb);
uint32_t RingBuffer_GetCount(RingBuffer_t *rb);

#endif /* RING_BUFFER_H */