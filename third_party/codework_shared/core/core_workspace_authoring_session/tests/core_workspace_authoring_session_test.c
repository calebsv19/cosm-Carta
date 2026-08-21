#include "core_workspace_authoring_session.h"

#include <assert.h>

typedef struct TestHost {
    CoreWorkspaceAuthoringSessionHookResult begin;
    CoreWorkspaceAuthoringSessionHookResult validate;
    CoreWorkspaceAuthoringSessionHookResult apply;
    CoreWorkspaceAuthoringSessionHookResult cancel;
    CoreWorkspaceAuthoringSessionHookResult resume;
    CoreWorkspaceAuthoringSessionHookResult recover;
} TestHost;

static CoreWorkspaceAuthoringSessionHookResult test_begin(void *context) { return ((TestHost *)context)->begin; }
static CoreWorkspaceAuthoringSessionHookResult test_validate(void *context) { return ((TestHost *)context)->validate; }
static CoreWorkspaceAuthoringSessionHookResult test_apply(void *context) { return ((TestHost *)context)->apply; }
static CoreWorkspaceAuthoringSessionHookResult test_cancel(void *context) { return ((TestHost *)context)->cancel; }
static CoreWorkspaceAuthoringSessionHookResult test_resume(void *context) { return ((TestHost *)context)->resume; }
static CoreWorkspaceAuthoringSessionHookResult test_recover(void *context) { return ((TestHost *)context)->recover; }

static CoreWorkspaceAuthoringSessionHooks test_hooks(void) {
    return (CoreWorkspaceAuthoringSessionHooks){ test_begin, test_validate, test_apply, test_cancel, test_resume, test_recover };
}

static TestHost test_host_ok(void) {
    return (TestHost){ CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK, CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK,
                       CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK, CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK,
                       CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK, CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK };
}

static void test_apply_and_cancel_gate_runtime_mutation(void) {
    TestHost host = test_host_ok();
    CoreWorkspaceAuthoringSession session;
    CoreWorkspaceAuthoringSessionHooks hooks = test_hooks();
    core_workspace_authoring_session_init(&session, CORE_WORKSPACE_AUTHORING_CAP_SAFE_RUNTIME_GATE, &host, &hooks);
    assert(core_workspace_authoring_session_runtime_mutation_allowed(&session));
    assert(core_workspace_authoring_session_enter(&session) == CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_NONE);
    assert(session.state == CORE_WORKSPACE_AUTHORING_SESSION_AUTHORING);
    assert(!core_workspace_authoring_session_runtime_mutation_allowed(&session));
    assert(core_workspace_authoring_session_cancel(&session) == CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_NOOP);
    assert(core_workspace_authoring_session_runtime_mutation_allowed(&session));
    assert(core_workspace_authoring_session_enter(&session) == CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_NONE);
    assert(core_workspace_authoring_session_apply(&session) == CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_APPLIED);
    assert(core_workspace_authoring_session_runtime_mutation_allowed(&session));
}

static void test_rejection_retains_authoring_for_draft_correction(void) {
    TestHost host = test_host_ok();
    CoreWorkspaceAuthoringSession session;
    CoreWorkspaceAuthoringSessionHooks hooks = test_hooks();
    host.validate = CORE_WORKSPACE_AUTHORING_SESSION_HOOK_REJECTED;
    core_workspace_authoring_session_init(&session, CORE_WORKSPACE_AUTHORING_CAP_SAFE_RUNTIME_GATE, &host, &hooks);
    assert(core_workspace_authoring_session_enter(&session) == CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_NONE);
    assert(core_workspace_authoring_session_apply(&session) == CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_REJECTED);
    assert(session.state == CORE_WORKSPACE_AUTHORING_SESSION_AUTHORING);
    assert(!core_workspace_authoring_session_runtime_mutation_allowed(&session));
}

static void test_failure_requires_explicit_recovery(void) {
    TestHost host = test_host_ok();
    CoreWorkspaceAuthoringSession session;
    CoreWorkspaceAuthoringSessionHooks hooks = test_hooks();
    host.apply = CORE_WORKSPACE_AUTHORING_SESSION_HOOK_FAILED;
    core_workspace_authoring_session_init(&session, CORE_WORKSPACE_AUTHORING_CAP_SAFE_RUNTIME_GATE, &host, &hooks);
    assert(core_workspace_authoring_session_enter(&session) == CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_NONE);
    assert(core_workspace_authoring_session_apply(&session) == CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_FAILED_SAFE);
    assert(session.state == CORE_WORKSPACE_AUTHORING_SESSION_FAILED_SAFE);
    assert(!core_workspace_authoring_session_runtime_mutation_allowed(&session));
    assert(core_workspace_authoring_session_recover(&session) == CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_NOOP);
    assert(core_workspace_authoring_session_runtime_mutation_allowed(&session));
}

static void test_missing_gate_and_invalid_transition_are_rejected(void) {
    TestHost host = test_host_ok();
    CoreWorkspaceAuthoringSession session;
    CoreWorkspaceAuthoringSessionHooks hooks = test_hooks();
    core_workspace_authoring_session_init(&session, 0u, &host, &hooks);
    assert(core_workspace_authoring_session_enter(&session) == CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_REJECTED);
    assert(core_workspace_authoring_session_apply(&session) == CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_REJECTED);
    assert(session.state == CORE_WORKSPACE_AUTHORING_SESSION_RUNTIME);
}

int main(void) {
    test_apply_and_cancel_gate_runtime_mutation();
    test_rejection_retains_authoring_for_draft_correction();
    test_failure_requires_explicit_recovery();
    test_missing_gate_and_invalid_transition_are_rejected();
    return 0;
}
