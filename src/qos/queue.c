/*
 * QoS Queue Management for Huawei VRP Style
 * Copyright (C) 2026 WhiteBox NE Team
 *
 * This module provides queue management functionality including:
 * - Queue scheduling (PQ, WFQ, CBWFQ)
 * - Congestion avoidance (WRED)
 * - Queue depth management
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../frr_core/lib/huawei_cli.h"

/* Queue scheduling types */
typedef enum {
    SCHED_TYPE_PQ = 0,    /* Priority Queuing */
    SCHED_TYPE_WFQ,       /* Weighted Fair Queuing */
    SCHED_TYPE_CBWFQ      /* Class-Based WFQ */
} sched_type_t;

/* WRED configuration */
struct wred_config {
    bool enabled;
    uint16_t min_threshold;
    uint16_t max_threshold;
    uint8_t drop_probability;
};

/* Queue configuration */
struct queue_config {
    uint8_t queue_id;
    char name[64];
    sched_type_t sched_type;
    uint16_t bandwidth;      /* kbps or percentage */
    uint16_t max_depth;      /* packets */
    struct wred_config wred;
    uint64_t enqueue_packets;
    uint64_t dequeue_packets;
    uint64_t drop_packets;
};

/* Interface queue profile */
struct queue_profile {
    char interface[64];
    struct queue_config queues[8];
    int queue_count;
    sched_type_t default_sched;
};

static struct queue_profile profiles[64];
static int profile_count = 0;
static struct queue_profile *current_profile = NULL;

/*
 * Configure queue scheduling
 * Command: qos queue-profile <interface>
 */
static int cmd_qos_queue_profile(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Interface name required\n");
        printf("Usage: qos queue-profile <interface>\n");
        return -1;
    }

    const char *interface = args->argv[1];

    /* Find or create profile */
    current_profile = NULL;
    for (int i = 0; i < profile_count; i++) {
        if (strcmp(profiles[i].interface, interface) == 0) {
            current_profile = &profiles[i];
            break;
        }
    }

    if (!current_profile && profile_count < 64) {
        current_profile = &profiles[profile_count++];
        memset(current_profile, 0, sizeof(struct queue_profile));
        strncpy(current_profile->interface, interface, sizeof(current_profile->interface) - 1);
        current_profile->default_sched = SCHED_TYPE_WFQ;
    }

    if (!current_profile) {
        printf("Error: Maximum profiles reached\n");
        return -1;
    }

    printf("Configuring queue profile for interface %s\n", interface);

    return 0;
}

/*
 * Configure queue
 * Command: queue <queue-id> [bandwidth <value>] [max-depth <value>]
 */
static int cmd_queue(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_profile) {
        printf("Error: No queue profile configured\n");
        return -1;
    }

    if (args->argc < 1) {
        printf("Error: Queue ID required\n");
        printf("Usage: queue <queue-id> [bandwidth <value>] [max-depth <value>]\n");
        return -1;
    }

    uint8_t queue_id = atoi(args->argv[0]);
    if (queue_id > 7) {
        printf("Error: Queue ID must be 0-7\n");
        return -1;
    }

    /* Find or create queue */
    struct queue_config *queue = NULL;
    for (int i = 0; i < current_profile->queue_count; i++) {
        if (current_profile->queues[i].queue_id == queue_id) {
            queue = &current_profile->queues[i];
            break;
        }
    }

    if (!queue && current_profile->queue_count < 8) {
        queue = &current_profile->queues[current_profile->queue_count++];
        memset(queue, 0, sizeof(struct queue_config));
        queue->queue_id = queue_id;
        snprintf(queue->name, sizeof(queue->name), "queue-%u", queue_id);
        queue->sched_type = current_profile->default_sched;
        queue->bandwidth = 0;  /* 0 means no limit */
        queue->max_depth = 64; /* default 64 packets */
    }

    if (!queue) {
        printf("Error: Maximum queues reached\n");
        return -1;
    }

    /* Parse optional parameters */
    for (int i = 1; i < args->argc; i++) {
        if (strcmp(args->argv[i], "bandwidth") == 0 && i + 1 < args->argc) {
            queue->bandwidth = atoi(args->argv[++i]);
        } else if (strcmp(args->argv[i], "max-depth") == 0 && i + 1 < args->argc) {
            queue->max_depth = atoi(args->argv[++i]);
        }
    }

    printf("Queue %u configured: bandwidth %u kbps, max-depth %u packets\n",
           queue_id, queue->bandwidth, queue->max_depth);

    return 0;
}

/*
 * Configure queue scheduling type
 * Command: qos schedule {pq|wfq|cbwfq}
 */
static int cmd_qos_schedule(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_profile) {
        printf("Error: No queue profile configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: Scheduling type required\n");
        printf("Usage: qos schedule {pq|wfq|cbwfq}\n");
        return -1;
    }

    const char *sched = args->argv[1];

    if (strcmp(sched, "pq") == 0) {
        current_profile->default_sched = SCHED_TYPE_PQ;
        printf("Priority Queuing (PQ) configured\n");
    } else if (strcmp(sched, "wfq") == 0) {
        current_profile->default_sched = SCHED_TYPE_WFQ;
        printf("Weighted Fair Queuing (WFQ) configured\n");
    } else if (strcmp(sched, "cbwfq") == 0) {
        current_profile->default_sched = SCHED_TYPE_CBWFQ;
        printf("Class-Based WFQ (CBWFQ) configured\n");
    } else {
        printf("Error: Invalid scheduling type\n");
        return -1;
    }

    return 0;
}

/*
 * Configure WRED
 * Command: wred queue <queue-id> min-threshold <min> max-threshold <max> drop-probability <prob>
 */
static int cmd_wred(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_profile) {
        printf("Error: No queue profile configured\n");
        return -1;
    }

    if (args->argc < 8) {
        printf("Error: WRED parameters required\n");
        printf("Usage: wred queue <queue-id> min-threshold <min> max-threshold <max> drop-probability <prob>\n");
        return -1;
    }

    uint8_t queue_id = atoi(args->argv[1]);

    /* Find queue */
    struct queue_config *queue = NULL;
    for (int i = 0; i < current_profile->queue_count; i++) {
        if (current_profile->queues[i].queue_id == queue_id) {
            queue = &current_profile->queues[i];
            break;
        }
    }

    if (!queue) {
        printf("Error: Queue %u not found\n", queue_id);
        return -1;
    }

    /* Parse WRED parameters */
    queue->wred.enabled = true;
    queue->wred.min_threshold = atoi(args->argv[3]);
    queue->wred.max_threshold = atoi(args->argv[5]);
    queue->wred.drop_probability = atoi(args->argv[7]);

    printf("WRED configured for queue %u: min %u, max %u, drop-prob %u%%\n",
           queue_id, queue->wred.min_threshold, queue->wred.max_threshold,
           queue->wred.drop_probability);

    return 0;
}

/*
 * Display queue configuration
 * Command: display qos queue interface <interface>
 */
static int cmd_display_qos_queue(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 4) {
        printf("Error: Interface name required\n");
        printf("Usage: display qos queue interface <interface>\n");
        return -1;
    }

    const char *interface = args->argv[3];

    /* Find profile */
    struct queue_profile *profile = NULL;
    for (int i = 0; i < profile_count; i++) {
        if (strcmp(profiles[i].interface, interface) == 0) {
            profile = &profiles[i];
            break;
        }
    }

    if (!profile) {
        printf("No queue profile configured for interface %s\n", interface);
        return 0;
    }

    printf("Queue Configuration for interface %s:\n", interface);
    printf("Default Scheduling: ");
    switch (profile->default_sched) {
        case SCHED_TYPE_PQ:
            printf("PQ (Priority Queuing)\n");
            break;
        case SCHED_TYPE_WFQ:
            printf("WFQ (Weighted Fair Queuing)\n");
            break;
        case SCHED_TYPE_CBWFQ:
            printf("CBWFQ (Class-Based WFQ)\n");
            break;
    }

    printf("\n%-5s %-15s %-10s %-10s %-10s %-10s %-10s\n",
           "Queue", "Bandwidth", "Max-Depth", "WRED", "Enqueue", "Dequeue", "Drops");
    printf("%-5s %-15s %-10s %-10s %-10s %-10s %-10s\n",
           "-----", "---------------", "----------", "----------",
           "----------", "----------", "----------");

    for (int i = 0; i < profile->queue_count; i++) {
        struct queue_config *q = &profile->queues[i];
        printf("%-5u %-15u %-10u %-10s %-10lu %-10lu %-10lu\n",
               q->queue_id,
               q->bandwidth,
               q->max_depth,
               q->wred.enabled ? "Enabled" : "Disabled",
               q->enqueue_packets,
               q->dequeue_packets,
               q->drop_packets);
    }

    return 0;
}

/*
 * Initialize QoS queue module
 */
void qos_queue_init(void)
{
    /* Register commands */
    HUAWEI_CMD_WITH_CATEGORY("qos queue-profile", cmd_qos_queue_profile, NULL,
                            "Configure queue profile", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("queue", cmd_queue, NULL,
                            "Configure queue", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("qos schedule", cmd_qos_schedule, NULL,
                            "Configure queue scheduling", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("wred", cmd_wred, NULL,
                            "Configure WRED", "QoS");
    HUAWEI_CMD_WITH_CATEGORY("display qos queue", cmd_display_qos_queue, NULL,
                            "Display queue configuration", "QoS");
}

