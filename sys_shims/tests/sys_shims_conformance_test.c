/*
 * sys_shims_conformance_test.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>

#include "sys_shims/shim_ctype.h"
#include "sys_shims/shim_limits.h"
#include "sys_shims/shim_locale.h"
#include "sys_shims/shim_errno.h"
#include "sys_shims/shim_math.h"
#include "sys_shims/shim_signal.h"
#include "sys_shims/shim_stddef.h"
#include "sys_shims/shim_stdint.h"
#include "sys_shims/shim_stdlib.h"
#include "sys_shims/shim_string.h"
#include "sys_shims/shim_time.h"

#define MAX_MISMATCHES 128
#define LANE_COUNT 7

typedef enum Severity {
    SEVERITY_MEDIUM = 0,
    SEVERITY_HIGH = 1
} Severity;

typedef enum Lane {
    LANE_TYPES = 0,
    LANE_STDLIB = 1,
    LANE_STRING = 2,
    LANE_ERRNO = 3,
    LANE_TIME = 4,
    LANE_MATH = 5,
    LANE_EXTENDED = 6
} Lane;

typedef struct LaneStats {
    const char *name;
    int checks_total;
    int checks_passed;
    int mismatches;
    int high_mismatches;
} LaneStats;

typedef struct MismatchEntry {
    const char *lane;
    char case_id[64];
    const char *severity;
    char detail[192];
} MismatchEntry;

typedef struct Report {
    LaneStats lanes[LANE_COUNT];
    int total_checks;
    int total_passed;
    int total_mismatches;
    int total_high_mismatches;
    MismatchEntry mismatches[MAX_MISMATCHES];
    int mismatch_count;
} Report;

typedef struct CaseSeen {
    const char *id;
    bool seen;
} CaseSeen;

static void record_check(Report *r,
                         Lane lane,
                         const char *case_id,
                         bool pass,
                         Severity severity,
                         const char *detail);

static void report_init(Report *r) {
    r->lanes[LANE_TYPES].name = "types";
    r->lanes[LANE_STDLIB].name = "stdlib";
    r->lanes[LANE_STRING].name = "string";
    r->lanes[LANE_ERRNO].name = "errno";
    r->lanes[LANE_TIME].name = "time";
    r->lanes[LANE_MATH].name = "math";
    r->lanes[LANE_EXTENDED].name = "extended";
}

static void run_types_lane(Report *r) {
    record_check(r,
                 LANE_TYPES,
                 "types_size_t_parity",
                 sizeof(shim_size_t) == sizeof(size_t),
                 SEVERITY_MEDIUM,
                 "size_t alias parity");

    record_check(r,
                 LANE_TYPES,
                 "types_uint64_parity",
                 sizeof(shim_uint64_t) == sizeof(uint64_t),
                 SEVERITY_MEDIUM,
                 "uint64 alias parity");

    record_check(r,
                 LANE_TYPES,
                 "types_macro_bounds",
                 (SHIM_UINT32_MAX == UINT32_MAX) && (SHIM_INT32_MIN == INT32_MIN),
                 SEVERITY_HIGH,
                 "stdint macro parity");
}

static void record_check(Report *r,
                         Lane lane,
                         const char *case_id,
                         bool pass,
                         Severity severity,
                         const char *detail) {
    LaneStats *ls = &r->lanes[lane];
    ls->checks_total += 1;
    r->total_checks += 1;

    if (pass) {
        ls->checks_passed += 1;
        r->total_passed += 1;
        return;
    }

    ls->mismatches += 1;
    r->total_mismatches += 1;
    if (severity == SEVERITY_HIGH) {
        ls->high_mismatches += 1;
        r->total_high_mismatches += 1;
    }

    if (r->mismatch_count < MAX_MISMATCHES) {
        MismatchEntry *m = &r->mismatches[r->mismatch_count++];
        m->lane = ls->name;
        snprintf(m->case_id, sizeof(m->case_id), "%s", case_id ? case_id : "unknown");
        m->severity = (severity == SEVERITY_HIGH) ? "high" : "medium";
        snprintf(m->detail, sizeof(m->detail), "%s", detail ? detail : "mismatch");
    }
}

static bool load_fixture_manifest(const char *path, CaseSeen *required, size_t required_count) {
    FILE *f = fopen(path, "rb");
    char line[512];

    if (!f) {
        return false;
    }

    while (fgets(line, sizeof(line), f) != NULL) {
        char *p = line;
        char *comma = NULL;
        size_t i;

        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;

        comma = strchr(p, ',');
        if (comma) *comma = '\0';

        for (i = 0; i < required_count; ++i) {
            if (strcmp(required[i].id, p) == 0) {
                required[i].seen = true;
                break;
            }
        }
    }

    fclose(f);

    for (size_t i = 0; i < required_count; ++i) {
        if (!required[i].seen) return false;
    }

    return true;
}

static void run_stdlib_lane(Report *r) {
    long s_value = 0;
    int s_err = -1;
    char *end = NULL;
    long c_value;

    bool shim_ok = shim_strtol_checked("456", 10, &s_value, &s_err);
    errno = 0;
    c_value = strtol("456", &end, 10);

    record_check(r,
                 LANE_STDLIB,
                 "stdlib_strtol_valid",
                 shim_ok && end && *end == '\0' && errno == 0 && s_value == c_value && s_err == 0,
                 SEVERITY_MEDIUM,
                 "valid strtol parity");

    shim_ok = shim_strtol_checked("456x", 10, &s_value, &s_err);
    record_check(r,
                 LANE_STDLIB,
                 "stdlib_strtol_invalid",
                 (!shim_ok && s_err == EINVAL),
                 SEVERITY_HIGH,
                 "invalid strtol rejection");
}

static void run_string_lane(Report *r) {
    const char *txt = "conformance";
    char s_dst[32];
    char c_dst[32];

    memset(s_dst, 0, sizeof(s_dst));
    memset(c_dst, 0, sizeof(c_dst));

    (void)memcpy(c_dst, txt, 12);
    record_check(r,
                 LANE_STRING,
                 "string_memcpy_checked",
                 shim_memcpy_checked(s_dst, sizeof(s_dst), txt, 12) && (memcmp(s_dst, c_dst, 12) == 0),
                 SEVERITY_MEDIUM,
                 "memcpy checked parity");

    record_check(r,
                 LANE_STRING,
                 "string_memcpy_overflow",
                 !shim_memcpy_checked(s_dst, 2, txt, 12),
                 SEVERITY_HIGH,
                 "overflow rejection");
}

static void run_errno_lane(Report *r) {
    shim_errno_set(EINVAL);
    record_check(r,
                 LANE_ERRNO,
                 "errno_set_get",
                 shim_errno_get() == errno,
                 SEVERITY_MEDIUM,
                 "errno parity");
    shim_errno_clear();
}

static void run_time_lane(Report *r) {
    time_t shim_now = 0;
    time_t libc_now = time(NULL);
    double diff = 0.0;

    if (!shim_time_now(&shim_now)) {
        record_check(r,
                     LANE_TIME,
                     "time_now_range",
                     false,
                     SEVERITY_MEDIUM,
                     "shim_time_now failure");
        return;
    }

    diff = difftime(shim_now, libc_now);
    record_check(r,
                 LANE_TIME,
                 "time_now_range",
                 (diff >= -1.0 && diff <= 1.0),
                 SEVERITY_MEDIUM,
                 "time range parity");
}

static void run_math_lane(Report *r) {
    double s_value = 0.0;
    double c_value = 0.0;
    shim_errno_t s_err = -1;

    if (shim_pow_checked(3.0, 4.0, &s_value, &s_err)) {
        c_value = pow(3.0, 4.0);
    }
    record_check(r,
                 LANE_MATH,
                 "math_pow_basic",
                 shim_nearly_equal_abs_f64(s_value, c_value, 1e-12) && s_err == 0,
                 SEVERITY_MEDIUM,
                 "pow parity");

    record_check(r,
                 LANE_MATH,
                 "math_sqrt_negative",
                 !shim_sqrt_checked(-4.0, &s_value, &s_err) && s_err == EDOM,
                 SEVERITY_HIGH,
                 "negative sqrt domain handling");
}

static void run_extended_lane(Report *r) {
    int shim_alpha = shim_isalpha_i('A') ? 1 : 0;
    int libc_alpha = isalpha('A') ? 1 : 0;
    int shim_digit = shim_isdigit_i('4') ? 1 : 0;
    int libc_digit = isdigit('4') ? 1 : 0;
    shim_lconv_t *lc = shim_localeconv();
    shim_sig_atomic_t sig_value = 2;

    record_check(r,
                 LANE_EXTENDED,
                 "extended_ctype_parity",
                 (shim_alpha == libc_alpha) && (shim_digit == libc_digit) &&
                     (shim_tolower_i('Q') == tolower('Q')),
                 SEVERITY_MEDIUM,
                 "ctype parity");

    record_check(r,
                 LANE_EXTENDED,
                 "extended_limits_parity",
                 (SHIM_CHAR_BIT == CHAR_BIT) && (SHIM_INT_MAX == INT_MAX) && (SHIM_LONG_MAX == LONG_MAX),
                 SEVERITY_HIGH,
                 "limits macro parity");

    record_check(r,
                 LANE_EXTENDED,
                 "extended_locale_signal_surface",
                 (lc != NULL) && (lc->decimal_point != NULL) &&
                     (SHIM_SIG_DFL == SIG_DFL) && (SHIM_SIG_IGN == SIG_IGN) && (sig_value == 2),
                 SEVERITY_MEDIUM,
                 "locale/signal shim surface");
}

static bool write_report_json(const char *path, const Report *r) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"summary\": {\n");
    fprintf(f, "    \"total_checks\": %d,\n", r->total_checks);
    fprintf(f, "    \"total_passed\": %d,\n", r->total_passed);
    fprintf(f, "    \"total_mismatches\": %d,\n", r->total_mismatches);
    fprintf(f, "    \"high_severity_mismatches\": %d\n", r->total_high_mismatches);
    fprintf(f, "  },\n");

    fprintf(f, "  \"lanes\": [\n");
    for (int i = 0; i < LANE_COUNT; ++i) {
        const LaneStats *ls = &r->lanes[i];
        fprintf(f,
                "    {\"name\":\"%s\",\"checks_total\":%d,\"checks_passed\":%d,\"mismatches\":%d,\"high_mismatches\":%d}%s\n",
                ls->name,
                ls->checks_total,
                ls->checks_passed,
                ls->mismatches,
                ls->high_mismatches,
                (i + 1 < LANE_COUNT) ? "," : "");
    }
    fprintf(f, "  ],\n");

    fprintf(f, "  \"mismatches\": [\n");
    for (int i = 0; i < r->mismatch_count; ++i) {
        const MismatchEntry *m = &r->mismatches[i];
        fprintf(f,
                "    {\"lane\":\"%s\",\"case_id\":\"%s\",\"severity\":\"%s\",\"detail\":\"%s\"}%s\n",
                m->lane,
                m->case_id,
                m->severity,
                m->detail,
                (i + 1 < r->mismatch_count) ? "," : "");
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    fclose(f);
    return true;
}

int main(int argc, char **argv) {
    const char *report_path = "build/sys_shims_conformance_report.json";
    const char *fixture_path = "tests/fixtures/regression_cases_v1.csv";
    Report report = {0};
    CaseSeen required[] = {
        {"types_size_t_parity", false},
        {"types_uint64_parity", false},
        {"types_macro_bounds", false},
        {"stdlib_strtol_valid", false},
        {"stdlib_strtol_invalid", false},
        {"string_memcpy_overflow", false},
        {"errno_set_get", false},
        {"math_sqrt_negative", false},
        {"math_pow_basic", false},
        {"time_now_range", false},
        {"extended_ctype_parity", false},
        {"extended_limits_parity", false},
        {"extended_locale_signal_surface", false}
    };

    if (argc >= 2 && argv[1] && argv[1][0] != '\0') {
        report_path = argv[1];
    }
    if (argc >= 3 && argv[2] && argv[2][0] != '\0') {
        fixture_path = argv[2];
    }

    report_init(&report);

    if (!load_fixture_manifest(fixture_path, required, sizeof(required) / sizeof(required[0]))) {
        record_check(&report,
                     LANE_TYPES,
                     "fixture_manifest_required_cases",
                     false,
                     SEVERITY_HIGH,
                     "missing or invalid regression fixture manifest");
    }

    run_types_lane(&report);
    run_stdlib_lane(&report);
    run_string_lane(&report);
    run_errno_lane(&report);
    run_time_lane(&report);
    run_math_lane(&report);
    run_extended_lane(&report);

    if (!write_report_json(report_path, &report)) {
        fprintf(stderr, "failed to write conformance report: %s\n", report_path);
        return 1;
    }

    printf("sys_shims conformance report: %s\n", report_path);
    printf("checks=%d passed=%d mismatches=%d high=%d\n",
           report.total_checks,
           report.total_passed,
           report.total_mismatches,
           report.total_high_mismatches);

    if (report.total_high_mismatches > 0) {
        fprintf(stderr, "conformance failed: high-severity mismatches present\n");
        return 1;
    }

    return report.total_mismatches == 0 ? 0 : 1;
}
