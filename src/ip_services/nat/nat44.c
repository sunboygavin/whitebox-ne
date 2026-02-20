/*
 * NAT44 Support for Huawei VRP Style
 * Copyright (C) 2026 WhiteBox NE Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../frr_core/lib/huawei_cli.h"

struct nat_outbound_rule {
    uint32_t acl_number;
    char interface[64];
    bool enabled;
};

struct nat_server_rule {
    char protocol[8];
    char global_ip[64];
    uint16_t global_port;
    char inside_ip[64];
    uint16_t inside_port;
};

static struct nat_outbound_rule nat_outbound_rules[64];
static int nat_outbound_count = 0;
static struct nat_server_rule nat_servers[64];
static int nat_server_count = 0;

/*
 * Configure NAT outbound
 * Command: nat outbound <acl-number>
 */
static int cmd_nat_outbound(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: ACL number required\n");
        printf("Usage: nat outbound <acl-number>\n");
        return -1;
    }

    uint32_t acl = atoi(args->argv[1]);
    if (nat_outbound_count < 64) {
        nat_outbound_rules[nat_outbound_count].acl_number = acl;
        nat_outbound_rules[nat_outbound_count].enabled = true;
        nat_outbound_count++;
    }

    printf("NAT outbound configured with ACL %u\n", acl);
    printf("Note: Configure iptables MASQUERADE rule\n");
    return 0;
}

/*
 * Configure NAT server (port mapping)
 * Command: nat server protocol tcp global <global-ip> <global-port> inside <inside-ip> <inside-port>
 */
static int cmd_nat_server(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 8) {
        printf("Error: Insufficient arguments\n");
        printf("Usage: nat server protocol tcp global <ip> <port> inside <ip> <port>\n");
        return -1;
    }

    if (nat_server_count < 64) {
        struct nat_server_rule *rule = &nat_servers[nat_server_count++];
        strncpy(rule->protocol, args->argv[2], sizeof(rule->protocol) - 1);
        strncpy(rule->global_ip, args->argv[4], sizeof(rule->global_ip) - 1);
        rule->global_port = atoi(args->argv[5]);
        strncpy(rule->inside_ip, args->argv[7], sizeof(rule->inside_ip) - 1);
        rule->inside_port = atoi(args->argv[8]);
    }

    printf("NAT server configured: %s %s:%s -> %s:%s\n",
           args->argv[2], args->argv[4], args->argv[5],
           args->argv[7], args->argv[8]);
    return 0;
}

/*
 * Display NAT configuration
 * Command: display nat session
 */
static int cmd_display_nat(struct cmd_element *cmd, struct cmd_args *args)
{
    printf("NAT Configuration:\n");
    printf("  Outbound rules: %d\n", nat_outbound_count);
    printf("  Server rules: %d\n", nat_server_count);

    if (nat_server_count > 0) {
        printf("\n  NAT Servers:\n");
        for (int i = 0; i < nat_server_count; i++) {
            printf("    %s %s:%u -> %s:%u\n",
                   nat_servers[i].protocol,
                   nat_servers[i].global_ip, nat_servers[i].global_port,
                   nat_servers[i].inside_ip, nat_servers[i].inside_port);
        }
    }
    return 0;
}

struct cmd_element nat_cmds[] = {
    HUAWEI_CMD_WITH_CATEGORY("nat outbound", cmd_nat_outbound, "ip nat inside source",
                             "Configure NAT outbound", CMD_CAT_IP_SERVICE),
    HUAWEI_CMD_WITH_CATEGORY("nat server", cmd_nat_server, "ip nat inside destination",
                             "Configure NAT server (port mapping)", CMD_CAT_IP_SERVICE),
    HUAWEI_CMD_WITH_CATEGORY("display nat session", cmd_display_nat, "show ip nat translations",
                             "Display NAT sessions", CMD_CAT_IP_SERVICE),
    { .name = NULL }
};

void register_nat_cmds(void) {
    printf("Registering NAT commands...\n");
}
