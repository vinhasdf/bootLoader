#include "queue.h"

typedef struct {
    queueEntry entry[MAX_QUEUE_SIZE];
    uint8_t size;
    uint8_t rp;
    uint8_t wp;
} queueType;

queueType queue;

queueEntry *getFreeQueueEntry(void)
{
    queueEntry *result = 0;
    if (queue.size < MAX_QUEUE_SIZE)
    {
        result = &queue.entry[queue.wp];
    }
    
    return result;
}

void putQueueEntry(void)
{
    queue.size++;
    queue.wp++;
    queue.wp %= MAX_QUEUE_SIZE;     /* wrap to begin */
}

queueEntry *getreadQueueEntry(void)
{
    queueEntry *result = 0;
    if (queue.size > 0)
    {
        result = &queue.entry[queue.rp];
    }
    
    return result;
}

void getQueueEntry(void)
{
    queue.entry[queue.rp].pos = 0;  /* reset pos */
    queue.size--;
    queue.rp++;
    queue.rp %= MAX_QUEUE_SIZE;     /* wrap to begin */
}

uint8_t getQeueuSize(void)
{
    return queue.size;
}