/*
 * Policy-Based Routing for Huawei VRP Style
 * Copyright (C) 2026 WhiteBox NE Team
 *
 * This module provides policy-based routing (PBR) with:
 * - Source-based routing
 * - Destination-based routing
 * - Interface-based routing
 * - ACL-based matching
 * - Next-hop manipulation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../lib/huawei_cli.h"

/* Policy action types */
typedef enum {
    POLICY_ACTION_PERMIT = 0,
    POLICY_ACTION_DENY = 1
} policy_action_t;

/* Match types */
typedef enum {
    MATCH_TYPE_ACL = 0,
    MATCH_TYPE_SOURCE = 1,
    MATCH_TYPE_DESTINATION = 2,
    MATCH_TYPE_INTERFACE = 3,
    MATCH_TYPE_LENGTH = 4
} match_type_t;

/* Apply actions */
typedef enum {
    APPLY_NEXTHOP = 0,
    APPLY_INTERFACE = 1,
    APPLY_DEFAULT_NEXTHOP = 2,
    APPLY_PRECEDENCE = 3,
    APPLY_DSCP = 4
} apply_action_t;

/* Policy node structure */
struct policy_node {
    uint32_t node_id;
    policy_action_t action;

    /* Match conditions */
    bool match_acl;
    uint32_t acl_number;
    bool match_source;
    char source_prefix[64];
    bool match_destination;
    char dest_prefix[64];
    bool match_interface;
    char interface[64];
    bool match_length;
    uint16_t length_min;
    uint16_t length_max;

    /* Apply actions */
    bool apply_nexthop;
    char nexthop[64];
    bool apply_interface;
    char apply_if[64];
    bool apply_default_nexthop;
    char default_nexthop[64];
    bool apply_precedence;
    uint8_t precedence;
    bool apply_dscp;
    uint8_t dscp;
};

/* Policy configuration */
struct policy_config {
    char name[64];
    struct policy_node nodes[32];
    int node_count;
};

/* Global policy configurations */
static struct policy_config policies[16];
static int policy_count = 0;
static struct policy_config *current_policy = NULL;
static struct policy_node *current_node = NULL;

/*
 * Create or enter policy
 * Command: policy-based-route <policy-name> [permit|deny] node <node-id>
 */
static int cmd_policy_based_route(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: Policy name required\n");
        printf("Usage: policy-based-route <policy-name> [permit|deny] node <node-id>\n");
        return -1;
    }

    const char *policy_name = args->argv[0];
    policy_action_t action = POLICY_ACTION_PERMIT;
    uint32_t node_id = 10;

    /* Find or create policy */
    current_policy = NULL;
    for (int i = 0; i < policy_count; i++) {
        if (strcmp(policies[i].name, policy_name) == 0) {
            current_policy = &policies[i];
            break;
        }
    }

    if (!current_policy && policy_count < 16) {
        current_policy = &policies[policy_count++];
        strncpy(current_policy->name, policy_name, sizeof(current_policy->name) - 1);
        current_policy->node_count = 0;
    }

    if (!current_policy) {
        printf("Error: Maximum policies reached\n");
        return -1;
    }

    /* Parse action and node ID */
    for (int i = 1; i < args->argc; i++) {
        if (strcmp(args->argv[i], "permit") == 0) {
            action = POLICY_ACTION_PERMIT;
        } else if (strcmp(args->argv[i], "deny") == 0) {
            action = POLICY_ACTION_DENY;
        } else if (strcmp(args->argv[i], "node") == 0 && i + 1 < args->argc) {
            node_id = atoi(args->argv[++i]);
        }
    }

    /* Find or create node */
    current_node = NULL;
    for (int i = 0; i < current_policy->node_count; i++) {
        if (current_policy->nodes[i].node_id == node_id) {
            current_node = &current_policy->nodes[i];
            break;
        }
    }

    if (!current_node && current_policy->node_count < 32) {
        current_node = &current_policy->nodes[current_policy->node_count++];
        memset(current_node, 0, sizeof(struct policy_node));
        current_node->node_id = node_id;
        current_node->action = action;
    }

    if (!current_node) {
        printf("Error: Maximum nodes reached for policy\n");
        return -1;
    }

    printf("Entering policy %s node %u (%s)\n", policy_name, node_id,
           action == POLICY_ACTION_PERMIT ? "permit" : "deny");
    printf("[Huawei-policy-%s-%u]\n", policy_name, node_id);

    return 0;
}

/*
 * Match ACL
 * Command: if-match acl <acl-number>
 */
static int cmd_policy_if_match_acl(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_node) {
        printf("Error: No policy node configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: ACL number required\n");
        printf("Usage: if-match acl <acl-number>\n");
        return -1;
    }

    uint32_t acl_number = atoi(args->argv[1]);
    current_node->match_acl = true;
    current_node->acl_number = acl_number;

    printf("Match condition: ACL %u\n", acl_number);

    return 0;
}

/*
 * Match source address
 * Command: if-match ip-address source <prefix>
 */
static int cmd_policy_if_match_source(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_node) {
        printf("Error: No policy node configured\n");
        return -1;
    }

    if (args->argc < 3) {
        printf("Error: Source prefix required\n");
        printf("Usage: if-match ip-address source <prefix>\n");
        return -1;
    }

    const char *prefix = args->argv[2];
    current_node->match_source = true;
    strncpy(current_node->source_prefix, prefix, sizeof(current_node->source_prefix) - 1);

    printf("Match condition: Source %s\n", prefix);

    return 0;
}

/*
 * Match destination address
 * Command: if-match ip-address destination <prefix>
 */
static int cmd_policy_if_match_destination(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_node) {
        printf("Error: No policy node configured\n");
        return -1;
    }

    if (args->argc < 3) {
        printf("Error: Destination prefix required\n");
        printf("Usage: if-match ip-address destination <prefix>\n");
        return -1;
    }

    const char *prefix = args->argv[2];
    current_node->match_destination = true;
    strncpy(current_node->dest_prefix, prefix, sizeof(current_node->dest_prefix) - 1);

    printf("Match condition: Destination %s\n", prefix);

    return 0;
}

/*
 * Match interface
 * Command: if-match interface <interface-name>
 */
static int cmd_policy_if_match_interface(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_node) {
        printf("Error: No policy node configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: Interface name required\n");
        printf("Usage: if-match interface <interface-name>\n");
        return -1;
    }

    const char *interface = args->argv[1];
    current_node->match_interface = true;
    strncpy(current_node->interface, interface, sizeof(current_node->interface) - 1);

    printf("Match condition: Interface %s\n", interface);

    return 0;
}

/*
 * Match packet length
 * Command: if-match packet-length <min> <max>
 */
static int cmd_policy_if_match_length(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_node) {
        printf("Error: No policy node configured\n");
        return -1;
    }

    if (args->argc < 3) {
        printf("Error: Min and max length required\n");
        printf("Usage: if-match packet-length <min> <max>\n");
        return -1;
    }

    uint16_t min_len = atoi(args->argv[1]);
    uint16_t max_len = atoi(args->argv[2]);

    current_node->match_length = true;
    current_node->length_min = min_len;
    current_node->length_max = max_len;

    printf("Match condition: Packet length %u-%u\n", min_len, max_len);

    return 0;
}

/*
 * Apply next-hop
 * Command: apply ip-address next-hop <ip-address>
 */
static int cmd_policy_apply_nexthop(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_node) {
        printf("Error: No policy node configured\n");
        return -1;
    }

    if (args->argc < 3) {
        printf("Error: Next-hop address required\n");
        printf("Usage: apply ip-address next-hop <ip-address>\n");
        return -1;
    }

    const char *nexthop = args->argv[2];
    current_node->apply_nexthop = true;
    strncpy(current_node->nexthop, nexthop, sizeof(current_node->nexthop) - 1);

    printf("Apply action: Next-hop %s\n", nexthop);

    return 0;
}

/*
 * Apply output interface
 * Command: apply output-interface <interface-name>
 */
static int cmd_policy_apply_interface(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_node) {
        printf("Error: No policy node configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: Interface name required\n");
        printf("Usage: apply output-interface <interface-name>\n");
        return -1;
    }

    const char *interface = args->argv[1];
    current_node->apply_interface = true;
    strncpy(current_node->apply_if, interface, sizeof(current_node->apply_if) - 1);

    printf("Apply action: Output interface %s\n", interface);

    return 0;
}

/*
 * Apply default next-hop
 * Command: apply ip-address default next-hop <ip-address>
 */
static int cmd_policy_apply_default_nexthop(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_node) {
        printf("Error: No policy node configured\n");
        return -1;
    }

    if (args->argc < 4) {
        printf("Error: Default next-hop address required\n");
        printf("Usage: apply ip-address default next-hop <ip-address>\n");
        return -1;
    }

    const char *nexthop = args->argv[3];
    current_node->apply_default_nexthop = true;
    strncpy(current_node->default_nexthop, nexthop, sizeof(current_node->default_nexthop) - 1);

    printf("Apply action: Default next-hop %s\n", nexthop);

    return 0;
}

/*
 * Apply IP precedence
 * Command: apply ip-precedence <precedence>
 */
static int cmd_policy_apply_precedence(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_node) {
        printf("Error: No policy node configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: Precedence value required\n");
        printf("Usage: apply ip-precedence <precedence>\n");
        return -1;
    }

    uint8_t precedence = atoi(args->argv[1]);
    if (precedence > 7) {
        printf("Error: Precedence must be 0-7\n");
        return -1;
    }

    current_node->apply_precedence = true;
    current_node->precedence = precedence;

    printf("Apply action: IP precedence %u\n", precedence);

    return 0;
}

/*
 * Apply DSCP
 * Command: apply dscp <dscp-value>
 */
static int cmd_policy_apply_dscp(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_node) {
        printf("Error: No policy node configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: DSCP value required\n");
        printf("Usage: apply dscp <dscp-value>\n");
        return -1;
    }

    uint8_t dscp = atoi(args->argv[1]);
    if (dscp > 63) {
        printf("Error: DSCP must be 0-63\n");
        return -1;
    }

    current_node->apply_dscp = true;
    current_node->dscp = dscp;

    printf("Apply action: DSCP %u\n", dscp);

    return 0;
}

/*
 * Apply policy to interface
 * Command: ip policy-based-route <policy-name>
 * (Used in interface configuration mode)
 */
static int cmd_interface_apply_policy(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Policy name required\n");
        printf("Usage: ip policy-based-route <policy-name>\n");
        return -1;
    }

    const char *policy_name = args->argv[1];

    /* Find policy */
    struct policy_config *policy = NULL;
    for (int i = 0; i < policy_count; i++) {
        if (strcmp(policies[i].name, policy_name) == 0) {
            policy = &policies[i];
            break;
        }
    }

    if (!policy) {
        printf("Error: Policy not found\n");
        return -1;
    }

    printf("Policy %s applied to interface\n", policy_name);
    printf("Note: Configure this under interface mode in FRR using route-map\n");

    return 0;
}

/*
 * Display policy configuration
 * Command: display policy-based-route [policy-name]
 */
static int cmd_display_policy(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc > 0) {
        /* Display specific policy */
        const char *policy_name = args->argv[0];
        struct policy_config *policy = NULL;

        for (int i = 0; i < policy_count; i++) {
            if (strcmp(policies[i].name, policy_name) == 0) {
                policy = &policies[i];
                break;
            }
        }

        if (!policy) {
            printf("Error: Policy not found\n");
            return -1;
        }

        printf("Policy: %s\n", policy->name);
        printf("  Nodes: %d\n", policy->node_count);

        for (int i = 0; i < policy->node_count; i++) {
            struct policy_node *node = &policy->nodes[i];
            printf("\n  Node %u (%s):\n", node->node_id,
                   node->action == POLICY_ACTION_PERMIT ? "permit" : "deny");

            printf("    Match conditions:\n");
            if (node->match_acl) {
                printf("      ACL: %u\n", node->acl_number);
            }
            if (node->match_source) {
                printf("      Source: %s\n", node->source_prefix);
            }
            if (node->match_destination) {
                printf("      Destination: %s\n", node->dest_prefix);
            }
            if (node->match_interface) {
                printf("      Interface: %s\n", node->interface);
            }
            if (node->match_length) {
                printf("      Packet length: %u-%u\n", node->length_min, node->length_max);
            }

            printf("    Apply actions:\n");
            if (node->apply_nexthop) {
                printf("      Next-hop: %s\n", node->nexthop);
            }
            if (node->apply_interface) {
                printf("      Output interface: %s\n", node->apply_if);
            }
            if (node->apply_default_nexthop) {
                printf("      Default next-hop: %s\n", node->default_nexthop);
            }
            if (node->apply_precedence) {
                printf("      IP precedence: %u\n", node->precedence);
            }
            if (node->apply_dscp) {
                printf("      DSCP: %u\n", node->dscp);
            }
        }
    } else {
        /* Display all policies */
        printf("Policy-Based Routing Configuration:\n");
        printf("  Total policies: %d\n\n", policy_count);

        for (int i = 0; i < policy_count; i++) {
            printf("  Policy: %s (%d nodes)\n", policies[i].name, policies[i].node_count);
        }
    }

    return 0;
}

/*
 * Delete policy
 * Command: undo policy-based-route <policy-name>
 */
static int cmd_undo_policy(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Policy name required\n");
        printf("Usage: undo policy-based-route <policy-name>\n");
        return -1;
    }

    const char *policy_name = args->argv[1];

    /* Find and remove policy */
    for (int i = 0; i < policy_count; i++) {
        if (strcmp(policies[i].name, policy_name) == 0) {
            /* Shift remaining policies */
            for (int j = i; j < policy_count - 1; j++) {
                policies[j] = policies[j + 1];
            }
            policy_count--;
            printf("Policy %s deleted\n", policy_name);
            return 0;
        }
    }

    printf("Error: Policy not found\n");
    return -1;
}

/* Command registration */
struct cmd_element policy_route_cmds[] = {
    HUAWEI_CMD_WITH_CATEGORY("policy-based-route", cmd_policy_based_route, "route-map",
                             "Configure policy-based routing", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("if-match acl", cmd_policy_if_match_acl, "match ip address",
                             "Match ACL", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("if-match ip-address source", cmd_policy_if_match_source, "match ip address",
                             "Match source address", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("if-match ip-address destination", cmd_policy_if_match_destination, "match ip address",
                             "Match destination address", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("if-match interface", cmd_policy_if_match_interface, "match interface",
                             "Match interface", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("if-match packet-length", cmd_policy_if_match_length, "match length",
                             "Match packet length", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("apply ip-address next-hop", cmd_policy_apply_nexthop, "set ip next-hop",
                             "Set next-hop address", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("apply output-interface", cmd_policy_apply_interface, "set interface",
                             "Set output interface", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("apply ip-address default next-hop", cmd_policy_apply_default_nexthop, "set default ip next-hop",
                             "Set default next-hop", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("apply ip-precedence", cmd_policy_apply_precedence, "set ip precedence",
                             "Set IP precedence", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("apply dscp", cmd_policy_apply_dscp, "set ip dscp",
                             "Set DSCP value", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("ip policy-based-route", cmd_interface_apply_policy, "ip policy route-map",
                             "Apply policy to interface", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("display policy-based-route", cmd_display_policy, "show route-map",
                             "Display policy configuration", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("undo policy-based-route", cmd_undo_policy, "no route-map",
                             "Delete policy", CMD_CAT_ROUTING),
    { .name = NULL }
};

/* Register policy routing commands */
void register_policy_route_cmds(void)
{
    printf("Registering policy-based routing commands...\n");
}
