#include "core_wake.h"

#include <assert.h>

typedef struct ExternalState {
    int signal_count;
    int wait_count;
} ExternalState;

static bool external_signal(void *ctx) {
    ExternalState *s = (ExternalState *)ctx;
    s->signal_count++;
    return true;
}

static CoreWakeWaitResult external_wait(void *ctx, uint32_t timeout_ms) {
    (void)timeout_ms;
    ExternalState *s = (ExternalState *)ctx;
    s->wait_count++;
    return CORE_WAKE_WAIT_SIGNALED;
}

int main(void) {
    CoreWake wake;
    assert(core_wake_init_cond(&wake));

    assert(core_wake_signal(&wake));
    CoreWakeWaitResult r = core_wake_wait(&wake, 10);
    assert(r == CORE_WAKE_WAIT_SIGNALED);

    r = core_wake_wait(&wake, 1);
    assert(r == CORE_WAKE_WAIT_TIMEOUT);

    core_wake_shutdown(&wake);

    ExternalState s = {0};
    CoreWake wake_ext;
    assert(core_wake_init_external(&wake_ext, external_signal, external_wait, &s));
    assert(core_wake_signal(&wake_ext));
    assert(core_wake_wait(&wake_ext, 5) == CORE_WAKE_WAIT_SIGNALED);
    assert(s.signal_count == 1);
    assert(s.wait_count == 1);
    core_wake_shutdown(&wake_ext);

    return 0;
}
