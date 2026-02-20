/*
 * QoS Traffic Behavior for Huawei VRP Style
 * Copyright (C) 2026 WhiteBox NE Team
 *
 * This module provides traffic behavior functionality including:
 * - Remark DSCP/IP precedence
 * - Rate limiting (CAR)
 * - Traffic shaping
 * - Priority marking
 * - Packet filtering
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../frr_core/lib/huawei_cli.h"

/* Action types */
typedef enum {
    ACTION_TYPE_REMARK_DSCP = 0,
    ACTION_TYPE_REMARK_IP_PRECEDENCE,
    ACTION_TYPE_CAR,
    ACTION_TYPE_PRIORITY,
    ACTION_TYPE_DENY,
    ACTION_TYPE_REDIRECT
} action_type_t;

/* CAR (Committed Access Rate) configuration */
struct car_config {
    uint32_t cir;  /* Committed Information Rate (kbps) */
    uint32_t cbs;  /* Committed Burst Size (bytes) */
    uint32_t pir;  /* Peak Information Rate (kbps) */
    uint32_t pbs;  /* Peak Burst Size (bytes) */
    bool green_pass;
    bool yellow_pass;
    bool red_discard;
};

/* Traffic action */
struct traffic_action {
    action_type_t type;
    union {
        uint8_t dscp;
        uint8_t ip_precedence;
        struct car_config car;
        uint8_t priority;
        char redirect_interface[64];
    } value;
};

/* Traffic behavior */
struct traffic_behavior {
    char name[64];
    struct traffic_action actions[8];
    int action_count;
    uint64_t apply_count;
};

static struct traffic_behavior behaviors[256];
static int behavior_count = 0;
static struct traffic_behavior *current_behavior = NULL;

/*
 * Create or enter traffic behavior
 * Command: traffic behavior <name>
 */
static int cmd_traffic_behavior(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Behavior name required\n");
        printf("Usage: traffic behavior <name>\n");
        return -1;
    }

    const char *name = args->argv[1];

    /* Find or create behavior */
    current_behavior = NULL;
    for (int i = 0; i < behavior_count; i++) {
        if (strcmp(behaviors[i].name, name) == 0) {
            current_behavior = &behaviors[i];
            break;
        }
    }

    if (!current_behavior && behavior_count < 256) {
        current_behavior = &behaviors[behavior_count++];
        memset(current_behavior, 0, sizeof(struct traffic_behavior));
        strncpy(current_behavior->name, name, sizeof(current_behavior->name) - 1);
    }

    if (!current_behavior) {
        printf("Error: Maximum behaviors reached\n");
        return -1;
    }

    printf("Entering traffic behavior %s configuration\n", name);
    printf("[Huawei-behavior-%s]\n", name);

    return 0;
}

/*
 * Remark DSCP
 * Command: remark dscp <dscp-value>
 */
static int cmd_remark_dscp(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_behavior) {
        printf("Error: No behavior configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: DSCP value required\n");
        printf("Usage: remark dscp <dscp-value>\n");
        return -1;
    }

    uint8_t dscp = atoi(args->argv[1]);
    if (dscp > 63) {
        printf("Error: DSCP value must be 0-63\n");
        return -1;
    }

    if (current_behavior->action_count < 8) {
        struct traffic_action *action = &current_behavior->actions[current_behavior->action_count++];
        action->type = ACTION_TYPE_REMARK_DSCP;
        action->value.dscp = dscp;
        printf("Remark DSCP %u configured\n", dscp);
    } else {
        printf("Error: Maximum actions reached\n");
        return -1;
    }

    return 0;
}

/*
 * Configure CAR (Committed Access Rate)
 * Command: car cir <cir> [cbs <cbs>] [pir <pir>] [pbs <pbs>] [green pass] [yellow pass] [red discard]
 */
static int cmd_car(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_behavior) {
        printf("Error: No behavior configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: CIR value required\n");
        printf("Usage: car cir <cir> [cbs <cbs>] [pir <pir>] [pbs <pbs>]\n");
        return -1;
    }

    if (current_behavior->action_count >= 8) {
        printf("Error: Maximum actions reached\n");
        return -1;
    }

    struct traffic_action *action = &current_behavior->actions[current_behavior->action_count++];
    action->type = ACTION_TYPE_CAR;

    /* Parse CIR */
    action->value.car.cir = atoi(args->argv[1]);

    /* Default values */
    action->value.car.cbs = action->value.car.cir * 125;  /* 1 second burst */
    action->value.car.pir = action->value.car.cir;
    action->value.car.pbs = action->value.car.cbs;
    action->value.car.green_pass = true;
    action->value.car.yellow_pass = true;
    action->value.car.red_discard = true;

    /* Parse optional parameters */
    for (int i = 2; i < args->argc; i++) {
        if (strcmp(args->argv[i], "cbs") == 0 && i + 1 < args->argc) {
            action->value.car.cbs = atoi(args->argv[++i]);
        } else if (strcmp(args->argv[i], "pir") == 0 && i + 1 < args->argc) {
            action->value.car.pir = atoi(args->argv[++i]);
        } else if (strcmp(args->argv[i], "pbs") == 0 && i + 1 < args->argc) {
            action->value.car.pbs = atoi(args->argv[++i]);
        }
    }

    printf("CAR configured: CIR %u kbps, CBS %u bytes\n",
           action->value.car.cir, action->value.car.cbs);

    return 0;
}

/*
 * Set priority
 * Command: priority <priority-value>
 */
static int cmd_priority(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_behavior) {
        printf("Error: No behavior configured\n");
        return -1;
    }

    if (args->argc < 1) {
        printf("Error: Priority value required\n");
        printf("Usage: priority <priority-value>\n");
        return -1;
    }

    uint8_t priority = atoi(args->argv[0]);
    if (priority > 7) {
        printf("Error: Priority must be 0-7\n");
        return -1;
    }

    if (current_behavior->action_count < 8) {
        struct traffic_action *action = &current_behavior->actions[current_behavior->action_count++];
        action->type = ACTION_TYPE_PRIORITY;
        action->value.priority = priority;
        printf("Priority %u configured\n", priority);
    } else {
        printf("Error: Maximum actions reached\n");
        return -1;
    }

    return 0;
}

/*
 * Deny traffic
 * Command: deny
 */
static int cmd_deny(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_behavior) {
        printf("Error: No behavior configured\n");
        return -1;
    }

    if (current_behavior->action_count < 8) {
        struct traffic_action *action = &current_behavior->actions[current_behavior->action_count++];
        action->type = ACTION_TYPE_DENY;
        printf("Deny action configured\n");
    } else {
        printf("Error: Maximum actions reached\n");
        return -1;
    }

    return 0;
}

/*
 * Display traffic behaviors
 * Command: display traffic behavior [name]
 */
static int cmd_display_traffic_behavior(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc > 1) {
        /* Display specific behavior */
        const char *name = args->argv[1];
        for (int i = 0; i < behavior_count; i++) {
            if (strcmp(behaviors[i].name, name) == 0) {
                printf("Traffic Behavior: %s\n", behaviors[i].name);
                printf("  Actions: %d\n", behaviors[i].action_count);
                printf("  Apply count: %lu\n", behaviors[i].apply_count);

                for (int j = 0; j < behaviors[i].action_count; j++) {
                    struct traffic_action *action = &behaviors[i].actions[j];
                    printf("  Action %d: ", j + 1);

                    switch (action->type) {
                        case ACTION_TYPE_REMARK_DSCP:
                            printf("Remark DSCP %u\n", action->value.dscp);
                            break;
                        case ACTION_TYPE_CAR:
                            printf("CAR CIR %u kbps, CBS %u bytes\n",
                                   action->value.car.cir, action->value.car.cbs);
                            break;
                        case ACTION_TYPE_PRIORITY:
                            printf("Priority %u\n", action->value.priority);
                            break;
                        case ACTION_TYPE_DENY:
                            printf("Deny\n");
                            break;
                        default:
                            printf("Unknown\n");
                    }
                }
                return 0;
            }
        }
        printf("Error: Behavior %s not found\n", name);
        return -1;
    }

    /* Display all behaviors */
    printf("Traffic Behaviors:\n");
    printf("%-20s %-10s %-15s\n", "Name", "Actions", "Apply Count");
    printf("%-20s %-10s %-15s\n", "--------------------", "----------", "---------------");

    for (int i = 0; i < behavior_count; i++) {
        printf("%-20s %-10d %-15lu\n",
               behaviors[i].name,
               behaviors[i].action_count,
               behaviors[i].apply_count);
    }

    return 0;
}

/*
 * Initialize QoS behavior module
 */
void qos_behavior_init(void)
{
    /* Register commands */
    HUAWEI_CMD_WITH_CATEGORY("traffic behavior", cmd_traffic_behavior, NULL,
                            "Configure traffic behavior", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("remark dscp", cmd_remark_dscp, NULL,
                            "Remark DSCP value", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("car", cmd_car, NULL,
                            "Configure CAR", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("priority", cmd_priority, NULL,
                            "Set priority", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("deny", cmd_deny, NULL,
                            "Deny traffic", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("display traffic behavior", cmd_display_traffic_behavior, NULL,
                            "Display traffic behaviors", "QoS");
}


