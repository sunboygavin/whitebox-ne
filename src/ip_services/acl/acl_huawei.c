/*
 * ACL Support for Huawei VRP Style
 * Copyright (C) 2026 WhiteBox NE Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../frr_core/lib/huawei_cli.h"

typedef enum {
    ACL_TYPE_BASIC = 0,    /* 2000-2999 */
    ACL_TYPE_ADVANCED = 1  /* 3000-3999 */
} acl_type_t;

struct acl_rule {
    uint32_t rule_id;
    bool permit;
    char source[64];
    char destination[64];
    char protocol[16];
    uint16_t src_port;
    uint16_t dst_port;
};

struct acl_config {
    uint32_t acl_number;
    acl_type_t type;
    char description[128];
    struct acl_rule rules[128];
    int rule_count;
};

static struct acl_config acls[1000];
static int acl_count = 0;
static struct acl_config *current_acl = NULL;

/*
 * Create or enter ACL
 * Command: acl <acl-number>
 */
static int cmd_acl(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 1) {
        printf("Error: ACL number required\n");
        return -1;
    }

    uint32_t acl_num = atoi(args->argv[0]);
    acl_type_t type;

    if (acl_num >= 2000 && acl_num <= 2999) {
        type = ACL_TYPE_BASIC;
    } else if (acl_num >= 3000 && acl_num <= 3999) {
        type = ACL_TYPE_ADVANCED;
    } else {
        printf("Error: ACL number must be 2000-2999 (basic) or 3000-3999 (advanced)\n");
        return -1;
    }

    /* Find or create ACL */
    current_acl = NULL;
    for (int i = 0; i < acl_count; i++) {
        if (acls[i].acl_number == acl_num) {
            current_acl = &acls[i];
            break;
        }
    }

    if (!current_acl && acl_count < 1000) {
        current_acl = &acls[acl_count++];
        memset(current_acl, 0, sizeof(struct acl_config));
        current_acl->acl_number = acl_num;
        current_acl->type = type;
    }

    printf("Entering ACL %u (%s) configuration\n", acl_num,
           type == ACL_TYPE_BASIC ? "basic" : "advanced");
    return 0;
}

/*
 * Add ACL rule
 * Command: rule [<rule-id>] {permit|deny} [source <ip> <wildcard>] [destination <ip> <wildcard>]
 */
static int cmd_acl_rule(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_acl) {
        printf("Error: No ACL configured\n");
        return -1;
    }

    if (args->argc < 2) {
        printf("Error: Insufficient arguments\n");
        return -1;
    }

    if (current_acl->rule_count >= 128) {
        printf("Error: Maximum rules reached\n");
        return -1;
    }

    struct acl_rule *rule = &current_acl->rules[current_acl->rule_count++];
    memset(rule, 0, sizeof(struct acl_rule));

    int idx = 0;
    /* Parse rule ID if present */
    if (atoi(args->argv[0]) > 0) {
        rule->rule_id = atoi(args->argv[0]);
        idx = 1;
    } else {
        rule->rule_id = current_acl->rule_count * 5;
    }

    /* Parse permit/deny */
    if (strcmp(args->argv[idx], "permit") == 0) {
        rule->permit = true;
    } else if (strcmp(args->argv[idx], "deny") == 0) {
        rule->permit = false;
    } else {
        printf("Error: Expected 'permit' or 'deny'\n");
        current_acl->rule_count--;
        return -1;
    }
    idx++;

    /* Parse source */
    for (int i = idx; i < args->argc; i++) {
        if (strcmp(args->argv[i], "source") == 0 && i + 1 < args->argc) {
            strncpy(rule->source, args->argv[i + 1], sizeof(rule->source) - 1);
            i++;
        } else if (strcmp(args->argv[i], "destination") == 0 && i + 1 < args->argc) {
            strncpy(rule->destination, args->argv[i + 1], sizeof(rule->destination) - 1);
            i++;
        }
    }

    printf("ACL rule %u added\n", rule->rule_id);
    return 0;
}

/*
 * Display ACL
 * Command: display acl [<acl-number>]
 */
static int cmd_display_acl(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc > 0) {
        uint32_t acl_num = atoi(args->argv[0]);
        struct acl_config *acl = NULL;

        for (int i = 0; i < acl_count; i++) {
            if (acls[i].acl_number == acl_num) {
                acl = &acls[i];
                break;
            }
        }

        if (!acl) {
            printf("Error: ACL not found\n");
            return -1;
        }

        printf("ACL %u (%s):\n", acl->acl_number,
               acl->type == ACL_TYPE_BASIC ? "Basic" : "Advanced");
        printf("  Rules: %d\n", acl->rule_count);
        for (int i = 0; i < acl->rule_count; i++) {
            printf("    Rule %u: %s", acl->rules[i].rule_id,
                   acl->rules[i].permit ? "permit" : "deny");
            if (acl->rules[i].source[0]) {
                printf(" source %s", acl->rules[i].source);
            }
            if (acl->rules[i].destination[0]) {
                printf(" destination %s", acl->rules[i].destination);
            }
            printf("\n");
        }
    } else {
        printf("ACL Configuration:\n");
        printf("  Total ACLs: %d\n", acl_count);
        for (int i = 0; i < acl_count; i++) {
            printf("    ACL %u (%s) - %d rules\n",
                   acls[i].acl_number,
                   acls[i].type == ACL_TYPE_BASIC ? "Basic" : "Advanced",
                   acls[i].rule_count);
        }
    }
    return 0;
}

struct cmd_element acl_cmds[] = {
    HUAWEI_CMD_WITH_CATEGORY("acl", cmd_acl, "access-list",
                             "Create or enter ACL configuration", CMD_CAT_IP_SERVICE),
    HUAWEI_CMD_WITH_CATEGORY("rule", cmd_acl_rule, "permit/deny",
                             "Add ACL rule", CMD_CAT_IP_SERVICE),
    HUAWEI_CMD_WITH_CATEGORY("display acl", cmd_display_acl, "show access-list",
                             "Display ACL configuration", CMD_CAT_IP_SERVICE),
    { .name = NULL }
};

void register_acl_cmds(void) {
    printf("Registering ACL commands...\n");
}
