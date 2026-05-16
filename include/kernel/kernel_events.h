#ifndef KERNEL_EVENTS_H
#define KERNEL_EVENTS_H

#include "message_queue.h"

void k_manual_trigger_event_init(message_queue_t *queue);
void k_send_manual_trigger_from_isr(void);

#endif // KERNEL_EVENTS_H
