#ifndef CORE_MEMDB_MEM_CLI_ARGS_H
#define CORE_MEMDB_MEM_CLI_ARGS_H

#include <stdint.h>

#include "core_memdb.h"

typedef enum MemCliOutputFormat {
    MEM_CLI_OUTPUT_TEXT = 0,
    MEM_CLI_OUTPUT_TSV = 1,
    MEM_CLI_OUTPUT_JSON = 2
} MemCliOutputFormat;

void print_usage(const char *argv0);
void print_core_error(const char *context, CoreResult result);

const char *find_flag_value(int argc, char **argv, const char *flag);
int parse_i64_arg(const char *text, int64_t *out_value);
void sleep_for_ms(int64_t delay_ms);
int parse_f64_arg(const char *text, double *out_value);
int has_flag(int argc, char **argv, const char *flag);
int is_allowed_link_kind(const char *kind);
int parse_output_format(int argc, char **argv, MemCliOutputFormat *out_format);

#endif
