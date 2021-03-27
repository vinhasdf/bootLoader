#ifndef QUEUE_H
#define QUEUE_H

#include "stm32f10x.h"

#define MAX_QUEUE_ENTRY_SIZE        50
#define MAX_QUEUE_SIZE              5

typedef struct {
    uint8_t buff[MAX_QUEUE_ENTRY_SIZE];
    uint8_t pos;
} queueEntry;

queueEntry *getFreeQueueEntry(void);
void putQueueEntry(void);
queueEntry *getreadQueueEntry(void);
uint8_t getQeueuSize(void);
void getQueueEntry(void);

#endif  /* QUEUE_H */