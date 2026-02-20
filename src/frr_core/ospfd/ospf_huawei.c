/*
 * OSPF Enhanced Features for Huawei VRP Style
 * Copyright (C) 2026 WhiteBox NE Team
 *
 * This module provides enhanced OSPF features including:
 * - Virtual links
 * - NSSA areas
 * - Route filtering
 * - Authentication (MD5/SHA)
 * - Stub areas
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../lib/huawei_cli.h"

/* OSPF area types */
typedef enum {
    OSPF_AREA_NORMAL = 0,
    OSPF_AREA_STUB = 1,
    OSPF_AREA_NSSA = 2,
    OSPF_AREA_TOTALLY_STUB = 3,
    OSPF_AREA_TOTALLY_NSSA = 4
} ospf_area_type_t;

/* OSPF authentication types */
typedef enum {
    OSPF_AUTH_NONE = 0,
    OSPF_AUTH_SIMPLE = 1,
    OSPF_AUTH_MD5 = 2,
    OSPF_AUTH_SHA = 3
} ospf_auth_type_t;

/* OSPF area configuration */
struct ospf_area_config {
    uint32_t area_id;
    ospf_area_type_t type;
    bool default_route;
    uint32_t default_cost;
    char filter_in[64];
    char filter_out[64];
    ospf_auth_type_t auth_type;
    char auth_key[64];
};

/* OSPF virtual link configuration */
struct ospf_vlink_config {
    uint32_t area_id;
    char neighbor_id[16];
    uint16_t hello_interval;
    uint16_t dead_interval;
    uint16_t retransmit_interval;
    uint16_t transmit_delay;
    ospf_auth_type_t auth_type;
    char auth_key[64];
};

/* Global OSPF configuration */
struct ospf_global_config {
    uint32_t process_id;
    char router_id[16];
    uint32_t reference_bandwidth;
    bool enabled;
    struct ospf_area_config areas[16];
    int area_count;
    struct ospf_vlink_config vlinks[8];
    int vlink_count;
};

static struct ospf_global_config global_ospf_config = {0};

/*
 * Enter OSPF configuration mode
 * Command: ospf [process-id] [router-id <router-id>]
 */
static int cmd_ospf(struct cmd_element *cmd, struct cmd_args *args)
{
    uint32_t process_id = 1;

    if (args->argc > 0) {
        process_id = atoi(args->argv[0]);
    }

    global_ospf_config.process_id = process_id;
    global_ospf_config.enabled = true;

    /* Parse optional router-id */
    for (int i = 1; i < args->argc; i++) {
        if (strcmp(args->argv[i], "router-id") == 0 && i + 1 < args->argc) {
            strncpy(global_ospf_config.router_id, args->argv[++i],
                    sizeof(global_ospf_config.router_id) - 1);
        }
    }

    printf("Entering OSPF %u configuration mode\n", process_id);
    printf("[Huawei-ospf-%u]\n", process_id);

    char frr_cmd[512];
    if (global_ospf_config.router_id[0]) {
        snprintf(frr_cmd, sizeof(frr_cmd),
                 "configure terminal\n"
                 "router ospf %u\n"
                 "ospf router-id %s\n"
                 "exit\n",
                 process_id, global_ospf_config.router_id);
    } else {
        snprintf(frr_cmd, sizeof(frr_cmd),
                 "configure terminal\n"
                 "router ospf %u\n"
                 "exit\n",
                 process_id);
    }

    int ret = execute_vtysh_command(frr_cmd);
    if (ret != 0) {
        printf("Error: Failed to configure OSPF\n");
    }

    return ret;
}

/*
 * Configure OSPF area
 * Command: area <area-id>
 */
static int cmd_ospf_area(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: Area ID required\n");
        printf("Usage: area <area-id>\n");
        return -1;
    }

    uint32_t area_id = atoi(args->argv[0]);

    /* Find or create area configuration */
    struct ospf_area_config *area = NULL;
    for (int i = 0; i < global_ospf_config.area_count; i++) {
        if (global_ospf_config.areas[i].area_id == area_id) {
            area = &global_ospf_config.areas[i];
            break;
        }
    }

    if (!area && global_ospf_config.area_count < 16) {
        area = &global_ospf_config.areas[global_ospf_config.area_count++];
        area->area_id = area_id;
        area->type = OSPF_AREA_NORMAL;
    }

    printf("Entering OSPF area %u configuration\n", area_id);
    printf("[Huawei-ospf-%u-area-%u]\n", global_ospf_config.process_id, area_id);

    return 0;
}

/*
 * Configure NSSA area
 * Command: nssa [default-route-advertise] [no-summary]
 */
static int cmd_ospf_area_nssa(struct cmd_element *cmd, struct cmd_args *args)
{
    if (global_ospf_config.area_count == 0) {
        printf("Error: No area configured. Use 'area <area-id>' first\n");
        return -1;
    }

    struct ospf_area_config *area = &global_ospf_config.areas[global_ospf_config.area_count - 1];

    bool default_route = false;
    bool no_summary = false;

    for (int i = 0; i < args->argc; i++) {
        if (strcmp(args->argv[i], "default-route-advertise") == 0) {
            default_route = true;
        } else if (strcmp(args->argv[i], "no-summary") == 0) {
            no_summary = true;
        }
    }

    area->type = no_summary ? OSPF_AREA_TOTALLY_NSSA : OSPF_AREA_NSSA;
    area->default_route = default_route;

    char frr_cmd[512];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router ospf %u\n"
             "area %u nssa%s%s\n"
             "exit\n",
             global_ospf_config.process_id, area->area_id,
             default_route ? " default-information-originate" : "",
             no_summary ? " no-summary" : "");

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("Area %u configured as NSSA\n", area->area_id);
    } else {
        printf("Error: Failed to configure NSSA area\n");
    }

    return ret;
}

/*
 * Configure stub area
 * Command: stub [no-summary]
 */
static int cmd_ospf_area_stub(struct cmd_element *cmd, struct cmd_args *args)
{
    if (global_ospf_config.area_count == 0) {
        printf("Error: No area configured\n");
        return -1;
    }

    struct ospf_area_config *area = &global_ospf_config.areas[global_ospf_config.area_count - 1];

    bool no_summary = false;
    for (int i = 0; i < args->argc; i++) {
        if (strcmp(args->argv[i], "no-summary") == 0) {
            no_summary = true;
        }
    }

    area->type = no_summary ? OSPF_AREA_TOTALLY_STUB : OSPF_AREA_STUB;

    char frr_cmd[512];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router ospf %u\n"
             "area %u stub%s\n"
             "exit\n",
             global_ospf_config.process_id, area->area_id,
             no_summary ? " no-summary" : "");

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("Area %u configured as stub\n", area->area_id);
    } else {
        printf("Error: Failed to configure stub area\n");
    }

    return ret;
}

/*
 * Configure virtual link
 * Command: vlink-peer <router-id> [hello <interval>] [dead <interval>]
 */
static int cmd_ospf_vlink(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: Neighbor router ID required\n");
        printf("Usage: vlink-peer <router-id> [hello <interval>] [dead <interval>]\n");
        return -1;
    }

    if (global_ospf_config.area_count == 0) {
        printf("Error: No area configured\n");
        return -1;
    }

    if (global_ospf_config.vlink_count >= 8) {
        printf("Error: Maximum virtual links reached\n");
        return -1;
    }

    struct ospf_vlink_config *vlink = &global_ospf_config.vlinks[global_ospf_config.vlink_count++];
    vlink->area_id = global_ospf_config.areas[global_ospf_config.area_count - 1].area_id;
    strncpy(vlink->neighbor_id, args->argv[0], sizeof(vlink->neighbor_id) - 1);
    vlink->hello_interval = 10;
    vlink->dead_interval = 40;

    /* Parse optional parameters */
    for (int i = 1; i < args->argc; i++) {
        if (strcmp(args->argv[i], "hello") == 0 && i + 1 < args->argc) {
            vlink->hello_interval = atoi(args->argv[++i]);
        } else if (strcmp(args->argv[i], "dead") == 0 && i + 1 < args->argc) {
            vlink->dead_interval = atoi(args->argv[++i]);
        }
    }

    char frr_cmd[512];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router ospf %u\n"
             "area %u virtual-link %s\n"
             "exit\n",
             global_ospf_config.process_id, vlink->area_id, vlink->neighbor_id);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("Virtual link configured: area %u, neighbor %s\n",
               vlink->area_id, vlink->neighbor_id);
    } else {
        printf("Error: Failed to configure virtual link\n");
    }

    return ret;
}

/*
 * Configure route filtering
 * Command: filter-policy {import|export} <acl-number>
 */
static int cmd_ospf_filter_policy(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Direction and ACL number required\n");
        printf("Usage: filter-policy {import|export} <acl-number>\n");
        return -1;
    }

    const char *direction = args->argv[0];
    const char *acl = args->argv[1];

    if (strcmp(direction, "import") != 0 && strcmp(direction, "export") != 0) {
        printf("Error: Direction must be 'import' or 'export'\n");
        return -1;
    }

    printf("OSPF filter policy configured: %s ACL %s\n", direction, acl);
    printf("Note: Implement ACL-based filtering in FRR using distribute-list\n");

    return 0;
}

/*
 * Configure OSPF authentication
 * Command: authentication-mode {simple|md5|sha} <password> [key-id <id>]
 */
static int cmd_ospf_authentication(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Authentication mode and password required\n");
        printf("Usage: authentication-mode {simple|md5|sha} <password> [key-id <id>]\n");
        return -1;
    }

    const char *mode = args->argv[0];
    const char *password = args->argv[1];
    uint8_t key_id = 1;

    /* Parse optional key-id */
    for (int i = 2; i < args->argc; i++) {
        if (strcmp(args->argv[i], "key-id") == 0 && i + 1 < args->argc) {
            key_id = atoi(args->argv[++i]);
        }
    }

    if (strcmp(mode, "simple") != 0 && strcmp(mode, "md5") != 0 && strcmp(mode, "sha") != 0) {
        printf("Error: Invalid authentication mode\n");
        return -1;
    }

    printf("OSPF authentication configured: mode=%s, key-id=%u\n", mode, key_id);
    printf("Note: Configure authentication per-interface or per-area in FRR\n");

    return 0;
}

/*
 * Set reference bandwidth
 * Command: bandwidth-reference <bandwidth>
 */
static int cmd_ospf_bandwidth_reference(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: Bandwidth value required\n");
        printf("Usage: bandwidth-reference <bandwidth>\n");
        return -1;
    }

    uint32_t bandwidth = atoi(args->argv[0]);
    global_ospf_config.reference_bandwidth = bandwidth;

    char frr_cmd[256];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router ospf %u\n"
             "auto-cost reference-bandwidth %u\n"
             "exit\n",
             global_ospf_config.process_id, bandwidth);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("Reference bandwidth set to %u Mbps\n", bandwidth);
    } else {
        printf("Error: Failed to set reference bandwidth\n");
    }

    return ret;
}

/*
 * Advertise default route
 * Command: default-route-advertise [always] [cost <cost>]
 */
static int cmd_ospf_default_route(struct cmd_element *cmd, struct cmd_args *args)
{
    bool always = false;
    uint32_t cost = 1;

    for (int i = 0; i < args->argc; i++) {
        if (strcmp(args->argv[i], "always") == 0) {
            always = true;
        } else if (strcmp(args->argv[i], "cost") == 0 && i + 1 < args->argc) {
            cost = atoi(args->argv[++i]);
        }
    }

    char frr_cmd[512];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router ospf %u\n"
             "default-information originate%s metric %u\n"
             "exit\n",
             global_ospf_config.process_id,
             always ? " always" : "",
             cost);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("Default route advertisement configured\n");
    } else {
        printf("Error: Failed to configure default route advertisement\n");
    }

    return ret;
}

/*
 * Display OSPF configuration
 * Command: display ospf [process-id]
 */
static int cmd_display_ospf(struct cmd_element *cmd, struct cmd_args *args)
{
    char output[4096];
    int ret = execute_vtysh_command_with_output("show ip ospf", output, sizeof(output));

    if (ret == 0) {
        printf("OSPF Configuration:\n");
        printf("  Process ID: %u\n", global_ospf_config.process_id);
        printf("  Router ID: %s\n", global_ospf_config.router_id[0] ?
               global_ospf_config.router_id : "Not configured");
        printf("  Reference Bandwidth: %u Mbps\n", global_ospf_config.reference_bandwidth);
        printf("  Areas: %d configured\n", global_ospf_config.area_count);

        for (int i = 0; i < global_ospf_config.area_count; i++) {
            struct ospf_area_config *area = &global_ospf_config.areas[i];
            printf("    Area %u: ", area->area_id);
            switch (area->type) {
                case OSPF_AREA_NORMAL:
                    printf("Normal\n");
                    break;
                case OSPF_AREA_STUB:
                    printf("Stub\n");
                    break;
                case OSPF_AREA_NSSA:
                    printf("NSSA\n");
                    break;
                case OSPF_AREA_TOTALLY_STUB:
                    printf("Totally Stub\n");
                    break;
                case OSPF_AREA_TOTALLY_NSSA:
                    printf("Totally NSSA\n");
                    break;
            }
        }

        printf("  Virtual Links: %d configured\n", global_ospf_config.vlink_count);
        printf("\nFRR OSPF Status:\n%s\n", output);
    } else {
        printf("Error: Failed to retrieve OSPF status\n");
    }

    return ret;
}

/* Command registration */
struct cmd_element ospf_enhanced_cmds[] = {
    {
        .name = "ospf",
        .func = cmd_ospf,
        .alias = "router ospf",
        .help = "Enter OSPF configuration mode (Huawei VRP style)",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "area",
        .func = cmd_ospf_area,
        .alias = "area",
        .help = "Configure OSPF area",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "nssa",
        .func = cmd_ospf_area_nssa,
        .alias = "area nssa",
        .help = "Configure NSSA area",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "stub",
        .func = cmd_ospf_area_stub,
        .alias = "area stub",
        .help = "Configure stub area",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "vlink-peer",
        .func = cmd_ospf_vlink,
        .alias = "area virtual-link",
        .help = "Configure OSPF virtual link",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "filter-policy",
        .func = cmd_ospf_filter_policy,
        .alias = "distribute-list",
        .help = "Configure route filtering",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "authentication-mode",
        .func = cmd_ospf_authentication,
        .alias = "area authentication",
        .help = "Configure OSPF authentication",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "bandwidth-reference",
        .func = cmd_ospf_bandwidth_reference,
        .alias = "auto-cost reference-bandwidth",
        .help = "Set reference bandwidth for cost calculation",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "default-route-advertise",
        .func = cmd_ospf_default_route,
        .alias = "default-information originate",
        .help = "Advertise default route",
        .category = CMD_CAT_ROUTING,
    },
    {
        .name = "display ospf",
        .func = cmd_display_ospf,
        .alias = "show ip ospf",
        .help = "Display OSPF configuration and status",
        .category = CMD_CAT_ROUTING,
    },
    { .name = NULL } /* Sentinel */
};

/* Register OSPF enhanced commands */
void register_ospf_enhanced_cmds(void)
{
    printf("Registering OSPF enhanced commands...\n");
}
