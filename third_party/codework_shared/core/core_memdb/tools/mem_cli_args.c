#include "mem_cli_args.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *k_allowed_link_kinds[] = {
    "supports",
    "depends_on",
    "references",
    "summarizes",
    "implements",
    "blocks",
    "contradicts",
    "related"
};

void print_usage(const char *argv0) {
    fprintf(stderr, "usage: %s <command> [args]\n", argv0);
    fprintf(stderr, "commands:\n");
    fprintf(stderr, "  add  --db <path> --title <text> --body <text> [--stable-id <id>] [--workspace <key>] [--project <key>] [--kind <value>] [--session-id <id>] [--session-max-writes <n>]\n");
    fprintf(stderr, "  batch-add --db <path> --input <tsv_path> [--workspace <key>] [--project <key>] [--kind <value>] [--session-id <id>] [--session-max-writes <n>] [--continue-on-error] [--max-errors <n>] [--retry-attempts <n>] [--retry-delay-ms <ms>]\n");
    fprintf(stderr, "  list --db <path> [--format text|tsv|json]\n");
    fprintf(stderr, "  find --db <path> --query <text> [--format text|tsv|json]\n");
    fprintf(stderr, "  query --db <path> [--query <text>] [--limit <n>] [--offset <n>] [--pinned-only] [--canonical-only] [--include-archived] [--workspace <key>] [--project <key>] [--kind <value>] [--format text|tsv|json]\n");
    fprintf(stderr, "  show --db <path> --id <rowid> [--format text|tsv|json]\n");
    fprintf(stderr, "  health --db <path> [--format text|json]\n");
    fprintf(stderr, "  audit-list --db <path> [--session-id <id>] [--limit <n>] [--format text|tsv|json]\n");
    fprintf(stderr, "  event-list --db <path> [--session-id <id>] [--event-type <type>] [--limit <n>] [--format text|tsv|json]\n");
    fprintf(stderr, "  event-replay-check --db <path> [--limit-events <n>] [--format text|json]\n");
    fprintf(stderr, "  event-replay-apply --db <source_path> --out-db <target_path> [--limit-events <n>] [--format text|json]\n");
    fprintf(stderr, "  event-backfill --db <path> [--session-id <id>] [--dry-run] [--format text|json]\n");
    fprintf(stderr, "  pin --db <path> --id <rowid> --on|--off [--session-id <id>]\n");
    fprintf(stderr, "  canonical --db <path> --id <rowid> --on|--off [--session-id <id>]\n");
    fprintf(stderr, "  rollup --db <path> --before <timestamp_ns> [--workspace <key>] [--project <key>] [--kind <value>] [--limit <n>] [--session-id <id>]\n");
    fprintf(stderr, "  link-add --db <path> --from <item_id> --to <item_id> --kind <text> [--weight <real>] [--note <text>] [--session-id <id>]\n");
    fprintf(stderr, "  link-list --db <path> --item-id <item_id>\n");
    fprintf(stderr, "  neighbors --db <path> --item-id <item_id> [--kind <text>] [--max-edges <n>] [--max-nodes <n>] [--format text|tsv|json]\n");
    fprintf(stderr, "  link-update --db <path> --id <link_id> --kind <text> [--weight <real>] [--note <text>] [--session-id <id>]\n");
    fprintf(stderr, "  link-remove --db <path> --id <link_id> [--session-id <id>]\n");
    fprintf(stderr, "  help\n");
}

void print_core_error(const char *context, CoreResult result) {
    fprintf(stderr, "%s: %s\n", context, result.message ? result.message : "error");
}

const char *find_flag_value(int argc, char **argv, const char *flag) {
    int i;

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], flag) == 0) {
            if ((i + 1) >= argc) {
                return 0;
            }
            return argv[i + 1];
        }
    }

    return 0;
}

int parse_i64_arg(const char *text, int64_t *out_value) {
    char *end_ptr = 0;
    long long parsed_value;

    if (!text || !out_value) {
        return 0;
    }

    parsed_value = strtoll(text, &end_ptr, 10);
    if (!end_ptr || *end_ptr != '\0') {
        return 0;
    }

    *out_value = (int64_t)parsed_value;
    return 1;
}

void sleep_for_ms(int64_t delay_ms) {
    struct timespec request;

    if (delay_ms <= 0) {
        return;
    }

    request.tv_sec = (time_t)(delay_ms / 1000);
    request.tv_nsec = (long)((delay_ms % 1000) * 1000000LL);
    while (nanosleep(&request, &request) == -1 && errno == EINTR) {
    }
}

int parse_f64_arg(const char *text, double *out_value) {
    char *end_ptr = 0;
    double parsed_value;

    if (!text || !out_value) {
        return 0;
    }

    errno = 0;
    parsed_value = strtod(text, &end_ptr);
    if (errno != 0 || !end_ptr || *end_ptr != '\0') {
        return 0;
    }

    *out_value = parsed_value;
    return 1;
}

int has_flag(int argc, char **argv, const char *flag) {
    int i;

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], flag) == 0) {
            return 1;
        }
    }

    return 0;
}

int is_allowed_link_kind(const char *kind) {
    size_t i;

    if (!kind || kind[0] == '\0') {
        return 0;
    }
    for (i = 0u; i < sizeof(k_allowed_link_kinds) / sizeof(k_allowed_link_kinds[0]); ++i) {
        if (strcmp(kind, k_allowed_link_kinds[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int parse_output_format(int argc, char **argv, MemCliOutputFormat *out_format) {
    const char *value;

    if (!out_format) {
        return 0;
    }

    *out_format = MEM_CLI_OUTPUT_TEXT;
    value = find_flag_value(argc, argv, "--format");
    if (!value || value[0] == '\0') {
        return 1;
    }

    if (strcmp(value, "text") == 0) {
        *out_format = MEM_CLI_OUTPUT_TEXT;
        return 1;
    }
    if (strcmp(value, "tsv") == 0) {
        *out_format = MEM_CLI_OUTPUT_TSV;
        return 1;
    }
    if (strcmp(value, "json") == 0) {
        *out_format = MEM_CLI_OUTPUT_JSON;
        return 1;
    }

    fprintf(stderr, "invalid --format value: %s (expected text|tsv|json)\n", value);
    return 0;
}
