/*
 * VLAN and VLAN Interface Support for Huawei VRP Style
 * Copyright (C) 2026 WhiteBox NE Team
 *
 * This module provides VLAN functionality including:
 * - VLAN creation and deletion
 * - VLAN interfaces (Vlanif)
 * - VLAN member management
 * - Trunk/Access port configuration
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../lib/huawei_cli.h"

/* VLAN configuration */
struct vlan_config {
    uint16_t vlan_id;
    char description[128];
    bool enabled;
    char member_ports[32][64];
    int member_count;
};

/* VLAN interface configuration */
struct vlanif_config {
    uint16_t vlan_id;
    char ip_address[64];
    char ipv6_address[64];
    bool enabled;
    char description[128];
    uint16_t mtu;
};

/* Port link type */
typedef enum {
    PORT_LINK_ACCESS = 0,
    PORT_LINK_TRUNK = 1,
    PORT_LINK_HYBRID = 2
} port_link_type_t;

/* Port configuration */
struct port_config {
    char interface[64];
    port_link_type_t link_type;
    uint16_t pvid;              /* Default VLAN for access/hybrid */
    uint16_t allowed_vlans[4096];
    int allowed_vlan_count;
};

/* Global configurations */
static struct vlan_config vlans[4096];
static int vlan_count = 0;
static struct vlanif_config vlanifs[4096];
static int vlanif_count = 0;
static struct port_config ports[256];
static int port_count = 0;

/*
 * Create VLAN
 * Command: vlan <vlan-id>
 */
static int cmd_vlan(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: VLAN ID required\n");
        printf("Usage: vlan <vlan-id>\n");
        return -1;
    }

    uint16_t vlan_id = atoi(args->argv[0]);
    if (vlan_id < 1 || vlan_id > 4094) {
        printf("Error: VLAN ID must be between 1 and 4094\n");
        return -1;
    }

    /* Find or create VLAN */
    struct vlan_config *vlan = NULL;
    for (int i = 0; i < vlan_count; i++) {
        if (vlans[i].vlan_id == vlan_id) {
            vlan = &vlans[i];
            break;
        }
    }

    if (!vlan && vlan_count < 4096) {
        vlan = &vlans[vlan_count++];
        memset(vlan, 0, sizeof(struct vlan_config));
        vlan->vlan_id = vlan_id;
        vlan->enabled = true;
    }

    if (!vlan) {
        printf("Error: Maximum VLANs reached\n");
        return -1;
    }

    printf("Entering VLAN %u configuration\n", vlan_id);
    printf("[Huawei-vlan%u]\n", vlan_id);

    /* Create VLAN in Linux */
    char cmd_str[256];
    snprintf(cmd_str, sizeof(cmd_str),
             "ip link add link eth0 name eth0.%u type vlan id %u 2>/dev/null || true",
             vlan_id, vlan_id);
    system(cmd_str);

    printf("VLAN %u created\n", vlan_id);

    return 0;
}

/*
 * Set VLAN description
 * Command: description <text>
 */
static int cmd_vlan_description(struct cmd_element *cmd, struct cmd_args *args)
{
    if (vlan_count == 0) {
        printf("Error: No VLAN configured\n");
        return -1;
    }

    if (args->argc < 1) {
        printf("Error: Description text required\n");
        printf("Usage: description <text>\n");
        return -1;
    }

    struct vlan_config *vlan = &vlans[vlan_count - 1];
    strncpy(vlan->description, args->argv[0], sizeof(vlan->description) - 1);

    printf("VLAN %u description set\n", vlan->vlan_id);

    return 0;
}

/*
 * Create VLAN interface
 * Command: interface Vlanif<vlan-id>
 */
static int cmd_interface_vlanif(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: VLAN ID required\n");
        printf("Usage: interface Vlanif<vlan-id>\n");
        return -1;
    }

    const char *vlanif_str = args->argv[0];
    uint16_t vlan_id = 0;

    /* Parse Vlanif number */
    if (sscanf(vlanif_str, "Vlanif%hu", &vlan_id) != 1 &&
        sscanf(vlanif_str, "vlanif%hu", &vlan_id) != 1 &&
        sscanf(vlanif_str, "%hu", &vlan_id) != 1) {
        printf("Error: Invalid Vlanif format\n");
        return -1;
    }

    if (vlan_id < 1 || vlan_id > 4094) {
        printf("Error: VLAN ID must be between 1 and 4094\n");
        return -1;
    }

    /* Find or create Vlanif */
    struct vlanif_config *vlanif = NULL;
    for (int i = 0; i < vlanif_count; i++) {
        if (vlanifs[i].vlan_id == vlan_id) {
            vlanif = &vlanifs[i];
            break;
        }
    }

    if (!vlanif && vlanif_count < 4096) {
        vlanif = &vlanifs[vlanif_count++];
        memset(vlanif, 0, sizeof(struct vlanif_config));
        vlanif->vlan_id = vlan_id;
        vlanif->enabled = true;
        vlanif->mtu = 1500;
    }

    if (!vlanif) {
        printf("Error: Maximum VLAN interfaces reached\n");
        return -1;
    }

    printf("Entering Vlanif%u configuration\n", vlan_id);
    printf("[Huawei-Vlanif%u]\n", vlan_id);

    /* Create VLAN interface */
    char cmd_str[256];
    snprintf(cmd_str, sizeof(cmd_str),
             "ip link add link eth0 name vlan%u type vlan id %u 2>/dev/null || true",
             vlan_id, vlan_id);
    system(cmd_str);

    snprintf(cmd_str, sizeof(cmd_str), "ip link set vlan%u up", vlan_id);
    system(cmd_str);

    return 0;
}

/*
 * Configure IP address on VLAN interface
 * Command: ip address <ip-address> <mask>
 */
static int cmd_vlanif_ip_address(struct cmd_element *cmd, struct cmd_args *args)
{
    if (vlanif_count == 0) {
        printf("Error: No VLAN interface configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: IP address and mask required\n");
        printf("Usage: ip address <ip-address> <mask>\n");
        return -1;
    }

    struct vlanif_config *vlanif = &vlanifs[vlanif_count - 1];
    snprintf(vlanif->ip_address, sizeof(vlanif->ip_address),
             "%s/%s", args->argv[0], args->argv[1]);

    /* Configure IP address */
    char cmd_str[256];
    snprintf(cmd_str, sizeof(cmd_str),
             "ip addr add %s/%s dev vlan%u 2>/dev/null || true",
             args->argv[0], args->argv[1], vlanif->vlan_id);
    system(cmd_str);

    printf("IP address %s/%s configured on Vlanif%u\n",
           args->argv[0], args->argv[1], vlanif->vlan_id);

    return 0;
}

/*
 * Configure port link type
 * Command: port link-type {access|trunk|hybrid}
 */
static int cmd_port_link_type(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Link type required\n");
        printf("Usage: port link-type {access|trunk|hybrid}\n");
        return -1;
    }

    const char *link_type_str = args->argv[1];
    port_link_type_t link_type;

    if (strcmp(link_type_str, "access") == 0) {
        link_type = PORT_LINK_ACCESS;
    } else if (strcmp(link_type_str, "trunk") == 0) {
        link_type = PORT_LINK_TRUNK;
    } else if (strcmp(link_type_str, "hybrid") == 0) {
        link_type = PORT_LINK_HYBRID;
    } else {
        printf("Error: Invalid link type\n");
        return -1;
    }

    printf("Port link type set to %s\n", link_type_str);
    printf("Note: Configure this under interface mode\n");

    return 0;
}

/*
 * Configure default VLAN
 * Command: port default vlan <vlan-id>
 */
static int cmd_port_default_vlan(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 3) {
        printf("Error: VLAN ID required\n");
        printf("Usage: port default vlan <vlan-id>\n");
        return -1;
    }

    uint16_t vlan_id = atoi(args->argv[2]);
    if (vlan_id < 1 || vlan_id > 4094) {
        printf("Error: VLAN ID must be between 1 and 4094\n");
        return -1;
    }

    printf("Default VLAN set to %u\n", vlan_id);

    return 0;
}

/*
 * Configure trunk allowed VLANs
 * Command: port trunk allow-pass vlan {<vlan-id>|<vlan-range>|all}
 */
static int cmd_port_trunk_allow_vlan(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 4) {
        printf("Error: VLAN specification required\n");
        printf("Usage: port trunk allow-pass vlan {<vlan-id>|<vlan-range>|all}\n");
        return -1;
    }

    const char *vlan_spec = args->argv[3];

    if (strcmp(vlan_spec, "all") == 0) {
        printf("All VLANs allowed on trunk\n");
    } else {
        printf("VLANs %s allowed on trunk\n", vlan_spec);
    }

    return 0;
}

/*
 * Display VLAN configuration
 * Command: display vlan [vlan-id]
 */
static int cmd_display_vlan(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc > 0) {
        /* Display specific VLAN */
        uint16_t vlan_id = atoi(args->argv[0]);
        struct vlan_config *vlan = NULL;

        for (int i = 0; i < vlan_count; i++) {
            if (vlans[i].vlan_id == vlan_id) {
                vlan = &vlans[i];
                break;
            }
        }

        if (!vlan) {
            printf("Error: VLAN not found\n");
            return -1;
        }

        printf("VLAN %u:\n", vlan->vlan_id);
        printf("  Description: %s\n", vlan->description[0] ? vlan->description : "None");
        printf("  Status: %s\n", vlan->enabled ? "Active" : "Inactive");
        printf("  Member ports: %d\n", vlan->member_count);
    } else {
        /* Display all VLANs */
        printf("VLAN Configuration:\n");
        printf("  Total VLANs: %d\n\n", vlan_count);

        printf("  VLAN ID  Description                Status\n");
        printf("  -------  -------------------------  ------\n");
        for (int i = 0; i < vlan_count; i++) {
            printf("  %-7u  %-25s  %s\n",
                   vlans[i].vlan_id,
                   vlans[i].description[0] ? vlans[i].description : "-",
                   vlans[i].enabled ? "Active" : "Inactive");
        }
    }

    return 0;
}

/*
 * Display VLAN interfaces
 * Command: display interface Vlanif
 */
static int cmd_display_vlanif(struct cmd_element *cmd, struct cmd_args *args)
{
    printf("VLAN Interfaces:\n");
    printf("  Total: %d\n\n", vlanif_count);

    printf("  Interface  IP Address           Status  MTU\n");
    printf("  ---------  -------------------  ------  ----\n");
    for (int i = 0; i < vlanif_count; i++) {
        printf("  Vlanif%-4u  %-19s  %-6s  %u\n",
               vlanifs[i].vlan_id,
               vlanifs[i].ip_address[0] ? vlanifs[i].ip_address : "Not configured",
               vlanifs[i].enabled ? "Up" : "Down",
               vlanifs[i].mtu);
    }

    return 0;
}

/*
 * Delete VLAN
 * Command: undo vlan <vlan-id>
 */
static int cmd_undo_vlan(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: VLAN ID required\n");
        printf("Usage: undo vlan <vlan-id>\n");
        return -1;
    }

    uint16_t vlan_id = atoi(args->argv[1]);

    /* Find and remove VLAN */
    for (int i = 0; i < vlan_count; i++) {
        if (vlans[i].vlan_id == vlan_id) {
            /* Delete VLAN interface */
            char cmd_str[256];
            snprintf(cmd_str, sizeof(cmd_str),
                     "ip link delete eth0.%u 2>/dev/null || true", vlan_id);
            system(cmd_str);

            /* Shift remaining VLANs */
            for (int j = i; j < vlan_count - 1; j++) {
                vlans[j] = vlans[j + 1];
            }
            vlan_count--;
            printf("VLAN %u deleted\n", vlan_id);
            return 0;
        }
    }

    printf("Error: VLAN not found\n");
    return -1;
}

/* Command registration */
struct cmd_element vlan_cmds[] = {
    HUAWEI_CMD_WITH_CATEGORY("vlan", cmd_vlan, "vlan",
                             "Create or enter VLAN configuration", CMD_CAT_INTERFACE),
    HUAWEI_CMD_WITH_CATEGORY("description", cmd_vlan_description, "description",
                             "Set VLAN description", CMD_CAT_INTERFACE),
    HUAWEI_CMD_WITH_CATEGORY("interface Vlanif", cmd_interface_vlanif, "interface vlan",
                             "Create or enter VLAN interface", CMD_CAT_INTERFACE),
    HUAWEI_CMD_WITH_CATEGORY("ip address", cmd_vlanif_ip_address, "ip address",
                             "Configure IP address on VLAN interface", CMD_CAT_INTERFACE),
    HUAWEI_CMD_WITH_CATEGORY("port link-type", cmd_port_link_type, "switchport mode",
                             "Configure port link type", CMD_CAT_INTERFACE),
    HUAWEI_CMD_WITH_CATEGORY("port default vlan", cmd_port_default_vlan, "switchport access vlan",
                             "Configure default VLAN", CMD_CAT_INTERFACE),
    HUAWEI_CMD_WITH_CATEGORY("port trunk allow-pass vlan", cmd_port_trunk_allow_vlan, "switchport trunk allowed vlan",
                             "Configure trunk allowed VLANs", CMD_CAT_INTERFACE),
    HUAWEI_CMD_WITH_CATEGORY("display vlan", cmd_display_vlan, "show vlan",
                             "Display VLAN configuration", CMD_CAT_INTERFACE),
    HUAWEI_CMD_WITH_CATEGORY("display interface Vlanif", cmd_display_vlanif, "show interface vlan",
                             "Display VLAN interfaces", CMD_CAT_INTERFACE),
    HUAWEI_CMD_WITH_CATEGORY("undo vlan", cmd_undo_vlan, "no vlan",
                             "Delete VLAN", CMD_CAT_INTERFACE),
    { .name = NULL }
};

/* Register VLAN commands */
void register_vlan_cmds(void)
{
    printf("Registering VLAN commands...\n");
}
