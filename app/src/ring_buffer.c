#include "ring_buffer.h"

/* 單核 Cortex-M 上 ISR 與 Task 同核，只需擋編譯器重排 */
#define COMPILER_BARRIER() __asm volatile("" ::: "memory")

/* 初始化 Ring Buffer 的讀寫指標與丟棄計數器 */
void RingBuffer_Init(RingBuffer_t *rb) {
    rb->head = 0;
    rb->tail = 0;
    rb->dropped = 0;
}

/* 計算目前 Buffer 內已儲存的有效資料數量（利用無號數相減自然環繞） */
uint32_t RingBuffer_Count(RingBuffer_t *rb) {
    return (rb->head - rb->tail) & RING_BUFFER_MASK;
}

/* 將單一字元寫入 Buffer，若空間已滿則增加丟棄計數並回傳失敗 */
bool RingBuffer_Put(RingBuffer_t *rb, rb_item_t data) {
    if (RingBuffer_Count(rb) == RING_BUFFER_MASK) {
        rb->dropped++;
        return false;
    }
    
    rb->buffer[rb->head & RING_BUFFER_MASK] = data;
    COMPILER_BARRIER();
    rb->head++; 
    return true;
}

/* 從 Buffer 中讀取單一字元，若為空則回傳失敗 */
bool RingBuffer_Get(RingBuffer_t *rb, rb_item_t *data) {
    if (RingBuffer_Count(rb) == 0) {
        return false;
    }
    
    *data = rb->buffer[rb->tail & RING_BUFFER_MASK];
    COMPILER_BARRIER();
    rb->tail++;
    return true;
}