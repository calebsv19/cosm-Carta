#ifndef MEM_CLI_CMD_WRITE_ITEM_H
#define MEM_CLI_CMD_WRITE_ITEM_H

int cmd_add(int argc, char **argv);
int cmd_batch_add(int argc, char **argv);
int cmd_set_bool_field(int argc, char **argv, const char *command_name, const char *field_name);
int cmd_item_retag(int argc, char **argv);
int cmd_rollup(int argc, char **argv);

#endif
