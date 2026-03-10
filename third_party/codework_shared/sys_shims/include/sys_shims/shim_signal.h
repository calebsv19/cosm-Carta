#ifndef SYS_SHIMS_SHIM_SIGNAL_H
#define SYS_SHIMS_SHIM_SIGNAL_H

#if defined(__clang__) || defined(__GNUC__)
#include_next <signal.h>
#else
#include <signal.h>
#endif

typedef sig_atomic_t shim_sig_atomic_t;

#define SHIM_SIG_DFL SIG_DFL
#define SHIM_SIG_IGN SIG_IGN
#define SHIM_SIG_ERR SIG_ERR

#endif
