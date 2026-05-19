#include <stdio.h>

#include "user_app.h"
#include "message_queue.h"
#include "syscalls.h"

void trigger_task(void) {
    while (1) {
        sys_heartbeat();
    }
}
