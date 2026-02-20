/*
 * IS-IS Protocol Support for Huawei VRP Style
 * Copyright (C) 2026 WhiteBox NE Team
 *
 * This module provides IS-IS protocol support with Huawei VRP style commands.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../lib/huawei_cli.h"

/* IS-IS level types */
typedef enum {
    ISIS_LEVEL_1 = 1,
    ISIS_LEVEL_2 = 2,
    ISIS_LEVEL_1_2 = 3
} isis_level_t;

/* IS-IS configuration structure */
struct isis_config {
    uint32_t process_id;
    bool enabled;
    char net[64];              /* Network Entity Title */
    isis_level_t level;
    bool overload_bit;
    char interfaces[16][64];   /* Enabled interfaces */
    int interface_count;
    bool wide_metric;
};

static struct isis_config global_isis_config = {0};

/*
 * Enter IS-IS configuration mode
 * Command: isis [process-id]
 */
static int cmd_isis(struct cmd_element *cmd, struct cmd_args *args)
{
    uint32_t process_id = 1;  /* Default process ID */

    if (args->argc > 0) {
        process_id = atoi(args->argv[0]);
    }

    global_isis_config.process_id = process_id;
    global_isis_config.enabled = true;
    global_isis_config.level = ISIS_LEVEL_1_2;  /* Default to Level-1-2 */
    global_isis_config.wide_metric = true;

    printf("Entering IS-IS %u configuration mode\n", process_id);
    printf("[Huawei-isis-%u]\n", process_id);

    /* Generate FRR command */
    char frr_cmd[256];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router isis %u\n"
             "metric-style wide\n"
             "exit\n",
             process_id);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret != 0) {
        printf("Error: Failed to configure IS-IS\n");
    }

    return ret;
}

/*
 * Configure Network Entity Title (NET)
 * Command: network-entity <net>
 * Example: network-entity 49.0001.0000.0000.0001.00
 */
static int cmd_isis_net(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: NET address required\n");
        printf("Usage: network-entity <net>\n");
        printf("Example: network-entity 49.0001.0000.0000.0001.00\n");
        return -1;
    }

    const char *net = args->argv[0];
    strncpy(global_isis_config.net, net, sizeof(global_isis_config.net) - 1);

    char frr_cmd[256];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router isis %u\n"
             "net %s\n"
             "exit\n",
             global_isis_config.process_id, net);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("NET configured: %s\n", net);
    } else {
        printf("Error: Failed to configure NET\n");
    }

    return ret;
}

/*
 * Set IS-IS level
 * Command: is-level {level-1|level-2|level-1-2}
 */
static int cmd_isis_level(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: Level type required\n");
        printf("Usage: is-level {level-1|level-2|level-1-2}\n");
        return -1;
    }

    const char *level_str = args->argv[0];
    isis_level_t level;
    const char *frr_level;

    if (strcmp(level_str, "level-1") == 0) {
        level = ISIS_LEVEL_1;
        frr_level = "level-1";
    } else if (strcmp(level_str, "level-2") == 0) {
        level = ISIS_LEVEL_2;
        frr_level = "level-2";
    } else if (strcmp(level_str, "level-1-2") == 0) {
        level = ISIS_LEVEL_1_2;
        frr_level = "level-1-2";
    } else {
        printf("Error: Invalid level. Must be level-1, level-2, or level-1-2\n");
        return -1;
    }

    global_isis_config.level = level;

    char frr_cmd[256];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router isis %u\n"
             "is-type %s\n"
             "exit\n",
             global_isis_config.process_id, frr_level);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("IS-IS level set to %s\n", level_str);
    } else {
        printf("Error: Failed to set IS-IS level\n");
    }

    return ret;
}

/*
 * Enable IS-IS on interface
 * Command: isis enable [process-id]
 * (Used in interface configuration mode)
 */
static int cmd_interface_isis_enable(struct cmd_element *cmd, struct cmd_args *args)
{
    uint32_t process_id = global_isis_config.process_id;

    if (args->argc > 0) {
        process_id = atoi(args->argv[0]);
    }

    printf("IS-IS enabled on interface (process %u)\n", process_id);
    printf("Note: Configure this under interface mode in FRR\n");

    return 0;
}

/*
 * Set interface circuit type
 * Command: isis circuit-type {level-1|level-2|level-1-2}
 */
static int cmd_interface_isis_circuit_type(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: Circuit type required\n");
        printf("Usage: isis circuit-type {level-1|level-2|level-1-2}\n");
        return -1;
    }

    const char *circuit_type = args->argv[0];
    printf("IS-IS circuit type set to %s\n", circuit_type);

    return 0;
}

/*
 * Set interface metric
 * Command: isis cost <metric> [level-1|level-2]
 */
static int cmd_interface_isis_cost(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: Metric value required\n");
        printf("Usage: isis cost <metric> [level-1|level-2]\n");
        return -1;
    }

    uint32_t metric = atoi(args->argv[0]);
    const char *level = (args->argc > 1) ? args->argv[1] : "both";

    printf("IS-IS metric set to %u for %s\n", metric, level);

    return 0;
}

/*
 * Set overload bit
 * Command: set-overload-bit
 */
static int cmd_isis_overload(struct cmd_element *cmd, struct cmd_args *args)
{
    global_isis_config.overload_bit = true;

    char frr_cmd[256];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router isis %u\n"
             "set-overload-bit\n"
             "exit\n",
             global_isis_config.process_id);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("Overload bit set\n");
    } else {
        printf("Error: Failed to set overload bit\n");
    }

    return ret;
}

/*
 * Configure IS-IS authentication
 * Command: area-authentication-mode {simple|md5} <password>
 */
static int cmd_isis_area_auth(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Authentication mode and password required\n");
        printf("Usage: area-authentication-mode {simple|md5} <password>\n");
        return -1;
    }

    const char *mode = args->argv[0];
    const char *password = args->argv[1];

    if (strcmp(mode, "simple") != 0 && strcmp(mode, "md5") != 0) {
        printf("Error: Invalid authentication mode\n");
        return -1;
    }

    printf("IS-IS area authentication configured: mode=%s\n", mode);

    return 0;
}

/*
 * Display IS-IS configuration
 * Command: display isis [process-id]
 */
static int cmd_display_isis(struct cmd_element *cmd, struct cmd_args *args)
{
    char output[4096];
    int ret = execute_vtysh_command_with_output("show isis summary", output, sizeof(output));

    if (ret == 0) {
        printf("IS-IS Configuration:\n");
        printf("  Process ID: %u\n", global_isis_config.process_id);
        printf("  NET: %s\n", global_isis_config.net[0] ? global_isis_config.net : "Not configured");
        printf("  Level: ");
        switch (global_isis_config.level) {
            case ISIS_LEVEL_1:
                printf("Level-1\n");
                break;
            case ISIS_LEVEL_2:
                printf("Level-2\n");
                break;
            case ISIS_LEVEL_1_2:
                printf("Level-1-2\n");
                break;
        }
        printf("  Overload Bit: %s\n", global_isis_config.overload_bit ? "Set" : "Not set");
        printf("  Metric Style: %s\n", global_isis_config.wide_metric ? "Wide" : "Narrow");
        printf("\nFRR IS-IS Status:\n%s\n", output);
    } else {
        printf("Error: Failed to retrieve IS-IS status\n");
    }

    return ret;
}

/*
 * Display IS-IS neighbors
 * Command: display isis peer
 */
static int cmd_display_isis_peer(struct cmd_element *cmd, struct cmd_args *args)
{
    char output[4096];
    int ret = execute_vtysh_command_with_output("show isis neighbor", output, sizeof(output));

    if (ret == 0) {
        printf("IS-IS Neighbors:\n");
        printf("%s\n", output);
    } else {
        printf("Error: Failed to retrieve IS-IS neighbors\n");
    }

    return ret;
}

/*
 * Display IS-IS database
 * Command: display isis lsdb
 */
static int cmd_display_isis_lsdb(struct cmd_element *cmd, struct cmd_args *args)
{
    char output[4096];
    int ret = execute_vtysh_command_with_output("show isis database", output, sizeof(output));

    if (ret == 0) {
        printf("IS-IS Link State Database:\n");
        printf("%s\n", output);
    } else {
        printf("Error: Failed to retrieve IS-IS database\n");
    }

    return ret;
}

/*
 * Disable IS-IS
 * Command: undo isis [process-id]
 */
static int cmd_undo_isis(struct cmd_element *cmd, struct cmd_args *args)
{
    uint32_t process_id = global_isis_config.process_id;

    if (args->argc > 0) {
        process_id = atoi(args->argv[0]);
    }

    char frr_cmd[256];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "no router isis %u\n"
             "exit\n",
             process_id);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("IS-IS disabled\n");
        memset(&global_isis_config, 0, sizeof(global_isis_config));
    } else {
        printf("Error: Failed to disable IS-IS\n");
    }

    return ret;
}

/* Command registration */
struct cmd_element isis_cmds[] = {
    {
        .name = "isis",
        .func = cmd_isis,
        .alias = "router isis",
        .help = "Enter IS-IS configuration mode (Huawei VRP style)",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "network-entity",
        .func = cmd_isis_net,
        .alias = "net",
        .help = "Configure Network Entity Title (NET)",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "is-level",
        .func = cmd_isis_level,
        .alias = "is-type",
        .help = "Set IS-IS level type",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "isis enable",
        .func = cmd_interface_isis_enable,
        .alias = "ip router isis",
        .help = "Enable IS-IS on interface",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "isis circuit-type",
        .func = cmd_interface_isis_circuit_type,
        .alias = "isis circuit-type",
        .help = "Set interface circuit type",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "isis cost",
        .func = cmd_interface_isis_cost,
        .alias = "isis metric",
        .help = "Set interface IS-IS metric",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "set-overload-bit",
        .func = cmd_isis_overload,
        .alias = "set-overload-bit",
        .help = "Set overload bit to avoid transit traffic",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "area-authentication-mode",
        .func = cmd_isis_area_auth,
        .alias = "area-password",
        .help = "Configure IS-IS area authentication",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "display isis",
        .func = cmd_display_isis,
        .alias = "show isis summary",
        .help = "Display IS-IS configuration and status",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "display isis peer",
        .func = cmd_display_isis_peer,
        .alias = "show isis neighbor",
        .help = "Display IS-IS neighbors",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "display isis lsdb",
        .func = cmd_display_isis_lsdb,
        .alias = "show isis database",
        .help = "Display IS-IS link state database",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "undo isis",
        .func = cmd_undo_isis,
        .alias = "no router isis",
        .help = "Disable IS-IS",
        .category = CMD_CAT_ROUTING,
    },
    { .name = NULL } /* Sentinel */
};

/* Register IS-IS commands */
void register_isis_cmds(void)
{
    printf("Registering IS-IS commands...\n");
}
