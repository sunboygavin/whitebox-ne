/*
 * AAA Authentication for Huawei VRP Style
 * Copyright (C) 2026 WhiteBox NE Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../frr_core/lib/huawei_cli.h"

struct local_user {
    char username[64];
    char password[128];
    uint8_t privilege_level;
    char service_type[64];
    bool enabled;
};

struct radius_server {
    char address[64];
    uint16_t auth_port;
    uint16_t acct_port;
    char shared_key[128];
};

static struct local_user users[256];
static int user_count = 0;
static struct radius_server radius_servers[8];
static int radius_count = 0;
static bool aaa_enabled = false;

/*
 * Enter AAA configuration
 * Command: aaa
 */
static int cmd_aaa(struct cmd_element *cmd, struct cmd_args *args)
{
    aaa_enabled = true;
    printf("Entering AAA configuration\n");
    printf("[Huawei-aaa]\n");
    return 0;
}

/*
 * Create local user
 * Command: local-user <username> password cipher <password>
 */
static int cmd_local_user(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 4) {
        printf("Error: Username and password required\n");
        printf("Usage: local-user <username> password cipher <password>\n");
        return -1;
    }

    const char *username = args->argv[0];
    const char *password = args->argv[3];

    if (user_count < 256) {
        struct local_user *user = &users[user_count++];
        strncpy(user->username, username, sizeof(user->username) - 1);
        strncpy(user->password, password, sizeof(user->password) - 1);
        user->privilege_level = 0;
        user->enabled = true;
        printf("Local user %s created\n", username);
    } else {
        printf("Error: Maximum users reached\n");
        return -1;
    }

    return 0;
}

/*
 * Set user privilege level
 * Command: local-user <username> privilege level <level>
 */
static int cmd_user_privilege(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 4) {
        printf("Error: Username and level required\n");
        return -1;
    }

    const char *username = args->argv[0];
    uint8_t level = atoi(args->argv[3]);

    if (level > 15) {
        printf("Error: Privilege level must be 0-15\n");
        return -1;
    }

    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            users[i].privilege_level = level;
            printf("User %s privilege level set to %u\n", username, level);
            return 0;
        }
    }

    printf("Error: User not found\n");
    return -1;
}

/*
 * Set user service type
 * Command: local-user <username> service-type {telnet|ssh|http|ftp}
 */
static int cmd_user_service_type(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Username and service type required\n");
        return -1;
    }

    const char *username = args->argv[0];
    const char *service = args->argv[1];

    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            strncpy(users[i].service_type, service, sizeof(users[i].service_type) - 1);
            printf("User %s service type set to %s\n", username, service);
            return 0;
        }
    }

    printf("Error: User not found\n");
    return -1;
}

/*
 * Configure RADIUS server
 * Command: radius-server <address> [auth-port <port>] [acct-port <port>] [key <key>]
 */
static int cmd_radius_server(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: Server address required\n");
        return -1;
    }

    if (radius_count < 8) {
        struct radius_server *server = &radius_servers[radius_count++];
        strncpy(server->address, args->argv[0], sizeof(server->address) - 1);
        server->auth_port = 1812;
        server->acct_port = 1813;

        for (int i = 1; i < args->argc; i++) {
            if (strcmp(args->argv[i], "auth-port") == 0 && i + 1 < args->argc) {
                server->auth_port = atoi(args->argv[++i]);
            } else if (strcmp(args->argv[i], "acct-port") == 0 && i + 1 < args->argc) {
                server->acct_port = atoi(args->argv[++i]);
            } else if (strcmp(args->argv[i], "key") == 0 && i + 1 < args->argc) {
                strncpy(server->shared_key, args->argv[++i], sizeof(server->shared_key) - 1);
            }
        }

        printf("RADIUS server %s configured\n", server->address);
    } else {
        printf("Error: Maximum RADIUS servers reached\n");
        return -1;
    }

    return 0;
}

/*
 * Display AAA configuration
 * Command: display aaa
 */
static int cmd_display_aaa(struct cmd_element *cmd, struct cmd_args *args)
{
    printf("AAA Configuration:\n");
    printf("  Status: %s\n", aaa_enabled ? "Enabled" : "Disabled");
    printf("  Local users: %d\n", user_count);

    for (int i = 0; i < user_count; i++) {
        printf("    User: %s\n", users[i].username);
        printf("      Privilege level: %u\n", users[i].privilege_level);
        printf("      Service type: %s\n", users[i].service_type[0] ? users[i].service_type : "Not set");
        printf("      Status: %s\n", users[i].enabled ? "Enabled" : "Disabled");
    }

    printf("  RADIUS servers: %d\n", radius_count);
    for (int i = 0; i < radius_count; i++) {
        printf("    Server: %s\n", radius_servers[i].address);
        printf("      Auth port: %u\n", radius_servers[i].auth_port);
        printf("      Acct port: %u\n", radius_servers[i].acct_port);
    }

    return 0;
}

struct cmd_element aaa_cmds[] = {
    HUAWEI_CMD_WITH_CATEGORY("aaa", cmd_aaa, "aaa new-model",
                             "Enter AAA configuration", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("local-user password", cmd_local_user, "username",
                             "Create local user", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("local-user privilege", cmd_user_privilege, "username privilege",
                             "Set user privilege level", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("local-user service-type", cmd_user_service_type, "username service",
                             "Set user service type", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("radius-server", cmd_radius_server, "radius server",
                             "Configure RADIUS server", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("display aaa", cmd_display_aaa, "show aaa",
                             "Display AAA configuration", CMD_CAT_SECURITY),
    { .name = NULL }
};

void register_aaa_cmds(void) {
    printf("Registering AAA commands...\n");
}
