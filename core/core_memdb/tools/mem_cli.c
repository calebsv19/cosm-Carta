#include <stdio.h>
#include <string.h>

#include "mem_cli_args.h"
#include "mem_cli_cmd_event.h"
#include "mem_cli_cmd_read.h"
#include "mem_cli_cmd_write_item.h"
#include "mem_cli_cmd_write_link.h"

int main(int argc, char **argv) {
    const char *command;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    command = argv[1];
    if (strcmp(command, "help") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    if (strcmp(command, "add") == 0) {
        return cmd_add(argc, argv);
    }
    if (strcmp(command, "batch-add") == 0) {
        return cmd_batch_add(argc, argv);
    }
    if (strcmp(command, "list") == 0) {
        return cmd_list(argc, argv);
    }
    if (strcmp(command, "find") == 0) {
        return cmd_find(argc, argv);
    }
    if (strcmp(command, "query") == 0) {
        return cmd_query(argc, argv);
    }
    if (strcmp(command, "show") == 0) {
        return cmd_show(argc, argv);
    }
    if (strcmp(command, "health") == 0) {
        return cmd_health(argc, argv);
    }
    if (strcmp(command, "audit-list") == 0) {
        return cmd_audit_list(argc, argv);
    }
    if (strcmp(command, "event-list") == 0) {
        return cmd_event_list(argc, argv);
    }
    if (strcmp(command, "event-replay-check") == 0) {
        return cmd_event_replay_check(argc, argv);
    }
    if (strcmp(command, "event-replay-apply") == 0) {
        return cmd_event_replay_apply(argc, argv);
    }
    if (strcmp(command, "event-backfill") == 0) {
        return cmd_event_backfill(argc, argv);
    }
    if (strcmp(command, "pin") == 0) {
        return cmd_set_bool_field(argc, argv, "pin", "pinned");
    }
    if (strcmp(command, "canonical") == 0) {
        return cmd_set_bool_field(argc, argv, "canonical", "canonical");
    }
    if (strcmp(command, "item-retag") == 0) {
        return cmd_item_retag(argc, argv);
    }
    if (strcmp(command, "rollup") == 0) {
        return cmd_rollup(argc, argv);
    }
    if (strcmp(command, "link-add") == 0) {
        return cmd_link_add(argc, argv);
    }
    if (strcmp(command, "link-list") == 0) {
        return cmd_link_list(argc, argv);
    }
    if (strcmp(command, "neighbors") == 0) {
        return cmd_neighbors(argc, argv);
    }
    if (strcmp(command, "link-update") == 0) {
        return cmd_link_update(argc, argv);
    }
    if (strcmp(command, "link-remove") == 0) {
        return cmd_link_remove(argc, argv);
    }

    fprintf(stderr, "unknown command: %s\n", command);
    print_usage(argv[0]);
    return 1;
}
