#include "core_workspace_authoring_session.h"

#include <string.h>

static CoreWorkspaceAuthoringSessionHookResult core_workspace_authoring_session_call(
    CoreWorkspaceAuthoringSession *session,
    CoreWorkspaceAuthoringSessionHook hook) {
    if (!session || !hook) {
        return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_FAILED;
    }
    return hook(session->context);
}

static CoreWorkspaceAuthoringSessionOutcome core_workspace_authoring_session_reject(
    CoreWorkspaceAuthoringSession *session,
    CoreWorkspaceAuthoringSessionState state) {
    session->state = state;
    session->last_outcome = CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_REJECTED;
    return session->last_outcome;
}

static CoreWorkspaceAuthoringSessionOutcome core_workspace_authoring_session_fail_safe(
    CoreWorkspaceAuthoringSession *session) {
    session->state = CORE_WORKSPACE_AUTHORING_SESSION_FAILED_SAFE;
    session->last_outcome = CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_FAILED_SAFE;
    session->transition_count += 1u;
    return session->last_outcome;
}

void core_workspace_authoring_session_init(CoreWorkspaceAuthoringSession *session,
                                           uint32_t capabilities,
                                           void *context,
                                           const CoreWorkspaceAuthoringSessionHooks *hooks) {
    if (!session) {
        return;
    }
    memset(session, 0, sizeof(*session));
    session->state = CORE_WORKSPACE_AUTHORING_SESSION_RUNTIME;
    session->capabilities = capabilities;
    session->context = context;
    if (hooks) {
        session->hooks = *hooks;
    }
}

bool core_workspace_authoring_session_runtime_mutation_allowed(const CoreWorkspaceAuthoringSession *session) {
    return session && session->state == CORE_WORKSPACE_AUTHORING_SESSION_RUNTIME;
}

bool core_workspace_authoring_session_authoring_active(const CoreWorkspaceAuthoringSession *session) {
    return session && (session->state == CORE_WORKSPACE_AUTHORING_SESSION_ENTERING ||
                       session->state == CORE_WORKSPACE_AUTHORING_SESSION_AUTHORING ||
                       session->state == CORE_WORKSPACE_AUTHORING_SESSION_APPLYING ||
                       session->state == CORE_WORKSPACE_AUTHORING_SESSION_CANCELING ||
                       session->state == CORE_WORKSPACE_AUTHORING_SESSION_RESUMING ||
                       session->state == CORE_WORKSPACE_AUTHORING_SESSION_FAILED_SAFE);
}

CoreWorkspaceAuthoringSessionOutcome core_workspace_authoring_session_enter(CoreWorkspaceAuthoringSession *session) {
    CoreWorkspaceAuthoringSessionHookResult result;
    if (!session || session->state != CORE_WORKSPACE_AUTHORING_SESSION_RUNTIME ||
        !(session->capabilities & CORE_WORKSPACE_AUTHORING_CAP_SAFE_RUNTIME_GATE)) {
        return session ? core_workspace_authoring_session_reject(session, session->state)
                       : CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_REJECTED;
    }
    session->state = CORE_WORKSPACE_AUTHORING_SESSION_ENTERING;
    result = core_workspace_authoring_session_call(session, session->hooks.begin_authoring);
    if (result == CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK) {
        session->state = CORE_WORKSPACE_AUTHORING_SESSION_AUTHORING;
        session->last_outcome = CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_NONE;
        session->transition_count += 1u;
        return session->last_outcome;
    }
    if (result == CORE_WORKSPACE_AUTHORING_SESSION_HOOK_REJECTED) {
        return core_workspace_authoring_session_reject(session, CORE_WORKSPACE_AUTHORING_SESSION_RUNTIME);
    }
    return core_workspace_authoring_session_fail_safe(session);
}

CoreWorkspaceAuthoringSessionOutcome core_workspace_authoring_session_apply(CoreWorkspaceAuthoringSession *session) {
    CoreWorkspaceAuthoringSessionHookResult result;
    if (!session || session->state != CORE_WORKSPACE_AUTHORING_SESSION_AUTHORING) {
        return session ? core_workspace_authoring_session_reject(session, session->state)
                       : CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_REJECTED;
    }
    session->state = CORE_WORKSPACE_AUTHORING_SESSION_APPLYING;
    result = core_workspace_authoring_session_call(session, session->hooks.validate_draft);
    if (result == CORE_WORKSPACE_AUTHORING_SESSION_HOOK_REJECTED) {
        return core_workspace_authoring_session_reject(session, CORE_WORKSPACE_AUTHORING_SESSION_AUTHORING);
    }
    if (result != CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK) {
        return core_workspace_authoring_session_fail_safe(session);
    }
    result = core_workspace_authoring_session_call(session, session->hooks.apply_draft);
    if (result == CORE_WORKSPACE_AUTHORING_SESSION_HOOK_REJECTED) {
        return core_workspace_authoring_session_reject(session, CORE_WORKSPACE_AUTHORING_SESSION_AUTHORING);
    }
    if (result != CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK) {
        return core_workspace_authoring_session_fail_safe(session);
    }
    session->state = CORE_WORKSPACE_AUTHORING_SESSION_RESUMING;
    result = core_workspace_authoring_session_call(session, session->hooks.resume_runtime);
    if (result != CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK) {
        return core_workspace_authoring_session_fail_safe(session);
    }
    session->state = CORE_WORKSPACE_AUTHORING_SESSION_RUNTIME;
    session->last_outcome = CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_APPLIED;
    session->transition_count += 1u;
    return session->last_outcome;
}

CoreWorkspaceAuthoringSessionOutcome core_workspace_authoring_session_cancel(CoreWorkspaceAuthoringSession *session) {
    CoreWorkspaceAuthoringSessionHookResult result;
    if (!session || session->state != CORE_WORKSPACE_AUTHORING_SESSION_AUTHORING) {
        return session ? core_workspace_authoring_session_reject(session, session->state)
                       : CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_REJECTED;
    }
    session->state = CORE_WORKSPACE_AUTHORING_SESSION_CANCELING;
    result = core_workspace_authoring_session_call(session, session->hooks.cancel_draft);
    if (result != CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK) {
        return core_workspace_authoring_session_fail_safe(session);
    }
    session->state = CORE_WORKSPACE_AUTHORING_SESSION_RESUMING;
    result = core_workspace_authoring_session_call(session, session->hooks.resume_runtime);
    if (result != CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK) {
        return core_workspace_authoring_session_fail_safe(session);
    }
    session->state = CORE_WORKSPACE_AUTHORING_SESSION_RUNTIME;
    session->last_outcome = CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_NOOP;
    session->transition_count += 1u;
    return session->last_outcome;
}

CoreWorkspaceAuthoringSessionOutcome core_workspace_authoring_session_recover(CoreWorkspaceAuthoringSession *session) {
    CoreWorkspaceAuthoringSessionHookResult result;
    if (!session || session->state != CORE_WORKSPACE_AUTHORING_SESSION_FAILED_SAFE) {
        return session ? core_workspace_authoring_session_reject(session, session->state)
                       : CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_REJECTED;
    }
    result = core_workspace_authoring_session_call(session, session->hooks.recover_failed_safe);
    if (result != CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK) {
        return core_workspace_authoring_session_fail_safe(session);
    }
    session->state = CORE_WORKSPACE_AUTHORING_SESSION_RUNTIME;
    session->last_outcome = CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_NOOP;
    session->transition_count += 1u;
    return session->last_outcome;
}
