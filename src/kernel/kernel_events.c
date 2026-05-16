#include "kernel_events.h"
#include "message_queue.h"
#include <stddef.h>

static message_queue_t *manual_trigger_queue = NULL;

void k_manual_trigger_event_init(message_queue_t *queue) {
    manual_trigger_queue = queue;
}

void k_send_manual_trigger_from_isr(void) {
    if (manual_trigger_queue == NULL) {
        return;
    }

    message_t msg = { .type = MSG_MANUAL_TRIGGER };
    mq_send_from_isr(manual_trigger_queue, &msg);
}
