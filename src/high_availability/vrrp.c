/*
 * VRRP (Virtual Router Redundancy Protocol) Enhanced Implementation
 * Copyright (C) 2026 WhiteBox NE Team
 *
 * This module provides enhanced VRRP functionality including:
 * - VRRP v2/v3 support
 * - Authentication (simple, MD5)
 * - Preemption control
 * - Track interface/BFD integration
 * - Priority adjustment
 * - Advertisement interval configuration
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include "../frr_core/lib/huawei_cli.h"

/* VRRP version */
typedef enum {
    VRRP_VERSION_2 = 2,
    VRRP_VERSION_3 = 3
} vrrp_version_t;

/* VRRP state */
typedef enum {
    VRRP_STATE_INITIALIZE = 0,
    VRRP_STATE_BACKUP,
    VRRP_STATE_MASTER
} vrrp_state_t;

/* Authentication type */
typedef enum {
    VRRP_AUTH_NONE = 0,
    VRRP_AUTH_SIMPLE,
    VRRP_AUTH_MD5
} vrrp_auth_type_t;

/* Track object */
struct vrrp_track {
    char name[64];
    int priority_reduce;
    bool is_interface;
    bool is_bfd;
};

/* VRRP instance */
struct vrrp_instance {
    uint8_t vrid;                    /* Virtual Router ID (1-255) */
    char interface[64];              /* Interface name */
    vrrp_version_t version;          /* VRRP version */
    vrrp_state_t state;              /* Current state */
    uint8_t priority;                /* Priority (1-254) */
    uint8_t effective_priority;      /* Effective priority after track */
    uint16_t advert_interval;        /* Advertisement interval (centiseconds) */
    bool preempt;                    /* Preemption enabled */
    uint16_t preempt_delay;          /* Preemption delay (seconds) */

    /* Virtual IP addresses */
    char virtual_ips[8][64];
    int virtual_ip_count;

    /* Authentication */
    vrrp_auth_type_t auth_type;
    char auth_key[64];

    /* Track objects */
    struct vrrp_track tracks[16];
    int track_count;

    /* Statistics */
    uint64_t master_transitions;
    uint64_t advert_sent;
    uint64_t advert_received;
    uint64_t priority_zero_sent;
    uint64_t priority_zero_received;

    /* Timers */
    time_t last_advert_time;
    time_t master_down_timer;
};

static struct vrrp_instance vrrp_instances[256];
static int vrrp_instance_count = 0;
static struct vrrp_instance *current_vrrp = NULL;

/*
 * Create or enter VRRP instance
 * Command: vrrp vrid <vrid>
 */
static int cmd_vrrp_vrid(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: VRID required\n");
        printf("Usage: vrrp vrid <vrid>\n");
        return -1;
    }

    uint8_t vrid = atoi(args->argv[1]);
    if (vrid < 1 || vrid > 255) {
        printf("Error: VRID must be 1-255\n");
        return -1;
    }

    /* Find or create VRRP instance */
    current_vrrp = NULL;
    for (int i = 0; i < vrrp_instance_count; i++) {
        if (vrrp_instances[i].vrid == vrid) {
            current_vrrp = &vrrp_instances[i];
            break;
        }
    }

    if (!current_vrrp && vrrp_instance_count < 256) {
        current_vrrp = &vrrp_instances[vrrp_instance_count++];
        memset(current_vrrp, 0, sizeof(struct vrrp_instance));
        current_vrrp->vrid = vrid;
        current_vrrp->version = VRRP_VERSION_2;
        current_vrrp->state = VRRP_STATE_INITIALIZE;
        current_vrrp->priority = 100;
        current_vrrp->effective_priority = 100;
        current_vrrp->advert_interval = 100;  /* 1 second */
        current_vrrp->preempt = true;
        current_vrrp->preempt_delay = 0;
        current_vrrp->auth_type = VRRP_AUTH_NONE;
    }

    if (!current_vrrp) {
        printf("Error: Maximum VRRP instances reached\n");
        return -1;
    }

    printf("Entering VRRP %u configuration\n", vrid);
    printf("[Huawei-vrrp-%u]\n", vrid);

    return 0;
}

/*
 * Set VRRP version
 * Command: version {2|3}
 */
static int cmd_vrrp_version(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_vrrp) {
        printf("Error: No VRRP instance configured\n");
        return -1;
    }

    if (args->argc < 1) {
        printf("Error: Version required\n");
        printf("Usage: version {2|3}\n");
        return -1;
    }

    int version = atoi(args->argv[0]);
    if (version != 2 && version != 3) {
        printf("Error: Version must be 2 or 3\n");
        return -1;
    }

    current_vrrp->version = (vrrp_version_t)version;
    printf("VRRP version %d configured\n", version);

    return 0;
}

/*
 * Set VRRP priority
 * Command: priority <priority>
 */
static int cmd_vrrp_priority(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_vrrp) {
        printf("Error: No VRRP instance configured\n");
        return -1;
    }

    if (args->argc < 1) {
        printf("Error: Priority required\n");
        printf("Usage: priority <priority>\n");
        return -1;
    }

    uint8_t priority = atoi(args->argv[0]);
    if (priority < 1 || priority > 254) {
        printf("Error: Priority must be 1-254\n");
        return -1;
    }

    current_vrrp->priority = priority;
    current_vrrp->effective_priority = priority;
    printf("VRRP priority %u configured\n", priority);

    return 0;
}

/*
 * Add virtual IP address
 * Command: virtual-ip <ip-address>
 */
static int cmd_vrrp_virtual_ip(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_vrrp) {
        printf("Error: No VRRP instance configured\n");
        return -1;
    }

    if (args->argc < 1) {
        printf("Error: IP address required\n");
        printf("Usage: virtual-ip <ip-address>\n");
        return -1;
    }

    if (current_vrrp->virtual_ip_count >= 8) {
        printf("Error: Maximum virtual IPs reached\n");
        return -1;
    }

    const char *ip = args->argv[0];
    strncpy(current_vrrp->virtual_ips[current_vrrp->virtual_ip_count++],
            ip, 63);
    printf("Virtual IP %s configured\n", ip);

    return 0;
}

/*
 * Set advertisement interval
 * Command: timer advertise <interval>
 */
static int cmd_vrrp_timer_advertise(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_vrrp) {
        printf("Error: No VRRP instance configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: Interval required\n");
        printf("Usage: timer advertise <interval>\n");
        return -1;
    }

    uint16_t interval = atoi(args->argv[1]);
    if (interval < 1 || interval > 25500) {
        printf("Error: Interval must be 1-25500 centiseconds\n");
        return -1;
    }

    current_vrrp->advert_interval = interval;
    printf("Advertisement interval %u centiseconds configured\n", interval);

    return 0;
}

/*
 * Configure preemption
 * Command: preempt-mode [timer delay <delay>]
 */
static int cmd_vrrp_preempt(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_vrrp) {
        printf("Error: No VRRP instance configured\n");
        return -1;
    }

    current_vrrp->preempt = true;

    /* Parse optional delay */
    for (int i = 0; i < args->argc; i++) {
        if (strcmp(args->argv[i], "delay") == 0 && i + 1 < args->argc) {
            current_vrrp->preempt_delay = atoi(args->argv[i + 1]);
            printf("Preemption enabled with %u seconds delay\n",
                   current_vrrp->preempt_delay);
            return 0;
        }
    }

    printf("Preemption enabled\n");
    return 0;
}

/*
 * Disable preemption
 * Command: undo preempt-mode
 */
static int cmd_vrrp_undo_preempt(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_vrrp) {
        printf("Error: No VRRP instance configured\n");
        return -1;
    }

    current_vrrp->preempt = false;
    current_vrrp->preempt_delay = 0;
    printf("Preemption disabled\n");

    return 0;
}

/*
 * Configure authentication
 * Command: authentication-mode {simple|md5} <key>
 */
static int cmd_vrrp_authentication(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_vrrp) {
        printf("Error: No VRRP instance configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: Authentication mode and key required\n");
        printf("Usage: authentication-mode {simple|md5} <key>\n");
        return -1;
    }

    const char *mode = args->argv[0];
    const char *key = args->argv[1];

    if (strcmp(mode, "simple") == 0) {
        current_vrrp->auth_type = VRRP_AUTH_SIMPLE;
    } else if (strcmp(mode, "md5") == 0) {
        current_vrrp->auth_type = VRRP_AUTH_MD5;
    } else {
        printf("Error: Invalid authentication mode\n");
        return -1;
    }

    strncpy(current_vrrp->auth_key, key, sizeof(current_vrrp->auth_key) - 1);
    printf("Authentication mode %s configured\n", mode);

    return 0;
}

/*
 * Track interface
 * Command: track interface <interface> [reduced <priority>]
 */
static int cmd_vrrp_track_interface(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_vrrp) {
        printf("Error: No VRRP instance configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: Interface name required\n");
        printf("Usage: track interface <interface> [reduced <priority>]\n");
        return -1;
    }

    if (current_vrrp->track_count >= 16) {
        printf("Error: Maximum track objects reached\n");
        return -1;
    }

    struct vrrp_track *track = &current_vrrp->tracks[current_vrrp->track_count++];
    strncpy(track->name, args->argv[1], sizeof(track->name) - 1);
    track->is_interface = true;
    track->is_bfd = false;
    track->priority_reduce = 10;  /* Default reduction */

    /* Parse optional priority reduction */
    for (int i = 2; i < args->argc; i++) {
        if (strcmp(args->argv[i], "reduced") == 0 && i + 1 < args->argc) {
            track->priority_reduce = atoi(args->argv[i + 1]);
            break;
        }
    }

    printf("Track interface %s configured (priority reduction: %d)\n",
           track->name, track->priority_reduce);

    return 0;
}

/*
 * Track BFD session
 * Command: track bfd-session <session-name> [reduced <priority>]
 */
static int cmd_vrrp_track_bfd(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_vrrp) {
        printf("Error: No VRRP instance configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: BFD session name required\n");
        printf("Usage: track bfd-session <session-name> [reduced <priority>]\n");
        return -1;
    }

    if (current_vrrp->track_count >= 16) {
        printf("Error: Maximum track objects reached\n");
        return -1;
    }

    struct vrrp_track *track = &current_vrrp->tracks[current_vrrp->track_count++];
    strncpy(track->name, args->argv[1], sizeof(track->name) - 1);
    track->is_interface = false;
    track->is_bfd = true;
    track->priority_reduce = 10;  /* Default reduction */

    /* Parse optional priority reduction */
    for (int i = 2; i < args->argc; i++) {
        if (strcmp(args->argv[i], "reduced") == 0 && i + 1 < args->argc) {
            track->priority_reduce = atoi(args->argv[i + 1]);
            break;
        }
    }

    printf("Track BFD session %s configured (priority reduction: %d)\n",
           track->name, track->priority_reduce);

    return 0;
}

/*
 * Display VRRP information
 * Command: display vrrp [brief] [interface <interface>]
 */
static int cmd_display_vrrp(struct cmd_element *cmd, struct cmd_args *args)
{
    bool brief = false;
    const char *interface = NULL;

    /* Parse arguments */
    for (int i = 1; i < args->argc; i++) {
        if (strcmp(args->argv[i], "brief") == 0) {
            brief = true;
        } else if (strcmp(args->argv[i], "interface") == 0 && i + 1 < args->argc) {
            interface = args->argv[++i];
        }
    }

    if (brief) {
        /* Brief display */
        printf("VRRP Brief Information:\n");
        printf("%-6s %-15s %-10s %-10s %-15s\n",
               "VRID", "Interface", "State", "Priority", "Virtual IP");
        printf("%-6s %-15s %-10s %-10s %-15s\n",
               "------", "---------------", "----------", "----------", "---------------");

        for (int i = 0; i < vrrp_instance_count; i++) {
            struct vrrp_instance *vrrp = &vrrp_instances[i];
            if (interface && strcmp(vrrp->interface, interface) != 0) {
                continue;
            }

            const char *state_str;
            switch (vrrp->state) {
                case VRRP_STATE_INITIALIZE: state_str = "Initialize"; break;
                case VRRP_STATE_BACKUP: state_str = "Backup"; break;
                case VRRP_STATE_MASTER: state_str = "Master"; break;
                default: state_str = "Unknown";
            }

            printf("%-6u %-15s %-10s %-10u %-15s\n",
                   vrrp->vrid,
                   vrrp->interface[0] ? vrrp->interface : "-",
                   state_str,
                   vrrp->effective_priority,
                   vrrp->virtual_ip_count > 0 ? vrrp->virtual_ips[0] : "-");
        }
    } else {
        /* Detailed display */
        for (int i = 0; i < vrrp_instance_count; i++) {
            struct vrrp_instance *vrrp = &vrrp_instances[i];
            if (interface && strcmp(vrrp->interface, interface) != 0) {
                continue;
            }

            printf("\nVRRP Instance %u:\n", vrrp->vrid);
            printf("  Interface: %s\n", vrrp->interface[0] ? vrrp->interface : "Not configured");
            printf("  Version: %d\n", vrrp->version);

            const char *state_str;
            switch (vrrp->state) {
                case VRRP_STATE_INITIALIZE: state_str = "Initialize"; break;
                case VRRP_STATE_BACKUP: state_str = "Backup"; break;
                case VRRP_STATE_MASTER: state_str = "Master"; break;
                default: state_str = "Unknown";
            }
            printf("  State: %s\n", state_str);

            printf("  Priority: %u (Effective: %u)\n",
                   vrrp->priority, vrrp->effective_priority);
            printf("  Advertisement Interval: %u centiseconds\n", vrrp->advert_interval);
            printf("  Preemption: %s", vrrp->preempt ? "Enabled" : "Disabled");
            if (vrrp->preempt && vrrp->preempt_delay > 0) {
                printf(" (Delay: %u seconds)", vrrp->preempt_delay);
            }
            printf("\n");

            printf("  Authentication: ");
            switch (vrrp->auth_type) {
                case VRRP_AUTH_NONE: printf("None\n"); break;
                case VRRP_AUTH_SIMPLE: printf("Simple\n"); break;
                case VRRP_AUTH_MD5: printf("MD5\n"); break;
            }

            printf("  Virtual IP Addresses:\n");
            for (int j = 0; j < vrrp->virtual_ip_count; j++) {
                printf("    %s\n", vrrp->virtual_ips[j]);
            }

            if (vrrp->track_count > 0) {
                printf("  Track Objects:\n");
                for (int j = 0; j < vrrp->track_count; j++) {
                    struct vrrp_track *track = &vrrp->tracks[j];
                    printf("    %s %s (Priority reduction: %d)\n",
                           track->is_bfd ? "BFD" : "Interface",
                           track->name,
                           track->priority_reduce);
                }
            }

            printf("  Statistics:\n");
            printf("    Master transitions: %lu\n", vrrp->master_transitions);
            printf("    Advertisements sent: %lu\n", vrrp->advert_sent);
            printf("    Advertisements received: %lu\n", vrrp->advert_received);
        }
    }

    return 0;
}

/*
 * Initialize VRRP module
 */
void vrrp_init(void)
{
    /* Register commands */
    HUAWEI_CMD_WITH_CATEGORY("vrrp vrid", cmd_vrrp_vrid, NULL,
                            "Configure VRRP instance", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("version", cmd_vrrp_version, NULL,
                            "Set VRRP version", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("priority", cmd_vrrp_priority, NULL,
                            "Set VRRP priority", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("virtual-ip", cmd_vrrp_virtual_ip, NULL,
                            "Configure virtual IP address", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("timer advertise", cmd_vrrp_timer_advertise, NULL,
                            "Set advertisement interval", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("preempt-mode", cmd_vrrp_preempt, NULL,
                            "Enable preemption", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("undo preempt-mode", cmd_vrrp_undo_preempt, NULL,
                            "Disable preemption", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("authentication-mode", cmd_vrrp_authentication, NULL,
                            "Configure authentication", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("track interface", cmd_vrrp_track_interface, NULL,
                            "Track interface status", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("track bfd-session", cmd_vrrp_track_bfd, NULL,
                            "Track BFD session", "High Availability");
    HUAWEI_CMD_WITH_CATEGORY("display vrrp", cmd_display_vrrp, NULL,
                            "Display VRRP information", "High Availability");
}



