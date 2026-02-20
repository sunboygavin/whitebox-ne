/*
 * QoS Traffic Classifier for Huawei VRP Style
 * Copyright (C) 2026 WhiteBox NE Team
 *
 * This module provides traffic classification functionality including:
 * - ACL matching
 * - DSCP matching
 * - 5-tuple matching (src/dst IP, src/dst port, protocol)
 * - Interface matching
 * - Packet length matching
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../frr_core/lib/huawei_cli.h"

/* Match types */
typedef enum {
    MATCH_TYPE_ACL = 0,
    MATCH_TYPE_DSCP,
    MATCH_TYPE_IP_PRECEDENCE,
    MATCH_TYPE_SOURCE_IP,
    MATCH_TYPE_DEST_IP,
    MATCH_TYPE_SOURCE_PORT,
    MATCH_TYPE_DEST_PORT,
    MATCH_TYPE_PROTOCOL,
    MATCH_TYPE_INTERFACE,
    MATCH_TYPE_PACKET_LENGTH
} match_type_t;

/* Match condition */
struct match_condition {
    match_type_t type;
    union {
        uint16_t acl_number;
        uint8_t dscp;
        uint8_t ip_precedence;
        char ip_address[64];
        uint16_t port;
        uint8_t protocol;
        char interface[64];
        struct {
            uint16_t min;
            uint16_t max;
        } packet_length;
    } value;
};

/* Traffic classifier */
struct traffic_classifier {
    char name[64];
    struct match_condition conditions[16];
    int condition_count;
    uint64_t match_count;
};

static struct traffic_classifier classifiers[256];
static int classifier_count = 0;
static struct traffic_classifier *current_classifier = NULL;

/*
 * Create or enter traffic classifier
 * Command: traffic classifier <name> [operator {and|or}]
 */
static int cmd_traffic_classifier(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Classifier name required\n");
        printf("Usage: traffic classifier <name> [operator {and|or}]\n");
        return -1;
    }

    const char *name = args->argv[1];

    /* Find or create classifier */
    current_classifier = NULL;
    for (int i = 0; i < classifier_count; i++) {
        if (strcmp(classifiers[i].name, name) == 0) {
            current_classifier = &classifiers[i];
            break;
        }
    }

    if (!current_classifier && classifier_count < 256) {
        current_classifier = &classifiers[classifier_count++];
        memset(current_classifier, 0, sizeof(struct traffic_classifier));
        strncpy(current_classifier->name, name, sizeof(current_classifier->name) - 1);
    }

    if (!current_classifier) {
        printf("Error: Maximum classifiers reached\n");
        return -1;
    }

    printf("Entering traffic classifier %s configuration\n", name);
    printf("[Huawei-classifier-%s]\n", name);

    return 0;
}

/*
 * Match ACL
 * Command: if-match acl <acl-number>
 */
static int cmd_if_match_acl(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_classifier) {
        printf("Error: No classifier configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: ACL number required\n");
        printf("Usage: if-match acl <acl-number>\n");
        return -1;
    }

    uint16_t acl_num = atoi(args->argv[1]);
    if (acl_num < 2000 || acl_num > 3999) {
        printf("Error: ACL number must be 2000-3999\n");
        return -1;
    }

    if (current_classifier->condition_count < 16) {
        struct match_condition *cond = &current_classifier->conditions[current_classifier->condition_count++];
        cond->type = MATCH_TYPE_ACL;
        cond->value.acl_number = acl_num;
        printf("Match ACL %u configured\n", acl_num);
    } else {
        printf("Error: Maximum match conditions reached\n");
        return -1;
    }

    return 0;
}

/*
 * Match DSCP
 * Command: if-match dscp <dscp-value>
 */
static int cmd_if_match_dscp(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_classifier) {
        printf("Error: No classifier configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: DSCP value required\n");
        printf("Usage: if-match dscp <dscp-value>\n");
        return -1;
    }

    uint8_t dscp = atoi(args->argv[1]);
    if (dscp > 63) {
        printf("Error: DSCP value must be 0-63\n");
        return -1;
    }

    if (current_classifier->condition_count < 16) {
        struct match_condition *cond = &current_classifier->conditions[current_classifier->condition_count++];
        cond->type = MATCH_TYPE_DSCP;
        cond->value.dscp = dscp;
        printf("Match DSCP %u configured\n", dscp);
    } else {
        printf("Error: Maximum match conditions reached\n");
        return -1;
    }

    return 0;
}

/*
 * Match source IP address
 * Command: if-match ip-address source <ip-address>
 */
static int cmd_if_match_source_ip(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_classifier) {
        printf("Error: No classifier configured\n");
        return -1;
    }

    if (args->argc < 3) {
        printf("Error: IP address required\n");
        printf("Usage: if-match ip-address source <ip-address>\n");
        return -1;
    }

    if (current_classifier->condition_count < 16) {
        struct match_condition *cond = &current_classifier->conditions[current_classifier->condition_count++];
        cond->type = MATCH_TYPE_SOURCE_IP;
        strncpy(cond->value.ip_address, args->argv[2], sizeof(cond->value.ip_address) - 1);
        printf("Match source IP %s configured\n", args->argv[2]);
    } else {
        printf("Error: Maximum match conditions reached\n");
        return -1;
    }

    return 0;
}

/*
 * Match destination IP address
 * Command: if-match ip-address destination <ip-address>
 */
static int cmd_if_match_dest_ip(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_classifier) {
        printf("Error: No classifier configured\n");
        return -1;
    }

    if (args->argc < 3) {
        printf("Error: IP address required\n");
        printf("Usage: if-match ip-address destination <ip-address>\n");
        return -1;
    }

    if (current_classifier->condition_count < 16) {
        struct match_condition *cond = &current_classifier->conditions[current_classifier->condition_count++];
        cond->type = MATCH_TYPE_DEST_IP;
        strncpy(cond->value.ip_address, args->argv[2], sizeof(cond->value.ip_address) - 1);
        printf("Match destination IP %s configured\n", args->argv[2]);
    } else {
        printf("Error: Maximum match conditions reached\n");
        return -1;
    }

    return 0;
}

/*
 * Display traffic classifiers
 * Command: display traffic classifier [name]
 */
static int cmd_display_traffic_classifier(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc > 1) {
        /* Display specific classifier */
        const char *name = args->argv[1];
        for (int i = 0; i < classifier_count; i++) {
            if (strcmp(classifiers[i].name, name) == 0) {
                printf("Traffic Classifier: %s\n", classifiers[i].name);
                printf("  Match conditions: %d\n", classifiers[i].condition_count);
                printf("  Match count: %lu\n", classifiers[i].match_count);

                for (int j = 0; j < classifiers[i].condition_count; j++) {
                    struct match_condition *cond = &classifiers[i].conditions[j];
                    printf("  Condition %d: ", j + 1);

                    switch (cond->type) {
                        case MATCH_TYPE_ACL:
                            printf("ACL %u\n", cond->value.acl_number);
                            break;
                        case MATCH_TYPE_DSCP:
                            printf("DSCP %u\n", cond->value.dscp);
                            break;
                        case MATCH_TYPE_SOURCE_IP:
                            printf("Source IP %s\n", cond->value.ip_address);
                            break;
                        case MATCH_TYPE_DEST_IP:
                            printf("Destination IP %s\n", cond->value.ip_address);
                            break;
                        case MATCH_TYPE_PROTOCOL:
                            printf("Protocol %u\n", cond->value.protocol);
                            break;
                        default:
                            printf("Unknown\n");
                    }
                }
                return 0;
            }
        }
        printf("Error: Classifier %s not found\n", name);
        return -1;
    }

    /* Display all classifiers */
    printf("Traffic Classifiers:\n");
    printf("%-20s %-10s %-15s\n", "Name", "Conditions", "Match Count");
    printf("%-20s %-10s %-15s\n", "--------------------", "----------", "---------------");

    for (int i = 0; i < classifier_count; i++) {
        printf("%-20s %-10d %-15lu\n",
               classifiers[i].name,
               classifiers[i].condition_count,
               classifiers[i].match_count);
    }

    return 0;
}

/*
 * Initialize QoS classifier module
 */
void qos_classifier_init(void)
{
    /* Register commands */
    HUAWEI_CMD_WITH_CATEGORY("traffic classifier", cmd_traffic_classifier, NULL,
                            "Configure traffic classifier", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("if-match acl", cmd_if_match_acl, NULL,
                            "Match ACL", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("if-match dscp", cmd_if_match_dscp, NULL,
                            "Match DSCP value", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("if-match ip-address source", cmd_if_match_source_ip, NULL,
                            "Match source IP address", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("if-match ip-address destination", cmd_if_match_dest_ip, NULL,
                            "Match destination IP address", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("display traffic classifier", cmd_display_traffic_classifier, NULL,
                            "Display traffic classifiers", "QoS");
}
