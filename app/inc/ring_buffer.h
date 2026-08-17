#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
/* 定義 Buffer 大小，必須是 2 的次方！ */
#define RING_BUFFER_SIZE 256
#define RING_BUFFER_MASK (RING_BUFFER_SIZE - 1)

/* _Static_assert 確保 SIZE 是 2 的次方 */
_Static_assert((RING_BUFFER_SIZE > 0) && ((RING_BUFFER_SIZE & RING_BUFFER_MASK) == 0), 
               "RING_BUFFER_SIZE must be a power of 2");

typedef uint8_t rb_item_t;

typedef struct {
    /* 
     * 面試考點 1：volatile！
     * 因為 head 在 ISR 中更新，tail 在 Task 中更新。
     * 沒加 volatile，編譯器 (開啟 -O2) 會把變數快取在暫存器，導致死迴圈。
     */
    rb_item_t buffer[RING_BUFFER_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t dropped; /* 滿了就 dropped++ */
} RingBuffer_t;

/* API 宣告 */
void RingBuffer_Init(RingBuffer_t *rb);
bool RingBuffer_Put(RingBuffer_t *rb, rb_item_t data);
bool RingBuffer_Get(RingBuffer_t *rb, rb_item_t *data);
uint32_t RingBuffer_Count(RingBuffer_t *rb);

#endif /* RING_BUFFER_H */