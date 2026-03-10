#ifndef CORE_MEMDB_MEM_CLI_OUTPUT_H
#define CORE_MEMDB_MEM_CLI_OUTPUT_H

#include "core_memdb.h"

void print_str(CoreStr value);
void print_tsv_sanitized_str(CoreStr value);
void print_json_escaped_str(CoreStr value);

#endif
