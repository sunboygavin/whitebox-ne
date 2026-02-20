/*
 * Zone-Based Firewall for Huawei VRP Style
 * Copyright (C) 2026 WhiteBox NE Team
 *
 * This module provides zone-based firewall functionality including:
 * - Security zones
 * - Zone member management
 * - Security policies
 * - Stateful inspection
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../frr_core/lib/huawei_cli.h"

/* Security zone configuration */
struct security_zone {
    char name[64];
    uint8_t priority;
    char description[128];
    char member_interfaces[32][64];
    int member_count;
};

/* Security policy rule */
struct security_rule {
    uint32_t rule_id;
    char name[64];
    char source_zone[64];
    char destination_zone[64];
    char source_address[64];
    char destination_address[64];
    char service[64];
    char action[16];  /* permit/deny */
    bool logging;
    uint64_t packet_count;
    uint64_t byte_count;
};

/* Security policy */
struct security_policy {
    char name[64];
    struct security_rule rules[256];
    int rule_count;
};

/* Global configurations */
static struct security_zone zones[16];
static int zone_count = 0;
static struct security_policy policies[16];
static int policy_count = 0;
static struct security_zone *current_zone = NULL;
static struct security_policy *current_policy = NULL;
static struct security_rule *current_rule = NULL;

/*
 * Create or enter security zone
 * Command: firewall zone <zone-name>
 */
static int cmd_firewall_zone(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Zone name required\n");
        printf("Usage: firewall zone <zone-name>\n");
        return -1;
    }

    const char *zone_name = args->argv[1];

    /* Find or create zone */
    current_zone = NULL;
    for (int i = 0; i < zone_count; i++) {
        if (strcmp(zones[i].name, zone_name) == 0) {
            current_zone = &zones[i];
            break;
        }
    }

    if (!current_zone && zone_count < 16) {
        current_zone = &zones[zone_count++];
        memset(current_zone, 0, sizeof(struct security_zone));
        strncpy(current_zone->name, zone_name, sizeof(current_zone->name) - 1);
        current_zone->priority = 50;
    }

    if (!current_zone) {
        printf("Error: Maximum zones reached\n");
        return -1;
    }

    printf("Entering security zone %s configuration\n", zone_name);
    printf("[Huawei-zone-%s]\n", zone_name);

    return 0;
}

/*
 * Set zone priority
 * Command: set priority <priority>
 */
static int cmd_zone_priority(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_zone) {
        printf("Error: No zone configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: Priority value required\n");
        printf("Usage: set priority <priority>\n");
        return -1;
    }

    uint8_t priority = atoi(args->argv[1]);
    if (priority > 100) {
        printf("Error: Priority must be 0-100\n");
        return -1;
    }

    current_zone->priority = priority;
    printf("Zone priority set to %u\n", priority);

    return 0;
}

/*
 * Add interface to zone
 * Command: add interface <interface-name>
 */
static int cmd_zone_add_interface(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_zone) {
        printf("Error: No zone configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: Interface name required\n");
        printf("Usage: add interface <interface-name>\n");
        return -1;
    }

    const char *interface = args->argv[1];

    if (current_zone->member_count < 32) {
        strncpy(current_zone->member_interfaces[current_zone->member_count],
                interface, 63);
        current_zone->member_count++;
        printf("Interface %s added to zone %s\n", interface, current_zone->name);
    } else {
        printf("Error: Maximum interfaces reached for zone\n");
        return -1;
    }

    return 0;
}

/*
 * Create or enter security policy
 * Command: security-policy
 */
static int cmd_security_policy(struct cmd_element *cmd, struct cmd_args *args)
{
    /* Use default policy */
    current_policy = NULL;
    for (int i = 0; i < policy_count; i++) {
        if (strcmp(policies[i].name, "default") == 0) {
            current_policy = &policies[i];
            break;
        }
    }

    if (!current_policy && policy_count < 16) {
        current_policy = &policies[policy_count++];
        memset(current_policy, 0, sizeof(struct security_policy));
        strncpy(current_policy->name, "default", sizeof(current_policy->name) - 1);
    }

    printf("Entering security policy configuration\n");
    printf("[Huawei-policy-security]\n");

    return 0;
}

/*
 * Create or enter security rule
 * Command: rule name <rule-name>
 */
static int cmd_security_rule(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_policy) {
        printf("Error: No security policy configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: Rule name required\n");
        printf("Usage: rule name <rule-name>\n");
        return -1;
    }

    const char *rule_name = args->argv[1];

    /* Find or create rule */
    current_rule = NULL;
    for (int i = 0; i < current_policy->rule_count; i++) {
        if (strcmp(current_policy->rules[i].name, rule_name) == 0) {
            current_rule = &current_policy->rules[i];
            break;
        }
    }

    if (!current_rule && current_policy->rule_count < 256) {
        current_rule = &current_policy->rules[current_policy->rule_count++];
        memset(current_rule, 0, sizeof(struct security_rule));
        strncpy(current_rule->name, rule_name, sizeof(current_rule->name) - 1);
        current_rule->rule_id = current_policy->rule_count;
        strncpy(current_rule->action, "deny", sizeof(current_rule->action) - 1);
    }

    if (!current_rule) {
        printf("Error: Maximum rules reached\n");
        return -1;
    }

    printf("Entering security rule %s configuration\n", rule_name);
    printf("[Huawei-policy-security-rule-%s]\n", rule_name);

    return 0;
}

/*
 * Set source zone
 * Command: source-zone <zone-name>
 */
static int cmd_rule_source_zone(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_rule) {
        printf("Error: No rule configured\n");
        return -1;
    }

    if (args->argc < 1) {
        printf("Error: Zone name required\n");
        printf("Usage: source-zone <zone-name>\n");
        return -1;
    }

    strncpy(current_rule->source_zone, args->argv[0],
            sizeof(current_rule->source_zone) - 1);
    printf("Source zone set to %s\n", args->argv[0]);

    return 0;
}

/*
 * Set destination zone
 * Command: destination-zone <zone-name>
 */
static int cmd_rule_destination_zone(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_rule) {
        printf("Error: No rule configured\n");
        return -1;
    }

    if (args->argc < 1) {
        printf("Error: Zone name required\n");
        printf("Usage: destination-zone <zone-name>\n");
        return -1;
    }

    strncpy(current_rule->destination_zone, args->argv[0],
            sizeof(current_rule->destination_zone) - 1);
    printf("Destination zone set to %s\n", args->argv[0]);

    return 0;
}

/*
 * Set source address
 * Command: source-address <address>
 */
static int cmd_rule_source_address(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_rule) {
        printf("Error: No rule configured\n");
        return -1;
    }

    if (args->argc < 1) {
        printf("Error: Address required\n");
        printf("Usage: source-address <address>\n");
        return -1;
    }

    strncpy(current_rule->source_address, args->argv[0],
            sizeof(current_rule->source_address) - 1);
    printf("Source address set to %s\n", args->argv[0]);

    return 0;
}

/*
 * Set destination address
 * Command: destination-address <address>
 */
static int cmd_rule_destination_address(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_rule) {
        printf("Error: No rule configured\n");
        return -1;
    }

    if (args->argc < 1) {
        printf("Error: Address required\n");
        printf("Usage: destination-address <address>\n");
        return -1;
    }

    strncpy(current_rule->destination_address, args->argv[0],
            sizeof(current_rule->destination_address) - 1);
    printf("Destination address set to %s\n", args->argv[0]);

    return 0;
}

/*
 * Set service
 * Command: service <service-name>
 */
static int cmd_rule_service(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_rule) {
        printf("Error: No rule configured\n");
        return -1;
    }

    if (args->argc < 1) {
        printf("Error: Service required\n");
        printf("Usage: service <service-name>\n");
        return -1;
    }

    strncpy(current_rule->service, args->argv[0],
            sizeof(current_rule->service) - 1);
    printf("Service set to %s\n", args->argv[0]);

    return 0;
}

/*
 * Set action
 * Command: action {permit|deny}
 */
static int cmd_rule_action(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_rule) {
        printf("Error: No rule configured\n");
        return -1;
    }

    if (args->argc < 1) {
        printf("Error: Action required\n");
        printf("Usage: action {permit|deny}\n");
        return -1;
    }

    const char *action = args->argv[0];
    if (strcmp(action, "permit") != 0 && strcmp(action, "deny") != 0) {
        printf("Error: Action must be 'permit' or 'deny'\n");
        return -1;
    }

    strncpy(current_rule->action, action, sizeof(current_rule->action) - 1);
    printf("Action set to %s\n", action);

    return 0;
}

/*
 * Display firewall zones
 * Command: display firewall zone
 */
static int cmd_display_firewall_zone(struct cmd_element *cmd, struct cmd_args *args)
{
    printf("Firewall Zones:\n");
    printf("  Total zones: %d\n\n", zone_count);

    for (int i = 0; i < zone_count; i++) {
        printf("  Zone: %s\n", zones[i].name);
        printf("    Priority: %u\n", zones[i].priority);
        printf("    Members: %d interfaces\n", zones[i].member_count);
        for (int j = 0; j < zones[i].member_count; j++) {
            printf("      - %s\n", zones[i].member_interfaces[j]);
        }
        printf("\n");
    }

    return 0;
}

/*
 * Display security policy
 * Command: display security-policy
 */
static int cmd_display_security_policy(struct cmd_element *cmd, struct cmd_args *args)
{
    printf("Security Policies:\n");
    printf("  Total policies: %d\n\n", policy_count);

    for (int i = 0; i < policy_count; i++) {
        printf("  Policy: %s\n", policies[i].name);
        printf("    Rules: %d\n", policies[i].rule_count);

        for (int j = 0; j < policies[i].rule_count; j++) {
            struct security_rule *rule = &policies[i].rules[j];
            printf("      Rule %u: %s\n", rule->rule_id, rule->name);
            printf("        Source zone: %s\n", rule->source_zone);
            printf("        Destination zone: %s\n", rule->destination_zone);
            printf("        Action: %s\n", rule->action);
            if (rule->packet_count > 0) {
                printf("        Packets: %lu, Bytes: %lu\n",
                       rule->packet_count, rule->byte_count);
            }
        }
        printf("\n");
    }

    return 0;
}

/* Command registration */
struct cmd_element firewall_cmds[] = {
    HUAWEI_CMD_WITH_CATEGORY("firewall zone", cmd_firewall_zone, "zone security",
                             "Create or enter security zone", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("set priority", cmd_zone_priority, "priority",
                             "Set zone priority", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("add interface", cmd_zone_add_interface, "zone-member",
                             "Add interface to zone", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("security-policy", cmd_security_policy, "policy-map",
                             "Enter security policy configuration", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("rule name", cmd_security_rule, "class-map",
                             "Create or enter security rule", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("source-zone", cmd_rule_source_zone, "match source",
                             "Set source zone", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("destination-zone", cmd_rule_destination_zone, "match destination",
                             "Set destination zone", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("source-address", cmd_rule_source_address, "match source address",
                             "Set source address", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("destination-address", cmd_rule_destination_address, "match destination address",
                             "Set destination address", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("service", cmd_rule_service, "match service",
                             "Set service", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("action", cmd_rule_action, "police",
                             "Set action (permit/deny)", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("display firewall zone", cmd_display_firewall_zone, "show zone security",
                             "Display firewall zones", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("display security-policy", cmd_display_security_policy, "show policy-map",
                             "Display security policies", CMD_CAT_SECURITY),
    { .name = NULL }
};

void register_firewall_cmds(void)
{
    printf("Registering firewall commands...\n");
}
