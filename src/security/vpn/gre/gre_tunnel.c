/*
 * GRE Tunnel Support for Huawei VRP Style
 * Copyright (C) 2026 WhiteBox NE Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../../frr_core/lib/huawei_cli.h"

struct gre_tunnel {
    char name[64];
    char source[64];
    char destination[64];
    char local_address[64];
    uint16_t tunnel_id;
    bool keepalive_enabled;
    uint16_t keepalive_interval;
    uint16_t keepalive_retries;
    bool enabled;
};

static struct gre_tunnel tunnels[64];
static int tunnel_count = 0;
static struct gre_tunnel *current_tunnel = NULL;

/*
 * Create tunnel interface
 * Command: interface Tunnel<id>
 */
static int cmd_interface_tunnel(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: Tunnel ID required\n");
        return -1;
    }

    uint16_t tunnel_id;
    if (sscanf(args->argv[0], "Tunnel%hu", &tunnel_id) != 1 &&
        sscanf(args->argv[0], "tunnel%hu", &tunnel_id) != 1 &&
        sscanf(args->argv[0], "%hu", &tunnel_id) != 1) {
        printf("Error: Invalid tunnel format\n");
        return -1;
    }

    current_tunnel = NULL;
    for (int i = 0; i < tunnel_count; i++) {
        if (tunnels[i].tunnel_id == tunnel_id) {
            current_tunnel = &tunnels[i];
            break;
        }
    }

    if (!current_tunnel && tunnel_count < 64) {
        current_tunnel = &tunnels[tunnel_count++];
        memset(current_tunnel, 0, sizeof(struct gre_tunnel));
        current_tunnel->tunnel_id = tunnel_id;
        snprintf(current_tunnel->name, sizeof(current_tunnel->name), "Tunnel%u", tunnel_id);
        current_tunnel->enabled = true;
        current_tunnel->keepalive_interval = 10;
        current_tunnel->keepalive_retries = 3;
    }

    printf("Entering Tunnel%u configuration\n", tunnel_id);
    return 0;
}

/*
 * Set tunnel protocol
 * Command: tunnel-protocol gre
 */
static int cmd_tunnel_protocol_gre(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_tunnel) {
        printf("Error: No tunnel configured\n");
        return -1;
    }

    printf("Tunnel protocol set to GRE\n");
    return 0;
}

/*
 * Set tunnel source
 * Command: source {<ip-address>|<interface>}
 */
static int cmd_tunnel_source(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_tunnel || args->argc < 1) {
        printf("Error: Source required\n");
        return -1;
    }

    strncpy(current_tunnel->source, args->argv[0], sizeof(current_tunnel->source) - 1);
    printf("Tunnel source set to %s\n", args->argv[0]);
    return 0;
}

/*
 * Set tunnel destination
 * Command: destination <ip-address>
 */
static int cmd_tunnel_destination(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_tunnel || args->argc < 1) {
        printf("Error: Destination required\n");
        return -1;
    }

    strncpy(current_tunnel->destination, args->argv[0], sizeof(current_tunnel->destination) - 1);
    printf("Tunnel destination set to %s\n", args->argv[0]);
    return 0;
}

/*
 * Enable keepalive
 * Command: keepalive [<interval> [<retries>]]
 */
static int cmd_tunnel_keepalive(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_tunnel) {
        printf("Error: No tunnel configured\n");
        return -1;
    }

    current_tunnel->keepalive_enabled = true;
    if (args->argc > 0) {
        current_tunnel->keepalive_interval = atoi(args->argv[0]);
    }
    if (args->argc > 1) {
        current_tunnel->keepalive_retries = atoi(args->argv[1]);
    }

    printf("Keepalive enabled: interval %u, retries %u\n",
           current_tunnel->keepalive_interval, current_tunnel->keepalive_retries);
    return 0;
}

/*
 * Display tunnels
 * Command: display interface tunnel
 */
static int cmd_display_tunnel(struct cmd_element *cmd, struct cmd_args *args)
{
    printf("GRE Tunnels: %d\n", tunnel_count);
    for (int i = 0; i < tunnel_count; i++) {
        printf("  %s: %s -> %s (%s)\n",
               tunnels[i].name, tunnels[i].source, tunnels[i].destination,
               tunnels[i].enabled ? "Up" : "Down");
    }
    return 0;
}

struct cmd_element gre_cmds[] = {
    HUAWEI_CMD_WITH_CATEGORY("interface Tunnel", cmd_interface_tunnel, "interface tunnel",
                             "Create tunnel interface", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("tunnel-protocol gre", cmd_tunnel_protocol_gre, "tunnel mode gre",
                             "Set tunnel protocol to GRE", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("source", cmd_tunnel_source, "tunnel source",
                             "Set tunnel source", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("destination", cmd_tunnel_destination, "tunnel destination",
                             "Set tunnel destination", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("keepalive", cmd_tunnel_keepalive, "keepalive",
                             "Enable tunnel keepalive", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("display interface tunnel", cmd_display_tunnel, "show interface tunnel",
                             "Display tunnel interfaces", CMD_CAT_SECURITY),
    { .name = NULL }
};

void register_gre_cmds(void) {
    printf("Registering GRE tunnel commands...\n");
}
