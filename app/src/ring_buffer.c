#include "ring_buffer.h"

/* 條件編譯：隔離硬體與 PC 測試環境 */
#ifdef __arm__ 
    #include "stm32f4xx_hal.h"
#else
    #define __DMB() 
#endif

void RingBuffer_Init(RingBuffer_t *rb) {
    rb->head = 0;
    rb->tail = 0;
}

bool RingBuffer_IsEmpty(RingBuffer_t *rb) {
    return (rb->head == rb->tail);
}

bool RingBuffer_IsFull(RingBuffer_t *rb) {
    /* 使用 Bitwise AND 取代 Modulo，極致效能 */
    return (((rb->head + 1) & RING_BUFFER_MASK) == rb->tail);
}

/* 
 * Producer (ISR 專用)
 * 只能修改 head，只能讀取 tail
 */
bool RingBuffer_Push(RingBuffer_t *rb, rb_item_t data) {
    if (RingBuffer_IsFull(rb)) {
        return false; // Buffer 滿了，丟棄資料 (防 Buffer Overflow)
    }
    
    // 1. 先把資料寫進記憶體
    rb->buffer[rb->head] = data;
    
    /* 
     * 面試考點 2：Memory Barrier (記憶體屏障)
     * 在高階 ARM 核心 (或開啟 Cache 時)，CPU 可能會打亂指令執行順序 (Out-of-order execution)。
     * __DMB() 保證「資料寫入 buffer」這個動作，絕對發生在「更新 head」之前。
     * 如果順序反了，Consumer 可能會讀到還沒寫入的垃圾資料。
     */
    __DMB(); 
    
    // 2. 更新 head 指標 (N-1 回繞)
    rb->head = (rb->head + 1) & RING_BUFFER_MASK;
    
    return true;
}

/* 
 * Consumer (Task 專用)
 * 只能修改 tail，只能讀取 head
 */
bool RingBuffer_Pop(RingBuffer_t *rb, rb_item_t *data) {
    if (RingBuffer_IsEmpty(rb)) {
        return false; // Buffer 空的，沒資料可讀
    }
    
    // 1. 先把資料讀出來
    *data = rb->buffer[rb->tail];
    
    __DMB(); // 保證讀取資料發生在更新 tail 之前
    
    // 2. 更新 tail 指標
    rb->tail = (rb->tail + 1) & RING_BUFFER_MASK;
    
    return true;
}

/* 取得目前 Buffer 內有效資料的數量 */
uint32_t RingBuffer_GetCount(RingBuffer_t *rb) {
    /* 
     * 因為是環形，head 可能小於 tail。
     * 利用位元遮罩可以完美解決負數回繞的問題。
     */
    return (rb->head - rb->tail) & RING_BUFFER_MASK;
}