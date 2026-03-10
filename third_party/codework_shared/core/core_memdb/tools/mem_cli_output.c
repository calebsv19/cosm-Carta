#include "mem_cli_output.h"

#include <stdio.h>

void print_str(CoreStr value) {
    if (value.data && value.len > 0u) {
        (void)fwrite(value.data, 1u, value.len, stdout);
    }
}

static void print_tsv_sanitized_segment(const char *text, size_t len) {
    size_t i;

    if (!text || len == 0u) {
        return;
    }

    for (i = 0u; i < len; ++i) {
        char c = text[i];
        if (c == '\t' || c == '\n' || c == '\r') {
            fputc(' ', stdout);
        } else {
            fputc((int)c, stdout);
        }
    }
}

void print_tsv_sanitized_str(CoreStr value) {
    if (!value.data || value.len == 0u) {
        return;
    }
    print_tsv_sanitized_segment(value.data, value.len);
}

static void print_json_escaped_segment(const char *text, size_t len) {
    size_t i;

    fputc('"', stdout);
    if (text && len > 0u) {
        for (i = 0u; i < len; ++i) {
            unsigned char c = (unsigned char)text[i];
            switch (c) {
                case '\"':
                    fputs("\\\"", stdout);
                    break;
                case '\\':
                    fputs("\\\\", stdout);
                    break;
                case '\b':
                    fputs("\\b", stdout);
                    break;
                case '\f':
                    fputs("\\f", stdout);
                    break;
                case '\n':
                    fputs("\\n", stdout);
                    break;
                case '\r':
                    fputs("\\r", stdout);
                    break;
                case '\t':
                    fputs("\\t", stdout);
                    break;
                default:
                    if (c < 0x20u) {
                        (void)fprintf(stdout, "\\u%04x", (unsigned int)c);
                    } else {
                        fputc((int)c, stdout);
                    }
                    break;
            }
        }
    }
    fputc('"', stdout);
}

void print_json_escaped_str(CoreStr value) {
    if (!value.data || value.len == 0u) {
        print_json_escaped_segment("", 0u);
        return;
    }
    print_json_escaped_segment(value.data, value.len);
}
