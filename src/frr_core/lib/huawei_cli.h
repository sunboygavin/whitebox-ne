/*
 * Huawei VRP Style CLI Extension
 * Copyright (C) 2026 WhiteBox NE Team
 *
 * This file provides Huawei VRP style command line interface extensions
 * for FRRouting, enabling compatibility with Huawei router commands.
 */

#ifndef _HUAWEI_CLI_H
#define _HUAWEI_CLI_H

#include <stdint.h>
#include <stdbool.h>

/* Command categories */
enum huawei_cmd_category {
    CMD_CAT_SYSTEM,      /* System commands */
    CMD_CAT_INTERFACE,   /* Interface commands */
    CMD_CAT_ROUTING,     /* Routing commands */
    CMD_CAT_IP_SERVICE,  /* IP service commands */
    CMD_CAT_SECURITY,    /* Security commands */
    CMD_CAT_QOS,         /* QoS commands */
    CMD_CAT_HA,          /* High availability commands */
    CMD_CAT_MONITOR      /* Monitoring commands */
};

/* Command arguments structure */
struct cmd_args {
    int argc;        /* Argument count */
    char **argv;     /* Argument vector */
};

/* Command element structure */
struct cmd_element {
    const char *name;                              /* Command name */
    int (*func)(struct cmd_element *, struct cmd_args *);  /* Handler function */
    const char *alias;                             /* Cisco style alias */
    const char *help;                              /* Help text */
    enum huawei_cmd_category category;             /* Command category */
    int subcmd_count;                              /* Subcommand count */
    struct cmd_element **subcmds;                  /* Subcommand array */
    bool (*validate)(struct cmd_args *);           /* Validation function */
};

/* Command registration functions */
void register_huawei_system_cmds(void);
void register_huawei_interface_cmds(void);
void register_huawei_routing_cmds(void);
void register_huawei_ip_service_cmds(void);
void register_huawei_security_cmds(void);
void register_huawei_qos_cmds(void);
void register_huawei_ha_cmds(void);
void register_huawei_monitor_cmds(void);

/* Utility functions */
int execute_vtysh_command(const char *cmd);
int execute_vtysh_command_with_output(const char *cmd, char *output, size_t output_size);
bool validate_ip_address(const char *ip);
bool validate_ipv6_address(const char *ip);
bool validate_prefix_length(const char *prefix, bool is_ipv6);

/* Command registration macro */
#define HUAWEI_CMD(name, func, alias, help) \
    { .name = name, .func = func, .alias = alias, .help = help, \
      .category = CMD_CAT_SYSTEM, .subcmd_count = 0, .subcmds = NULL, .validate = NULL }

#define HUAWEI_CMD_WITH_CATEGORY(name, func, alias, help, cat) \
    { .name = name, .func = func, .alias = alias, .help = help, \
      .category = cat, .subcmd_count = 0, .subcmds = NULL, .validate = NULL }

#endif /* _HUAWEI_CLI_H */
