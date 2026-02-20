/*
 * RIP Protocol Support for Huawei VRP Style
 * Copyright (C) 2026 WhiteBox NE Team
 *
 * This module provides RIP v1/v2 protocol support with Huawei VRP style commands.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../lib/huawei_cli.h"

/* RIP configuration structure */
struct rip_config {
    uint32_t process_id;
    uint8_t version;           /* 1 or 2 */
    bool enabled;
    char networks[32][64];     /* Network addresses */
    int network_count;
    char silent_interfaces[16][64];  /* Silent interfaces */
    int silent_count;
    bool authentication_enabled;
    char authentication_key[64];
};

static struct rip_config global_rip_config = {0};

/*
 * Enter RIP configuration mode
 * Command: rip [process-id]
 */
static int cmd_rip(struct cmd_element *cmd, struct cmd_args *args)
{
    uint32_t process_id = 1;  /* Default process ID */

    if (args->argc > 0) {
        process_id = atoi(args->argv[0]);
    }

    global_rip_config.process_id = process_id;
    global_rip_config.enabled = true;
    global_rip_config.version = 2;  /* Default to RIP v2 */

    printf("Entering RIP %u configuration mode\n", process_id);
    printf("[Huawei-rip-%u]\n", process_id);

    /* Generate FRR command */
    char frr_cmd[256];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router rip\n"
             "version 2\n"
             "exit\n");

    int ret = execute_vtysh_command(frr_cmd);
    if (ret != 0) {
        printf("Error: Failed to configure RIP\n");
    }

    return ret;
}

/*
 * Set RIP version
 * Command: version {1|2}
 */
static int cmd_rip_version(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: Version number required\n");
        printf("Usage: version {1|2}\n");
        return -1;
    }

    uint8_t version = atoi(args->argv[0]);
    if (version != 1 && version != 2) {
        printf("Error: Invalid version. Must be 1 or 2\n");
        return -1;
    }

    global_rip_config.version = version;

    char frr_cmd[256];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router rip\n"
             "version %u\n"
             "exit\n",
             version);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("RIP version set to %u\n", version);
    } else {
        printf("Error: Failed to set RIP version\n");
    }

    return ret;
}

/*
 * Add network to RIP
 * Command: network <network-address>
 */
static int cmd_rip_network(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: Network address required\n");
        printf("Usage: network <network-address>\n");
        return -1;
    }

    const char *network = args->argv[0];

    /* Add to configuration */
    if (global_rip_config.network_count < 32) {
        strncpy(global_rip_config.networks[global_rip_config.network_count],
                network, 63);
        global_rip_config.network_count++;
    }

    char frr_cmd[256];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router rip\n"
             "network %s\n"
             "exit\n",
             network);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("Network %s added to RIP\n", network);
    } else {
        printf("Error: Failed to add network to RIP\n");
    }

    return ret;
}

/*
 * Configure silent interface
 * Command: silent-interface <interface-name>
 */
static int cmd_rip_silent_interface(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: Interface name required\n");
        printf("Usage: silent-interface <interface-name>\n");
        return -1;
    }

    const char *interface = args->argv[0];

    /* Add to configuration */
    if (global_rip_config.silent_count < 16) {
        strncpy(global_rip_config.silent_interfaces[global_rip_config.silent_count],
                interface, 63);
        global_rip_config.silent_count++;
    }

    char frr_cmd[256];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router rip\n"
             "passive-interface %s\n"
             "exit\n",
             interface);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("Interface %s configured as silent\n", interface);
    } else {
        printf("Error: Failed to configure silent interface\n");
    }

    return ret;
}

/*
 * Configure RIP authentication
 * Command: authentication-mode {simple|md5} <password>
 */
static int cmd_rip_authentication(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Authentication mode and password required\n");
        printf("Usage: authentication-mode {simple|md5} <password>\n");
        return -1;
    }

    const char *mode = args->argv[0];
    const char *password = args->argv[1];

    if (strcmp(mode, "simple") != 0 && strcmp(mode, "md5") != 0) {
        printf("Error: Invalid authentication mode. Must be 'simple' or 'md5'\n");
        return -1;
    }

    global_rip_config.authentication_enabled = true;
    strncpy(global_rip_config.authentication_key, password, 63);

    printf("RIP authentication configured: mode=%s\n", mode);
    printf("Note: Authentication must be configured per-interface in FRR\n");

    return 0;
}

/*
 * Display RIP configuration
 * Command: display rip [process-id]
 */
static int cmd_display_rip(struct cmd_element *cmd, struct cmd_args *args)
{
    char output[4096];
    int ret = execute_vtysh_command_with_output("show ip rip status", output, sizeof(output));

    if (ret == 0) {
        printf("RIP Configuration:\n");
        printf("  Process ID: %u\n", global_rip_config.process_id);
        printf("  Version: %u\n", global_rip_config.version);
        printf("  Enabled: %s\n", global_rip_config.enabled ? "Yes" : "No");
        printf("  Networks: %d configured\n", global_rip_config.network_count);
        for (int i = 0; i < global_rip_config.network_count; i++) {
            printf("    - %s\n", global_rip_config.networks[i]);
        }
        printf("  Silent Interfaces: %d configured\n", global_rip_config.silent_count);
        for (int i = 0; i < global_rip_config.silent_count; i++) {
            printf("    - %s\n", global_rip_config.silent_interfaces[i]);
        }
        printf("\nFRR RIP Status:\n%s\n", output);
    } else {
        printf("Error: Failed to retrieve RIP status\n");
    }

    return ret;
}

/*
 * Display RIP database
 * Command: display rip database
 */
static int cmd_display_rip_database(struct cmd_element *cmd, struct cmd_args *args)
{
    char output[4096];
    int ret = execute_vtysh_command_with_output("show ip rip", output, sizeof(output));

    if (ret == 0) {
        printf("RIP Database:\n");
        printf("%s\n", output);
    } else {
        printf("Error: Failed to retrieve RIP database\n");
    }

    return ret;
}

/*
 * Disable RIP
 * Command: undo rip [process-id]
 */
static int cmd_undo_rip(struct cmd_element *cmd, struct cmd_args *args)
{
    char frr_cmd[256];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "no router rip\n"
             "exit\n");

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("RIP disabled\n");
        memset(&global_rip_config, 0, sizeof(global_rip_config));
    } else {
        printf("Error: Failed to disable RIP\n");
    }

    return ret;
}

/* Command registration */
struct cmd_element rip_cmds[] = {
    {
        .name = "rip",
        .func = cmd_rip,
        .alias = "router rip",
        .help = "Enter RIP configuration mode (Huawei VRP style)",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "version",
        .func = cmd_rip_version,
        .alias = "version",
        .help = "Set RIP version (1 or 2)",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "network",
        .func = cmd_rip_network,
        .alias = "network",
        .help = "Add network to RIP",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "silent-interface",
        .func = cmd_rip_silent_interface,
        .alias = "passive-interface",
        .help = "Configure interface as silent (no RIP updates sent)",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "authentication-mode",
        .func = cmd_rip_authentication,
        .alias = "ip rip authentication",
        .help = "Configure RIP authentication",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "display rip",
        .func = cmd_display_rip,
        .alias = "show ip rip status",
        .help = "Display RIP configuration and status",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "display rip database",
        .func = cmd_display_rip_database,
        .alias = "show ip rip",
        .help = "Display RIP routing database",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "undo rip",
        .func = cmd_undo_rip,
        .alias = "no router rip",
        .help = "Disable RIP",
        .category = CMD_CAT_ROUTING,
    },
    { .name = NULL } /* Sentinel */
};

/* Register RIP commands */
void register_rip_cmds(void)
{
    printf("Registering RIP commands...\n");
}
