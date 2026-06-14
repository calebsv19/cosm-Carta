#ifndef CORE_SIM_TRACE_H
#define CORE_SIM_TRACE_H

#include "core_base.h"
#include "core_sim.h"
#include "core_trace.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CORE_SIM_TRACE_LANE_FRAME "core_sim.frame"
#define CORE_SIM_TRACE_LANE_DT "core_sim.dt"
#define CORE_SIM_TRACE_LANE_TICKS "core_sim.ticks"
#define CORE_SIM_TRACE_LANE_PASSES "core_sim.passes"
#define CORE_SIM_TRACE_LANE_REASON "core_sim.reason"
#define CORE_SIM_TRACE_LANE_ACCUM "core_sim.accum"
#define CORE_SIM_TRACE_LANE_SIM_ADV "core_sim.sim_adv"
#define CORE_SIM_TRACE_LANE_EVENT "core_sim.event"

typedef struct CoreSimTraceFrameEmitOptions {
    bool emit_frame_marker;
    bool emit_reason_markers;
} CoreSimTraceFrameEmitOptions;

void core_sim_trace_frame_emit_options_defaults(CoreSimTraceFrameEmitOptions *options);

CoreResult core_sim_trace_emit_frame_record(CoreTraceSession *trace,
                                            const CoreSimFrameRecord *record,
                                            const CoreSimTraceFrameEmitOptions *options);

#ifdef __cplusplus
}
#endif

#endif
