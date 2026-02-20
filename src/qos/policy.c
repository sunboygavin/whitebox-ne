/*
 * QoS Traffic Policy for Huawei VRP Style
 * Copyright (C) 2026 WhiteBox NE Team
 *
 * This module provides traffic policy functionality including:
 * - Binding classifiers to behaviors
 * - Policy application to interfaces
 * - Statistics collection
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../frr_core/lib/huawei_cli.h"

/* Policy rule */
struct policy_rule {
    char classifier_name[64];
    char behavior_name[64];
    uint64_t match_packets;
    uint64_t match_bytes;
};

/* Traffic policy */
struct traffic_policy {
    char name[64];
    struct policy_rule rules[32];
    int rule_count;
    bool applied;
    char applied_interface[64];
    char direction[16];  /* inbound/outbound */
};

static struct traffic_policy policies[128];
static int policy_count = 0;
static struct traffic_policy *current_policy = NULL;

/*
 * Create or enter traffic policy
 * Command: traffic policy <name>
 */
static int cmd_traffic_policy(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Policy name required\n");
        printf("Usage: traffic policy <name>\n");
        return -1;
    }

    const char *name = args->argv[1];

    /* Find or create policy */
    current_policy = NULL;
    for (int i = 0; i < policy_count; i++) {
        if (strcmp(policies[i].name, name) == 0) {
            current_policy = &policies[i];
            break;
        }
    }

    if (!current_policy && policy_count < 128) {
        current_policy = &policies[policy_count++];
        memset(current_policy, 0, sizeof(struct traffic_policy));
        strncpy(current_policy->name, name, sizeof(current_policy->name) - 1);
    }

    if (!current_policy) {
        printf("Error: Maximum policies reached\n");
        return -1;
    }

    printf("Entering traffic policy %s configuration\n", name);
    printf("[Huawei-trafficpolicy-%s]\n", name);

    return 0;
}

/*
 * Bind classifier to behavior
 * Command: classifier <classifier-name> behavior <behavior-name>
 */
static int cmd_classifier_behavior(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_policy) {
        printf("Error: No policy configured\n");
        return -1;
    }

    if (args->argc < 3) {
        printf("Error: Classifier and behavior names required\n");
        printf("Usage: classifier <classifier-name> behavior <behavior-name>\n");
        return -1;
    }

    const char *classifier_name = args->argv[0];
    const char *behavior_name = args->argv[2];

    if (current_policy->rule_count < 32) {
        struct policy_rule *rule = &current_policy->rules[current_policy->rule_count++];
        strncpy(rule->classifier_name, classifier_name, sizeof(rule->classifier_name) - 1);
        strncpy(rule->behavior_name, behavior_name, sizeof(rule->behavior_name) - 1);
        rule->match_packets = 0;
        rule->match_bytes = 0;
        printf("Classifier %s bound to behavior %s\n", classifier_name, behavior_name);
    } else {
        printf("Error: Maximum rules reached\n");
        return -1;
    }

    return 0;
}

/*
 * Apply policy to interface
 * Command: traffic-policy <policy-name> {inbound|outbound}
 */
static int cmd_apply_traffic_policy(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Policy name and direction required\n");
        printf("Usage: traffic-policy <policy-name> {inbound|outbound}\n");
        return -1;
    }

    const char *policy_name = args->argv[0];
    const char *direction = args->argv[1];

    if (strcmp(direction, "inbound") != 0 && strcmp(direction, "outbound") != 0) {
        printf("Error: Direction must be 'inbound' or 'outbound'\n");
        return -1;
    }

    /* Find policy */
    struct traffic_policy *policy = NULL;
    for (int i = 0; i < policy_count; i++) {
        if (strcmp(policies[i].name, policy_name) == 0) {
            policy = &policies[i];
            break;
        }
    }

    if (!policy) {
        printf("Error: Policy %s not found\n", policy_name);
        return -1;
    }

    policy->applied = true;
    strncpy(policy->direction, direction, sizeof(policy->direction) - 1);
    printf("Traffic policy %s applied %s\n", policy_name, direction);

    return 0;
}

/*
 * Display traffic policy
 * Command: display traffic policy [name] [interface <interface-name>]
 */
static int cmd_display_traffic_policy(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc > 2) {
        /* Display specific policy */
        const char *name = args->argv[2];
        for (int i = 0; i < policy_count; i++) {
            if (strcmp(policies[i].name, name) == 0) {
                printf("Traffic Policy: %s\n", policies[i].name);
                printf("  Rules: %d\n", policies[i].rule_count);
                printf("  Applied: %s\n", policies[i].applied ? "Yes" : "No");
                if (policies[i].applied) {
                    printf("  Direction: %s\n", policies[i].direction);
                }

                printf("\n  Policy Rules:\n");
                printf("  %-20s %-20s %-15s %-15s\n",
                       "Classifier", "Behavior", "Packets", "Bytes");
                printf("  %-20s %-20s %-15s %-15s\n",
                       "--------------------", "--------------------",
                       "---------------", "---------------");

                for (int j = 0; j < policies[i].rule_count; j++) {
                    struct policy_rule *rule = &policies[i].rules[j];
                    printf("  %-20s %-20s %-15lu %-15lu\n",
                           rule->classifier_name,
                           rule->behavior_name,
                           rule->match_packets,
                           rule->match_bytes);
                }
                return 0;
            }
        }
        printf("Error: Policy %s not found\n", name);
        return -1;
    }

    /* Display all policies */
    printf("Traffic Policies:\n");
    printf("%-20s %-10s %-10s %-15s\n", "Name", "Rules", "Applied", "Direction");
    printf("%-20s %-10s %-10s %-15s\n",
           "--------------------", "----------", "----------", "---------------");

    for (int i = 0; i < policy_count; i++) {
        printf("%-20s %-10d %-10s %-15s\n",
               policies[i].name,
               policies[i].rule_count,
               policies[i].applied ? "Yes" : "No",
               policies[i].applied ? policies[i].direction : "-");
    }

    return 0;
}

/*
 * Display QoS statistics
 * Command: display qos statistics interface <interface-name>
 */
static int cmd_display_qos_statistics(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 4) {
        printf("Error: Interface name required\n");
        printf("Usage: display qos statistics interface <interface-name>\n");
        return -1;
    }

    const char *interface = args->argv[3];

    printf("QoS Statistics for interface %s:\n\n", interface);

    /* Find applied policies */
    bool found = false;
    for (int i = 0; i < policy_count; i++) {
        if (policies[i].applied) {
            found = true;
            printf("Policy: %s (%s)\n", policies[i].name, policies[i].direction);
            printf("%-20s %-20s %-15s %-15s\n",
                   "Classifier", "Behavior", "Packets", "Bytes");
            printf("%-20s %-20s %-15s %-15s\n",
                   "--------------------", "--------------------",
                   "---------------", "---------------");

            for (int j = 0; j < policies[i].rule_count; j++) {
                struct policy_rule *rule = &policies[i].rules[j];
                printf("%-20s %-20s %-15lu %-15lu\n",
                       rule->classifier_name,
                       rule->behavior_name,
                       rule->match_packets,
                       rule->match_bytes);
            }
            printf("\n");
        }
    }

    if (!found) {
        printf("No QoS policy applied to interface %s\n", interface);
    }

    return 0;
}

/*
 * Initialize QoS policy module
 */
void qos_policy_init(void)
{
    /* Register commands */
    HUAWEI_CMD_WITH_CATEGORY("traffic policy", cmd_traffic_policy, NULL,
                            "Configure traffic policy", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("classifier", cmd_classifier_behavior, NULL,
                            "Bind classifier to behavior", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("traffic-policy", cmd_apply_traffic_policy, NULL,
                            "Apply traffic policy to interface", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("display traffic policy", cmd_display_traffic_policy, NULL,
                            "Display traffic policies", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("display qos statistics", cmd_display_qos_statistics, NULL,
                            "Display QoS statistics", "QoS");
}

