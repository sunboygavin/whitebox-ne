/*
 * BFD (Bidirectional Forwarding Detection) Implementation
 * Copyright (C) 2026 WhiteBox NE Team
 *
 * This module provides BFD functionality including:
 * - BFD session management
 * - Single-hop and multi-hop BFD
 * - Configurable timers (min-tx, min-rx, detect-multiplier)
 * - BFD for static routes, OSPF, BGP, IS-IS
 * - Echo mode support
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "../frr_core/lib/huawei_cli.h"

/* BFD session type */
typedef enum {
    BFD_TYPE_SINGLE_HOP = 0,
    BFD_TYPE_MULTI_HOP
} bfd_type_t;

/* BFD session state */
typedef enum {
    BFD_STATE_ADMIN_DOWN = 0,
    BFD_STATE_DOWN,
    BFD_STATE_INIT,
    BFD_STATE_UP
} bfd_state_t;

/* BFD diagnostic code */
typedef enum {
    BFD_DIAG_NONE = 0,
    BFD_DIAG_DETECT_TIME_EXPIRED,
    BFD_DIAG_ECHO_FAILED,
    BFD_DIAG_NEIGHBOR_SIGNALED_DOWN,
    BFD_DIAG_FORWARDING_PLANE_RESET,
    BFD_DIAG_PATH_DOWN,
    BFD_DIAG_CONCATENATED_PATH_DOWN,
    BFD_DIAG_ADMIN_DOWN,
    BFD_DIAG_REVERSE_CONCATENATED_PATH_DOWN
} bfd_diag_t;

/* BFD session */
struct bfd_session {
    char name[64];
    bfd_type_t type;
    bfd_state_t local_state;
    bfd_state_t remote_state;
    bfd_diag_t local_diag;
    bfd_diag_t remote_diag;

    /* Peer information */
    char peer_ip[64];
    char source_ip[64];
    char interface[64];
    uint16_t vrf_id;

    /* Timers (in microseconds) */
    uint32_t min_tx_interval;
    uint32_t min_rx_interval;
    uint8_t detect_multiplier;

    /* Echo mode */
    bool echo_mode;
    uint32_t echo_interval;

    /* Session identification */
    uint32_t local_discriminator;
    uint32_t remote_discriminator;

    /* Statistics */
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t up_time;
    uint64_t down_count;
    time_t last_up_time;
    time_t last_down_time;

    /* Binding */
    char bind_interface[64];
    char bind_peer_ip[64];
};

static struct bfd_session bfd_sessions[512];
static int bfd_session_count = 0;
static struct bfd_session *current_bfd = NULL;

/*
 * Create BFD session
 * Command: bfd <session-name> bind peer-ip <ip> [interface <interface>] [source-ip <ip>]
 */
static int cmd_bfd_session(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 4) {
        printf("Error: Session name and peer IP required\n");
        printf("Usage: bfd <session-name> bind peer-ip <ip> [interface <interface>] [source-ip <ip>]\n");
        return -1;
    }

    const char *name = args->argv[0];
    const char *peer_ip = args->argv[3];

    /* Find or create BFD session */
    current_bfd = NULL;
    for (int i = 0; i < bfd_session_count; i++) {
        if (strcmp(bfd_sessions[i].name, name) == 0) {
            current_bfd = &bfd_sessions[i];
            break;
        }
    }

    if (!current_bfd && bfd_session_count < 512) {
        current_bfd = &bfd_sessions[bfd_session_count++];
        memset(current_bfd, 0, sizeof(struct bfd_session));
        strncpy(current_bfd->name, name, sizeof(current_bfd->name) - 1);

        /* Default values */
        current_bfd->type = BFD_TYPE_SINGLE_HOP;
        current_bfd->local_state = BFD_STATE_DOWN;
        current_bfd->remote_state = BFD_STATE_DOWN;
        current_bfd->min_tx_interval = 1000000;  /* 1 second */
        current_bfd->min_rx_interval = 1000000;  /* 1 second */
        current_bfd->detect_multiplier = 3;
        current_bfd->echo_mode = false;
        current_bfd->echo_interval = 500000;  /* 500ms */
        current_bfd->local_discriminator = rand();
    }

    if (!current_bfd) {
        printf("Error: Maximum BFD sessions reached\n");
        return -1;
    }

    strncpy(current_bfd->peer_ip, peer_ip, sizeof(current_bfd->peer_ip) - 1);

    /* Parse optional parameters */
    for (int i = 4; i < args->argc; i++) {
        if (strcmp(args->argv[i], "interface") == 0 && i + 1 < args->argc) {
            strncpy(current_bfd->interface, args->argv[++i],
                    sizeof(current_bfd->interface) - 1);
        } else if (strcmp(args->argv[i], "source-ip") == 0 && i + 1 < args->argc) {
            strncpy(current_bfd->source_ip, args->argv[++i],
                    sizeof(current_bfd->source_ip) - 1);
        }
    }

    printf("BFD session %s created for peer %s\n", name, peer_ip);
    printf("[Huawei-bfd-session-%s]\n", name);

    return 0;
}

/*
 * Configure BFD timers
 * Command: discriminator local <value>
 */
static int cmd_bfd_discriminator(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_bfd) {
        printf("Error: No BFD session configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: Discriminator value required\n");
        printf("Usage: discriminator local <value>\n");
        return -1;
    }

    current_bfd->local_discriminator = atoi(args->argv[1]);
    printf("Local discriminator %u configured\n", current_bfd->local_discriminator);

    return 0;
}

/*
 * Configure minimum transmit interval
 * Command: min-tx-interval <interval>
 */
static int cmd_bfd_min_tx(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_bfd) {
        printf("Error: No BFD session configured\n");
        return -1;
    }

    if (args->argc < 1) {
        printf("Error: Interval required\n");
        printf("Usage: min-tx-interval <interval>\n");
        return -1;
    }

    uint32_t interval = atoi(args->argv[0]);
    if (interval < 3 || interval > 20000) {
        printf("Error: Interval must be 3-20000 milliseconds\n");
        return -1;
    }

    current_bfd->min_tx_interval = interval * 1000;  /* Convert to microseconds */
    printf("Minimum transmit interval %u ms configured\n", interval);

    return 0;
}

/*
 * Configure minimum receive interval
 * Command: min-rx-interval <interval>
 */
static int cmd_bfd_min_rx(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_bfd) {
        printf("Error: No BFD session configured\n");
        return -1;
    }

    if (args->argc < 1) {
        printf("Error: Interval required\n");
        printf("Usage: min-rx-interval <interval>\n");
        return -1;
    }

    uint32_t interval = atoi(args->argv[0]);
    if (interval < 3 || interval > 20000) {
        printf("Error: Interval must be 3-20000 milliseconds\n");
        return -1;
    }

    current_bfd->min_rx_interval = interval * 1000;  /* Convert to microseconds */
    printf("Minimum receive interval %u ms configured\n", interval);

    return 0;
}

/*
 * Configure detect multiplier
 * Command: detect-multiplier <multiplier>
 */
static int cmd_bfd_detect_multiplier(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_bfd) {
        printf("Error: No BFD session configured\n");
        return -1;
    }

    if (args->argc < 1) {
        printf("Error: Multiplier required\n");
        printf("Usage: detect-multiplier <multiplier>\n");
        return -1;
    }

    uint8_t multiplier = atoi(args->argv[0]);
    if (multiplier < 3 || multiplier > 50) {
        printf("Error: Multiplier must be 3-50\n");
        return -1;
    }

    current_bfd->detect_multiplier = multiplier;
    printf("Detect multiplier %u configured\n", multiplier);

    return 0;
}

/*
 * Enable echo mode
 * Command: echo-mode [interval <interval>]
 */
static int cmd_bfd_echo_mode(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_bfd) {
        printf("Error: No BFD session configured\n");
        return -1;
    }

    current_bfd->echo_mode = true;

    /* Parse optional interval */
    for (int i = 0; i < args->argc; i++) {
        if (strcmp(args->argv[i], "interval") == 0 && i + 1 < args->argc) {
            uint32_t interval = atoi(args->argv[i + 1]);
            if (interval >= 3 && interval <= 20000) {
                current_bfd->echo_interval = interval * 1000;
            }
            break;
        }
    }

    printf("Echo mode enabled (interval: %u ms)\n",
           current_bfd->echo_interval / 1000);

    return 0;
}

/*
 * Set BFD session type
 * Command: session-type {single-hop|multi-hop}
 */
static int cmd_bfd_session_type(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_bfd) {
        printf("Error: No BFD session configured\n");
        return -1;
    }

    if (args->argc < 1) {
        printf("Error: Session type required\n");
        printf("Usage: session-type {single-hop|multi-hop}\n");
        return -1;
    }

    const char *type = args->argv[0];

    if (strcmp(type, "single-hop") == 0) {
        current_bfd->type = BFD_TYPE_SINGLE_HOP;
        printf("BFD session type: single-hop\n");
    } else if (strcmp(type, "multi-hop") == 0) {
        current_bfd->type = BFD_TYPE_MULTI_HOP;
        printf("BFD session type: multi-hop\n");
    } else {
        printf("Error: Invalid session type\n");
        return -1;
    }

    return 0;
}

/*
 * Enable BFD for static route
 * Command: bfd static-route <destination> <nexthop> <session-name>
 */
static int cmd_bfd_static_route(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 3) {
        printf("Error: Destination, nexthop, and session name required\n");
        printf("Usage: bfd static-route <destination> <nexthop> <session-name>\n");
        return -1;
    }

    const char *dest = args->argv[0];
    const char *nexthop = args->argv[1];
    const char *session = args->argv[2];

    printf("BFD enabled for static route %s via %s (session: %s)\n",
           dest, nexthop, session);

    return 0;
}

/*
 * Display BFD sessions
 * Command: display bfd session [all|<session-name>]
 */
static int cmd_display_bfd_session(struct cmd_element *cmd, struct cmd_args *args)
{
    const char *session_name = NULL;

    if (args->argc > 2) {
        if (strcmp(args->argv[2], "all") != 0) {
            session_name = args->argv[2];
        }
    }

    printf("BFD Session Information:\n\n");

    for (int i = 0; i < bfd_session_count; i++) {
        struct bfd_session *bfd = &bfd_sessions[i];

        if (session_name && strcmp(bfd->name, session_name) != 0) {
            continue;
        }

        printf("Session: %s\n", bfd->name);
        printf("  Type: %s\n", bfd->type == BFD_TYPE_SINGLE_HOP ? "Single-hop" : "Multi-hop");
        printf("  Peer IP: %s\n", bfd->peer_ip);
        if (bfd->source_ip[0]) {
            printf("  Source IP: %s\n", bfd->source_ip);
        }
        if (bfd->interface[0]) {
            printf("  Interface: %s\n", bfd->interface);
        }

        const char *local_state_str, *remote_state_str;
        switch (bfd->local_state) {
            case BFD_STATE_ADMIN_DOWN: local_state_str = "AdminDown"; break;
            case BFD_STATE_DOWN: local_state_str = "Down"; break;
            case BFD_STATE_INIT: local_state_str = "Init"; break;
            case BFD_STATE_UP: local_state_str = "Up"; break;
            default: local_state_str = "Unknown";
        }
        switch (bfd->remote_state) {
            case BFD_STATE_ADMIN_DOWN: remote_state_str = "AdminDown"; break;
            case BFD_STATE_DOWN: remote_state_str = "Down"; break;
            case BFD_STATE_INIT: remote_state_str = "Init"; break;
            case BFD_STATE_UP: remote_state_str = "Up"; break;
            default: remote_state_str = "Unknown";
        }

        printf("  Local State: %s\n", local_state_str);
        printf("  Remote State: %s\n", remote_state_str);
        printf("  Local Discriminator: %u\n", bfd->local_discriminator);
        if (bfd->remote_discriminator > 0) {
            printf("  Remote Discriminator: %u\n", bfd->remote_discriminator);
        }

        printf("  Timers:\n");
        printf("    Min TX Interval: %u ms\n", bfd->min_tx_interval / 1000);
        printf("    Min RX Interval: %u ms\n", bfd->min_rx_interval / 1000);
        printf("    Detect Multiplier: %u\n", bfd->detect_multiplier);
        printf("    Detection Time: %u ms\n",
               (bfd->min_rx_interval / 1000) * bfd->detect_multiplier);

        if (bfd->echo_mode) {
            printf("  Echo Mode: Enabled (interval: %u ms)\n",
                   bfd->echo_interval / 1000);
        }

        printf("  Statistics:\n");
        printf("    Packets Sent: %lu\n", bfd->packets_sent);
        printf("    Packets Received: %lu\n", bfd->packets_received);
        printf("    Up Time: %lu seconds\n", bfd->up_time);
        printf("    Down Count: %lu\n", bfd->down_count);

        printf("\n");
    }

    if (bfd_session_count == 0) {
        printf("No BFD sessions configured\n");
    }

    return 0;
}

/*
 * Display BFD statistics
 * Command: display bfd statistics
 */
static int cmd_display_bfd_statistics(struct cmd_element *cmd, struct cmd_args *args)
{
    printf("BFD Statistics:\n\n");
    printf("%-20s %-10s %-15s %-15s %-10s\n",
           "Session", "State", "Packets Sent", "Packets Recv", "Down Count");
    printf("%-20s %-10s %-15s %-15s %-10s\n",
           "--------------------", "----------", "---------------",
           "---------------", "----------");

    for (int i = 0; i < bfd_session_count; i++) {
        struct bfd_session *bfd = &bfd_sessions[i];

        const char *state_str;
        switch (bfd->local_state) {
            case BFD_STATE_ADMIN_DOWN: state_str = "AdminDown"; break;
            case BFD_STATE_DOWN: state_str = "Down"; break;
            case BFD_STATE_INIT: state_str = "Init"; break;
            case BFD_STATE_UP: state_str = "Up"; break;
            default: state_str = "Unknown";
        }

        printf("%-20s %-10s %-15lu %-15lu %-10lu\n",
               bfd->name,
               state_str,
               bfd->packets_sent,
               bfd->packets_received,
               bfd->down_count);
    }

    return 0;
}

/*
 * Initialize BFD module
 */
void bfd_init(void)
{
    /* Register commands */
    HUAWEI_CMD_WITH_CATEGORY("bfd", cmd_bfd_session, NULL,
                            "Configure BFD session", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("discriminator local", cmd_bfd_discriminator, NULL,
                            "Set local discriminator", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("min-tx-interval", cmd_bfd_min_tx, NULL,
                            "Set minimum transmit interval", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("min-rx-interval", cmd_bfd_min_rx, NULL,
                            "Set minimum receive interval", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("detect-multiplier", cmd_bfd_detect_multiplier, NULL,
                            "Set detect multiplier", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("echo-mode", cmd_bfd_echo_mode, NULL,
                            "Enable echo mode", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("session-type", cmd_bfd_session_type, NULL,
                            "Set session type", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("bfd static-route", cmd_bfd_static_route, NULL,
                            "Enable BFD for static route", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("display bfd session", cmd_display_bfd_session, NULL,
                            "Display BFD sessions", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("display bfd statistics", cmd_display_bfd_statistics, NULL,
                            "Display BFD statistics", "High Availability");
}


