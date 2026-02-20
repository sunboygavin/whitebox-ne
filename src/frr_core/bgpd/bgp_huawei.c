/*
 * BGP Enhanced Features for Huawei VRP Style
 * Copyright (C) 2026 WhiteBox NE Team
 *
 * This module provides enhanced BGP features including:
 * - Route policies
 * - Community attributes
 * - Route reflectors
 * - Confederations
 * - Route aggregation
 * - AS-path filtering
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../lib/huawei_cli.h"

/* BGP peer types */
typedef enum {
    BGP_PEER_IBGP = 0,
    BGP_PEER_EBGP = 1
} bgp_peer_type_t;

/* BGP address family */
typedef enum {
    BGP_AFI_IPV4_UNICAST = 0,
    BGP_AFI_IPV6_UNICAST = 1,
    BGP_AFI_IPV4_MULTICAST = 2,
    BGP_AFI_VPNv4 = 3,
    BGP_AFI_VPNv6 = 4
} bgp_afi_t;

/* BGP peer configuration */
struct bgp_peer_config {
    char peer_address[64];
    uint32_t remote_as;
    char description[128];
    bool enabled;
    char password[64];
    uint16_t connect_retry_time;
    uint16_t keepalive_time;
    uint16_t hold_time;
    bool route_reflector_client;
    char route_policy_import[64];
    char route_policy_export[64];
    char filter_policy_import[64];
    char filter_policy_export[64];
};

/* BGP global configuration */
struct bgp_global_config {
    uint32_t as_number;
    char router_id[16];
    bool enabled;
    uint32_t confederation_id;
    uint32_t confederation_peers[16];
    int confederation_peer_count;
    struct bgp_peer_config peers[64];
    int peer_count;
    char aggregate_addresses[32][64];
    int aggregate_count;
};

static struct bgp_global_config global_bgp_config = {0};

/*
 * Enter BGP configuration mode
 * Command: bgp <as-number>
 */
static int cmd_bgp(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: AS number required\n");
        printf("Usage: bgp <as-number>\n");
        return -1;
    }

    uint32_t as_number = atoi(args->argv[0]);
    global_bgp_config.as_number = as_number;
    global_bgp_config.enabled = true;

    printf("Entering BGP %u configuration mode\n", as_number);
    printf("[Huawei-bgp]\n");

    char frr_cmd[256];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router bgp %u\n"
             "exit\n",
             as_number);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret != 0) {
        printf("Error: Failed to configure BGP\n");
    }

    return ret;
}

/*
 * Set BGP router ID
 * Command: router-id <router-id>
 */
static int cmd_bgp_router_id(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: Router ID required\n");
        printf("Usage: router-id <router-id>\n");
        return -1;
    }

    const char *router_id = args->argv[0];
    strncpy(global_bgp_config.router_id, router_id, sizeof(global_bgp_config.router_id) - 1);

    char frr_cmd[256];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router bgp %u\n"
             "bgp router-id %s\n"
             "exit\n",
             global_bgp_config.as_number, router_id);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("BGP router ID set to %s\n", router_id);
    } else {
        printf("Error: Failed to set router ID\n");
    }

    return ret;
}

/*
 * Configure BGP peer
 * Command: peer <peer-address> as-number <as-number>
 */
static int cmd_bgp_peer(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 3) {
        printf("Error: Peer address and AS number required\n");
        printf("Usage: peer <peer-address> as-number <as-number>\n");
        return -1;
    }

    if (global_bgp_config.peer_count >= 64) {
        printf("Error: Maximum peers reached\n");
        return -1;
    }

    const char *peer_addr = args->argv[0];

    if (strcmp(args->argv[1], "as-number") != 0) {
        printf("Error: Expected 'as-number' keyword\n");
        return -1;
    }

    uint32_t remote_as = atoi(args->argv[2]);

    struct bgp_peer_config *peer = &global_bgp_config.peers[global_bgp_config.peer_count++];
    strncpy(peer->peer_address, peer_addr, sizeof(peer->peer_address) - 1);
    peer->remote_as = remote_as;
    peer->enabled = true;
    peer->keepalive_time = 60;
    peer->hold_time = 180;

    char frr_cmd[512];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router bgp %u\n"
             "neighbor %s remote-as %u\n"
             "exit\n",
             global_bgp_config.as_number, peer_addr, remote_as);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("BGP peer %s configured (AS %u)\n", peer_addr, remote_as);
    } else {
        printf("Error: Failed to configure BGP peer\n");
    }

    return ret;
}

/*
 * Configure peer description
 * Command: peer <peer-address> description <text>
 */
static int cmd_bgp_peer_description(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 3) {
        printf("Error: Peer address and description required\n");
        printf("Usage: peer <peer-address> description <text>\n");
        return -1;
    }

    const char *peer_addr = args->argv[0];
    const char *description = args->argv[2];

    /* Find peer */
    struct bgp_peer_config *peer = NULL;
    for (int i = 0; i < global_bgp_config.peer_count; i++) {
        if (strcmp(global_bgp_config.peers[i].peer_address, peer_addr) == 0) {
            peer = &global_bgp_config.peers[i];
            break;
        }
    }

    if (!peer) {
        printf("Error: Peer not found\n");
        return -1;
    }

    strncpy(peer->description, description, sizeof(peer->description) - 1);

    char frr_cmd[512];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router bgp %u\n"
             "neighbor %s description %s\n"
             "exit\n",
             global_bgp_config.as_number, peer_addr, description);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("Peer description set\n");
    }

    return ret;
}

/*
 * Configure peer password
 * Command: peer <peer-address> password cipher <password>
 */
static int cmd_bgp_peer_password(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 4) {
        printf("Error: Peer address and password required\n");
        printf("Usage: peer <peer-address> password cipher <password>\n");
        return -1;
    }

    const char *peer_addr = args->argv[0];
    const char *password = args->argv[3];

    char frr_cmd[512];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router bgp %u\n"
             "neighbor %s password %s\n"
             "exit\n",
             global_bgp_config.as_number, peer_addr, password);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("Peer password configured\n");
    } else {
        printf("Error: Failed to configure peer password\n");
    }

    return ret;
}

/*
 * Configure route reflector client
 * Command: peer <peer-address> reflect-client
 */
static int cmd_bgp_peer_rr_client(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Peer address required\n");
        printf("Usage: peer <peer-address> reflect-client\n");
        return -1;
    }

    const char *peer_addr = args->argv[0];

    /* Find peer */
    struct bgp_peer_config *peer = NULL;
    for (int i = 0; i < global_bgp_config.peer_count; i++) {
        if (strcmp(global_bgp_config.peers[i].peer_address, peer_addr) == 0) {
            peer = &global_bgp_config.peers[i];
            break;
        }
    }

    if (!peer) {
        printf("Error: Peer not found\n");
        return -1;
    }

    peer->route_reflector_client = true;

    char frr_cmd[512];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router bgp %u\n"
             "neighbor %s route-reflector-client\n"
             "exit\n",
             global_bgp_config.as_number, peer_addr);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("Peer %s configured as route reflector client\n", peer_addr);
    } else {
        printf("Error: Failed to configure route reflector client\n");
    }

    return ret;
}

/*
 * Configure route policy
 * Command: peer <peer-address> route-policy <policy-name> {import|export}
 */
static int cmd_bgp_peer_route_policy(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 4) {
        printf("Error: Peer address, policy name, and direction required\n");
        printf("Usage: peer <peer-address> route-policy <policy-name> {import|export}\n");
        return -1;
    }

    const char *peer_addr = args->argv[0];
    const char *policy_name = args->argv[2];
    const char *direction = args->argv[3];

    if (strcmp(direction, "import") != 0 && strcmp(direction, "export") != 0) {
        printf("Error: Direction must be 'import' or 'export'\n");
        return -1;
    }

    /* Find peer */
    struct bgp_peer_config *peer = NULL;
    for (int i = 0; i < global_bgp_config.peer_count; i++) {
        if (strcmp(global_bgp_config.peers[i].peer_address, peer_addr) == 0) {
            peer = &global_bgp_config.peers[i];
            break;
        }
    }

    if (!peer) {
        printf("Error: Peer not found\n");
        return -1;
    }

    if (strcmp(direction, "import") == 0) {
        strncpy(peer->route_policy_import, policy_name, sizeof(peer->route_policy_import) - 1);
    } else {
        strncpy(peer->route_policy_export, policy_name, sizeof(peer->route_policy_export) - 1);
    }

    char frr_cmd[512];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router bgp %u\n"
             "neighbor %s route-map %s %s\n"
             "exit\n",
             global_bgp_config.as_number, peer_addr, policy_name,
             strcmp(direction, "import") == 0 ? "in" : "out");

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("Route policy %s applied to peer %s (%s)\n", policy_name, peer_addr, direction);
    } else {
        printf("Error: Failed to apply route policy\n");
    }

    return ret;
}

/*
 * Configure BGP confederation
 * Command: confederation id <confederation-id>
 */
static int cmd_bgp_confederation_id(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Confederation ID required\n");
        printf("Usage: confederation id <confederation-id>\n");
        return -1;
    }

    uint32_t conf_id = atoi(args->argv[1]);
    global_bgp_config.confederation_id = conf_id;

    char frr_cmd[256];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router bgp %u\n"
             "bgp confederation identifier %u\n"
             "exit\n",
             global_bgp_config.as_number, conf_id);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("BGP confederation ID set to %u\n", conf_id);
    } else {
        printf("Error: Failed to set confederation ID\n");
    }

    return ret;
}

/*
 * Configure confederation peers
 * Command: confederation peer-as <as-number> [<as-number> ...]
 */
static int cmd_bgp_confederation_peers(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: At least one peer AS required\n");
        printf("Usage: confederation peer-as <as-number> [<as-number> ...]\n");
        return -1;
    }

    global_bgp_config.confederation_peer_count = 0;

    char as_list[256] = "";
    for (int i = 1; i < args->argc && i < 17; i++) {
        uint32_t peer_as = atoi(args->argv[i]);
        global_bgp_config.confederation_peers[global_bgp_config.confederation_peer_count++] = peer_as;

        char as_str[16];
        snprintf(as_str, sizeof(as_str), "%u ", peer_as);
        strcat(as_list, as_str);
    }

    char frr_cmd[512];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router bgp %u\n"
             "bgp confederation peers %s\n"
             "exit\n",
             global_bgp_config.as_number, as_list);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("BGP confederation peers configured: %s\n", as_list);
    } else {
        printf("Error: Failed to configure confederation peers\n");
    }

    return ret;
}

/*
 * Configure route aggregation
 * Command: aggregate <address> <mask> [detail-suppressed] [as-set]
 */
static int cmd_bgp_aggregate(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Address and mask required\n");
        printf("Usage: aggregate <address> <mask> [detail-suppressed] [as-set]\n");
        return -1;
    }

    const char *address = args->argv[0];
    const char *mask = args->argv[1];
    bool detail_suppressed = false;
    bool as_set = false;

    for (int i = 2; i < args->argc; i++) {
        if (strcmp(args->argv[i], "detail-suppressed") == 0) {
            detail_suppressed = true;
        } else if (strcmp(args->argv[i], "as-set") == 0) {
            as_set = true;
        }
    }

    if (global_bgp_config.aggregate_count < 32) {
        snprintf(global_bgp_config.aggregate_addresses[global_bgp_config.aggregate_count++],
                 64, "%s/%s", address, mask);
    }

    char frr_cmd[512];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router bgp %u\n"
             "aggregate-address %s/%s%s%s\n"
             "exit\n",
             global_bgp_config.as_number, address, mask,
             detail_suppressed ? " summary-only" : "",
             as_set ? " as-set" : "");

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("BGP aggregate address configured: %s/%s\n", address, mask);
    } else {
        printf("Error: Failed to configure aggregate address\n");
    }

    return ret;
}

/*
 * Configure community attribute
 * Command: peer <peer-address> advertise-community
 */
static int cmd_bgp_peer_community(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Peer address required\n");
        printf("Usage: peer <peer-address> advertise-community\n");
        return -1;
    }

    const char *peer_addr = args->argv[0];

    char frr_cmd[512];
    snprintf(frr_cmd, sizeof(frr_cmd),
             "configure terminal\n"
             "router bgp %u\n"
             "neighbor %s send-community\n"
             "exit\n",
             global_bgp_config.as_number, peer_addr);

    int ret = execute_vtysh_command(frr_cmd);
    if (ret == 0) {
        printf("Community advertisement enabled for peer %s\n", peer_addr);
    } else {
        printf("Error: Failed to enable community advertisement\n");
    }

    return ret;
}

/*
 * Display BGP configuration
 * Command: display bgp peer
 */
static int cmd_display_bgp_peer(struct cmd_element *cmd, struct cmd_args *args)
{
    char output[4096];
    int ret = execute_vtysh_command_with_output("show ip bgp summary", output, sizeof(output));

    if (ret == 0) {
        printf("BGP Configuration:\n");
        printf("  AS Number: %u\n", global_bgp_config.as_number);
        printf("  Router ID: %s\n", global_bgp_config.router_id[0] ?
               global_bgp_config.router_id : "Not configured");

        if (global_bgp_config.confederation_id > 0) {
            printf("  Confederation ID: %u\n", global_bgp_config.confederation_id);
            printf("  Confederation Peers: ");
            for (int i = 0; i < global_bgp_config.confederation_peer_count; i++) {
                printf("%u ", global_bgp_config.confederation_peers[i]);
            }
            printf("\n");
        }

        printf("  Peers: %d configured\n", global_bgp_config.peer_count);
        for (int i = 0; i < global_bgp_config.peer_count; i++) {
            struct bgp_peer_config *peer = &global_bgp_config.peers[i];
            printf("    %s (AS %u)%s\n", peer->peer_address, peer->remote_as,
                   peer->route_reflector_client ? " [RR Client]" : "");
            if (peer->description[0]) {
                printf("      Description: %s\n", peer->description);
            }
            if (peer->route_policy_import[0]) {
                printf("      Import Policy: %s\n", peer->route_policy_import);
            }
            if (peer->route_policy_export[0]) {
                printf("      Export Policy: %s\n", peer->route_policy_export);
            }
        }

        printf("\nFRR BGP Status:\n%s\n", output);
    } else {
        printf("Error: Failed to retrieve BGP status\n");
    }

    return ret;
}

/*
 * Display BGP routing table
 * Command: display bgp routing-table
 */
static int cmd_display_bgp_routing_table(struct cmd_element *cmd, struct cmd_args *args)
{
    char output[4096];
    int ret = execute_vtysh_command_with_output("show ip bgp", output, sizeof(output));

    if (ret == 0) {
        printf("BGP Routing Table:\n");
        printf("%s\n", output);
    } else {
        printf("Error: Failed to retrieve BGP routing table\n");
    }

    return ret;
}

/* Command registration */
struct cmd_element bgp_enhanced_cmds[] = {
    HUAWEI_CMD_WITH_CATEGORY("bgp", cmd_bgp, "router bgp",
                             "Enter BGP configuration mode", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("router-id", cmd_bgp_router_id, "bgp router-id",
                             "Set BGP router ID", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("peer", cmd_bgp_peer, "neighbor",
                             "Configure BGP peer", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("peer description", cmd_bgp_peer_description, "neighbor description",
                             "Set peer description", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("peer password", cmd_bgp_peer_password, "neighbor password",
                             "Set peer password", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("peer reflect-client", cmd_bgp_peer_rr_client, "neighbor route-reflector-client",
                             "Configure route reflector client", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("peer route-policy", cmd_bgp_peer_route_policy, "neighbor route-map",
                             "Apply route policy to peer", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("confederation id", cmd_bgp_confederation_id, "bgp confederation identifier",
                             "Set confederation ID", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("confederation peer-as", cmd_bgp_confederation_peers, "bgp confederation peers",
                             "Configure confederation peers", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("aggregate", cmd_bgp_aggregate, "aggregate-address",
                             "Configure route aggregation", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("peer advertise-community", cmd_bgp_peer_community, "neighbor send-community",
                             "Enable community advertisement", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("display bgp peer", cmd_display_bgp_peer, "show ip bgp summary",
                             "Display BGP peer information", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("display bgp routing-table", cmd_display_bgp_routing_table, "show ip bgp",
                             "Display BGP routing table", CMD_CAT_ROUTING),
    { .name = NULL }
};

/* Register BGP enhanced commands */
void register_bgp_enhanced_cmds(void)
{
    printf("Registering BGP enhanced commands...\n");
}
