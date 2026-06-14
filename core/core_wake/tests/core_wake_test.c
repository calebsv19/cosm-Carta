#include "core_wake.h"

#include <assert.h>

typedef struct ExternalState {
    int signal_count;
    int wait_count;
    bool signal_ok;
    CoreWakeWaitResult wait_result;
} ExternalState;

static bool external_signal(void *ctx) {
    ExternalState *s = (ExternalState *)ctx;
    s->signal_count++;
    return s->signal_ok;
}

static CoreWakeWaitResult external_wait(void *ctx, uint32_t timeout_ms) {
    (void)timeout_ms;
    ExternalState *s = (ExternalState *)ctx;
    s->wait_count++;
    return s->wait_result;
}

int main(void) {
    assert(!core_wake_init_cond(NULL));

    CoreWake invalid_ext;
    assert(!core_wake_init_external(NULL, external_signal, external_wait, NULL));
    assert(!core_wake_init_external(&invalid_ext, NULL, external_wait, NULL));
    assert(!core_wake_init_external(&invalid_ext, external_signal, NULL, NULL));

    assert(!core_wake_signal(NULL));
    assert(core_wake_wait(NULL, 1) == CORE_WAKE_WAIT_ERROR);

    CoreWake wake;
    assert(core_wake_init_cond(&wake));

    assert(core_wake_signal(&wake));
    CoreWakeWaitResult r = core_wake_wait(&wake, 10);
    assert(r == CORE_WAKE_WAIT_SIGNALED);

    assert(core_wake_signal(&wake));
    assert(core_wake_signal(&wake));
    assert(core_wake_wait(&wake, 0) == CORE_WAKE_WAIT_SIGNALED);
    assert(core_wake_wait(&wake, 0) == CORE_WAKE_WAIT_SIGNALED);

    r = core_wake_wait(&wake, 1);
    assert(r == CORE_WAKE_WAIT_TIMEOUT);

    wake.pending_signals = UINT64_MAX;
    assert(!core_wake_signal(&wake));
    assert(wake.pending_signals == UINT64_MAX);

    core_wake_shutdown(&wake);
    assert(!core_wake_signal(&wake));
    assert(core_wake_wait(&wake, 1) == CORE_WAKE_WAIT_ERROR);

    ExternalState s = {0, 0, true, CORE_WAKE_WAIT_SIGNALED};
    CoreWake wake_ext;
    assert(core_wake_init_external(&wake_ext, external_signal, external_wait, &s));
    assert(core_wake_signal(&wake_ext));
    assert(core_wake_wait(&wake_ext, 5) == CORE_WAKE_WAIT_SIGNALED);
    assert(s.signal_count == 1);
    assert(s.wait_count == 1);

    s.signal_ok = false;
    s.wait_result = CORE_WAKE_WAIT_TIMEOUT;
    assert(!core_wake_signal(&wake_ext));
    assert(core_wake_wait(&wake_ext, 5) == CORE_WAKE_WAIT_TIMEOUT);
    assert(s.signal_count == 2);
    assert(s.wait_count == 2);

    core_wake_shutdown(&wake_ext);
    assert(!core_wake_signal(&wake_ext));
    assert(core_wake_wait(&wake_ext, 1) == CORE_WAKE_WAIT_ERROR);

    return 0;
}
