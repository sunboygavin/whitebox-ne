/*
 * Sub-interface and 802.1Q Support for Huawei VRP Style
 * Copyright (C) 2026 WhiteBox NE Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../lib/huawei_cli.h"

struct subif_config {
    char parent_if[64];
    uint16_t subif_id;
    uint16_t vlan_id;
    char ip_address[64];
    bool enabled;
};

static struct subif_config subifs[256];
static int subif_count = 0;

/*
 * Create sub-interface
 * Command: interface GigabitEthernet0/0/1.100
 */
static int cmd_interface_subif(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: Sub-interface name required\n");
        return -1;
    }

    char parent[64];
    uint16_t subif_id;
    if (sscanf(args->argv[0], "%[^.].%hu", parent, &subif_id) != 2) {
        printf("Error: Invalid sub-interface format\n");
        return -1;
    }

    if (subif_count < 256) {
        struct subif_config *subif = &subifs[subif_count++];
        strncpy(subif->parent_if, parent, sizeof(subif->parent_if) - 1);
        subif->subif_id = subif_id;
        subif->enabled = true;
    }

    printf("Entering sub-interface %s.%u configuration\n", parent, subif_id);
    return 0;
}

/*
 * Configure 802.1Q termination
 * Command: dot1q termination vid <vlan-id>
 */
static int cmd_dot1q_termination(struct cmd_element *cmd, struct cmd_args *args)
{
    if (subif_count == 0 || args->argc < 3) {
        printf("Error: VLAN ID required\n");
        return -1;
    }

    uint16_t vlan_id = atoi(args->argv[2]);
    subifs[subif_count - 1].vlan_id = vlan_id;

    printf("802.1Q termination VID %u configured\n", vlan_id);
    return 0;
}

/*
 * Display sub-interfaces
 * Command: display interface sub-interface
 */
static int cmd_display_subif(struct cmd_element *cmd, struct cmd_args *args)
{
    printf("Sub-interfaces: %d\n", subif_count);
    for (int i = 0; i < subif_count; i++) {
        printf("  %s.%u - VLAN %u - %s\n",
               subifs[i].parent_if, subifs[i].subif_id,
               subifs[i].vlan_id,
               subifs[i].enabled ? "Up" : "Down");
    }
    return 0;
}

struct cmd_element subif_cmds[] = {
    HUAWEI_CMD_WITH_CATEGORY("interface subif", cmd_interface_subif, "interface",
                             "Create sub-interface", CMD_CAT_INTERFACE),
    HUAWEI_CMD_WITH_CATEGORY("dot1q termination vid", cmd_dot1q_termination, "encapsulation dot1q",
                             "Configure 802.1Q termination", CMD_CAT_INTERFACE),
    HUAWEI_CMD_WITH_CATEGORY("display interface sub-interface", cmd_display_subif, "show interface",
                             "Display sub-interfaces", CMD_CAT_INTERFACE),
    { .name = NULL }
};

void register_subif_cmds(void) {
    printf("Registering sub-interface commands...\n");
}
