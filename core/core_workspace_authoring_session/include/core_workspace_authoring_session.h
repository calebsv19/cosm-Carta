#ifndef CORE_WORKSPACE_AUTHORING_SESSION_H
#define CORE_WORKSPACE_AUTHORING_SESSION_H

#include <stdbool.h>
#include <stdint.h>

typedef enum CoreWorkspaceAuthoringSessionState {
    CORE_WORKSPACE_AUTHORING_SESSION_RUNTIME = 0,
    CORE_WORKSPACE_AUTHORING_SESSION_ENTERING = 1,
    CORE_WORKSPACE_AUTHORING_SESSION_AUTHORING = 2,
    CORE_WORKSPACE_AUTHORING_SESSION_APPLYING = 3,
    CORE_WORKSPACE_AUTHORING_SESSION_CANCELING = 4,
    CORE_WORKSPACE_AUTHORING_SESSION_RESUMING = 5,
    CORE_WORKSPACE_AUTHORING_SESSION_FAILED_SAFE = 6
} CoreWorkspaceAuthoringSessionState;

typedef enum CoreWorkspaceAuthoringSessionOutcome {
    CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_NONE = 0,
    CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_APPLIED = 1,
    CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_REJECTED = 2,
    CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_FAILED_SAFE = 3,
    CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_NOOP = 4
} CoreWorkspaceAuthoringSessionOutcome;

typedef enum CoreWorkspaceAuthoringSessionHookResult {
    CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK = 0,
    CORE_WORKSPACE_AUTHORING_SESSION_HOOK_REJECTED = 1,
    CORE_WORKSPACE_AUTHORING_SESSION_HOOK_FAILED = 2
} CoreWorkspaceAuthoringSessionHookResult;

enum {
    CORE_WORKSPACE_AUTHORING_CAP_LAYOUT_DRAFT = 1u << 0,
    CORE_WORKSPACE_AUTHORING_CAP_MODULE_CATALOG = 1u << 1,
    CORE_WORKSPACE_AUTHORING_CAP_FONT_THEME_DRAFT = 1u << 2,
    CORE_WORKSPACE_AUTHORING_CAP_PROFILE_EXPORT = 1u << 3,
    CORE_WORKSPACE_AUTHORING_CAP_PROFILE_IMPORT = 1u << 4,
    CORE_WORKSPACE_AUTHORING_CAP_SAFE_RUNTIME_GATE = 1u << 5,
    CORE_WORKSPACE_AUTHORING_CAP_PROFILE_COMPATIBILITY = 1u << 6,
    CORE_WORKSPACE_AUTHORING_CAP_MODULE_STATE_MIGRATION = 1u << 7,
    CORE_WORKSPACE_AUTHORING_CAP_RUNTIME_CONTROL_DRAFT = 1u << 8
};

typedef CoreWorkspaceAuthoringSessionHookResult (*CoreWorkspaceAuthoringSessionHook)(void *context);

typedef struct CoreWorkspaceAuthoringSessionHooks {
    CoreWorkspaceAuthoringSessionHook begin_authoring;
    CoreWorkspaceAuthoringSessionHook validate_draft;
    CoreWorkspaceAuthoringSessionHook apply_draft;
    CoreWorkspaceAuthoringSessionHook cancel_draft;
    CoreWorkspaceAuthoringSessionHook resume_runtime;
    CoreWorkspaceAuthoringSessionHook recover_failed_safe;
} CoreWorkspaceAuthoringSessionHooks;

typedef struct CoreWorkspaceAuthoringSession {
    CoreWorkspaceAuthoringSessionState state;
    CoreWorkspaceAuthoringSessionOutcome last_outcome;
    uint32_t capabilities;
    uint64_t transition_count;
    void *context;
    CoreWorkspaceAuthoringSessionHooks hooks;
} CoreWorkspaceAuthoringSession;

void core_workspace_authoring_session_init(CoreWorkspaceAuthoringSession *session,
                                           uint32_t capabilities,
                                           void *context,
                                           const CoreWorkspaceAuthoringSessionHooks *hooks);
bool core_workspace_authoring_session_runtime_mutation_allowed(const CoreWorkspaceAuthoringSession *session);
bool core_workspace_authoring_session_authoring_active(const CoreWorkspaceAuthoringSession *session);
CoreWorkspaceAuthoringSessionOutcome core_workspace_authoring_session_enter(CoreWorkspaceAuthoringSession *session);
CoreWorkspaceAuthoringSessionOutcome core_workspace_authoring_session_apply(CoreWorkspaceAuthoringSession *session);
CoreWorkspaceAuthoringSessionOutcome core_workspace_authoring_session_cancel(CoreWorkspaceAuthoringSession *session);
CoreWorkspaceAuthoringSessionOutcome core_workspace_authoring_session_recover(CoreWorkspaceAuthoringSession *session);

#endif
