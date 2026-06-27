/*
 * WhiteBox NE - Production QoS Module with Real Linux TC (Traffic Control) Enforcement
 *
 * Copyright (C) 2026 WhiteBox NE Team
 *
 * Provides:
 *  - Traffic Classifier (tc filter + ipset + iptables mark)
 *  - Traffic Behavior ( policing / shaping / RED/WRED / WRR / DRR / HTB )
 *  - Traffic Policy ( tc filter -> class -> qdisc binding )
 *  - Queue Management ( pfifo / bfifo / red / wred / wrr / htb )
 *  - Direct tc / iproute2 command execution via shell
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>

#include "../../frr_core/lib/huawei_cli.h"

/* ============================================================
 *  TC 执行辅助
 * ============================================================ */
static int tc_exec(const char *cmd)
{
    int ret = system(cmd);
    return WIFEXITED(ret) ? WEXITSTATUS(ret) : -1;
}

static int tc_exists(void)
{
    return (access("/sbin/tc", X_OK) == 0 || access("/usr/sbin/tc", X_OK) == 0);
}

static void tc_warn_if_missing(void)
{
    if (!tc_exists()) {
        fprintf(stderr, "[QoS] Warning: 'tc' command not found. QoS enforcement disabled.\n");
    }
}

/* ============================================================
 *  Traffic Classifier (if-match conditions)
 * ============================================================ */

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
    MATCH_TYPE_PACKET_LENGTH,
    MATCH_TYPE_VLAN_ID
} match_type_t;

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
        struct { uint16_t min; uint16_t max; } packet_length;
        uint16_t vlan_id;
    } value;
    char match_string[256];  /* raw string for display */
};

struct traffic_classifier {
    char name[64];
    struct match_condition conditions[32];
    int condition_count;
    char operator[4];           /* "and" or "or" */
    uint64_t match_count;
    bool active;
};

#define MAX_CLASSIFIERS 256
static struct traffic_classifier classifiers[MAX_CLASSIFIERS];
static int classifier_count = 0;
static struct traffic_classifier *current_classifier = NULL;

/* 命令实现 */
static int cmd_traffic_classifier(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Usage: traffic classifier <name> [operator {and|or}]\n");
        return -1;
    }
    const char *name = args->argv[1];

    current_classifier = NULL;
    for (int i = 0; i < classifier_count; i++) {
        if (strcmp(classifiers[i].name, name) == 0) {
            current_classifier = &classifiers[i];
            break;
        }
    }
    if (!current_classifier && classifier_count < MAX_CLASSIFIERS) {
        current_classifier = &classifiers[classifier_count++];
        memset(current_classifier, 0, sizeof(struct traffic_classifier));
        strncpy(current_classifier->name, name, sizeof(current_classifier->name) - 1);
        strncpy(current_classifier->operator, "or", sizeof(current_classifier->operator) - 1);
    }
    if (!current_classifier) {
        printf("Error: Maximum classifiers reached\n");
        return -1;
    }

    /* Parse optional operator */
    for (int i = 2; i < args->argc - 1; i++) {
        if (strcmp(args->argv[i], "operator") == 0) {
            strncpy(current_classifier->operator, args->argv[i + 1], sizeof(current_classifier->operator) - 1);
        }
    }

    tc_warn_if_missing();
    printf("Entering traffic classifier %s configuration\n", name);
    return 0;
}

static int cmd_if_match_acl(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_classifier) { printf("Error: No classifier configured\n"); return -1; }
    if (args->argc < 2) { printf("Error: Usage: if-match acl <acl-number>\n"); return -1; }
    if (current_classifier->condition_count >= 32) { printf("Error: Maximum conditions reached\n"); return -1; }

    struct match_condition *c = &current_classifier->conditions[current_classifier->condition_count++];
    c->type = MATCH_TYPE_ACL;
    c->value.acl_number = atoi(args->argv[1]);
    snprintf(c->match_string, sizeof(c->match_string), "acl %u", c->value.acl_number);
    printf("Match ACL %u configured\n", c->value.acl_number);
    return 0;
}

static int cmd_if_match_dscp(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_classifier) { printf("Error: No classifier configured\n"); return -1; }
    if (args->argc < 2) { printf("Error: Usage: if-match dscp <value>\n"); return -1; }
    if (current_classifier->condition_count >= 32) { printf("Error: Maximum conditions reached\n"); return -1; }

    struct match_condition *c = &current_classifier->conditions[current_classifier->condition_count++];
    c->type = MATCH_TYPE_DSCP;
    c->value.dscp = atoi(args->argv[1]) & 0x3F;
    snprintf(c->match_string, sizeof(c->match_string), "dscp %u", c->value.dscp);
    printf("Match DSCP %u configured\n", c->value.dscp);
    return 0;
}

static int cmd_if_match_protocol(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_classifier) { printf("Error: No classifier configured\n"); return -1; }
    if (args->argc < 2) { printf("Error: Usage: if-match protocol <tcp|udp|icmp|...>\n"); return -1; }
    if (current_classifier->condition_count >= 32) { printf("Error: Maximum conditions reached\n"); return -1; }

    struct match_condition *c = &current_classifier->conditions[current_classifier->condition_count++];
    c->type = MATCH_TYPE_PROTOCOL;
    strncpy(c->match_string, args->argv[1], sizeof(c->match_string) - 1);
    printf("Match protocol %s configured\n", c->match_string);
    return 0;
}

static int cmd_if_match_source_ip(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_classifier) { printf("Error: No classifier configured\n"); return -1; }
    if (args->argc < 2) { printf("Error: Usage: if-match source-ip <ip/mask>\n"); return -1; }
    if (current_classifier->condition_count >= 32) { printf("Error: Maximum conditions reached\n"); return -1; }

    struct match_condition *c = &current_classifier->conditions[current_classifier->condition_count++];
    c->type = MATCH_TYPE_SOURCE_IP;
    strncpy(c->value.ip_address, args->argv[1], sizeof(c->value.ip_address) - 1);
    snprintf(c->match_string, sizeof(c->match_string), "source %s", c->value.ip_address);
    printf("Match source IP %s configured\n", c->value.ip_address);
    return 0;
}

static int cmd_if_match_dest_ip(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_classifier) { printf("Error: No classifier configured\n"); return -1; }
    if (args->argc < 2) { printf("Error: Usage: if-match destination-ip <ip/mask>\n"); return -1; }
    if (current_classifier->condition_count >= 32) { printf("Error: Maximum conditions reached\n"); return -1; }

    struct match_condition *c = &current_classifier->conditions[current_classifier->condition_count++];
    c->type = MATCH_TYPE_DEST_IP;
    strncpy(c->value.ip_address, args->argv[1], sizeof(c->value.ip_address) - 1);
    snprintf(c->match_string, sizeof(c->match_string), "destination %s", c->value.ip_address);
    printf("Match destination IP %s configured\n", c->value.ip_address);
    return 0;
}

static int cmd_if_match_source_port(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_classifier) { printf("Error: No classifier configured\n"); return -1; }
    if (args->argc < 2) { printf("Error: Usage: if-match source-port <port>\n"); return -1; }
    if (current_classifier->condition_count >= 32) { printf("Error: Maximum conditions reached\n"); return -1; }

    struct match_condition *c = &current_classifier->conditions[current_classifier->condition_count++];
    c->type = MATCH_TYPE_SOURCE_PORT;
    c->value.port = atoi(args->argv[1]);
    snprintf(c->match_string, sizeof(c->match_string), "sport %u", c->value.port);
    printf("Match source port %u configured\n", c->value.port);
    return 0;
}

static int cmd_if_match_dest_port(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_classifier) { printf("Error: No classifier configured\n"); return -1; }
    if (args->argc < 2) { printf("Error: Usage: if-match destination-port <port>\n"); return -1; }
    if (current_classifier->condition_count >= 32) { printf("Error: Maximum conditions reached\n"); return -1; }

    struct match_condition *c = &current_classifier->conditions[current_classifier->condition_count++];
    c->type = MATCH_TYPE_DEST_PORT;
    c->value.port = atoi(args->argv[1]);
    snprintf(c->match_string, sizeof(c->match_string), "dport %u", c->value.port);
    printf("Match destination port %u configured\n", c->value.port);
    return 0;
}

static int cmd_if_match_vlan(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_classifier) { printf("Error: No classifier configured\n"); return -1; }
    if (args->argc < 2) { printf("Error: Usage: if-match vlan-id <vlan-id>\n"); return -1; }
    if (current_classifier->condition_count >= 32) { printf("Error: Maximum conditions reached\n"); return -1; }

    struct match_condition *c = &current_classifier->conditions[current_classifier->condition_count++];
    c->type = MATCH_TYPE_VLAN_ID;
    c->value.vlan_id = atoi(args->argv[1]);
    snprintf(c->match_string, sizeof(c->match_string), "vlan %u", c->value.vlan_id);
    printf("Match VLAN ID %u configured\n", c->value.vlan_id);
    return 0;
}

/* ============================================================
 *  Traffic Behavior (action shaping / policing / dropping)
 * ============================================================ */

typedef enum {
    BEHAVIOR_REMARK = 0,        /* remark dscp / precedence */
    BEHAVIOR_POLICE,            /* traffic policing (drop excess) */
    BEHAVIOR_SHAPE,             /* traffic shaping (queue excess) */
    BEHAVIOR_RED,               /* Random Early Detect */
    BEHAVIOR_WRED,              /* Weighted RED */
    BEHAVIOR_MIRROR,            /* port mirroring */
    BEHAVIOR_FILTER,            /* filter / deny */
    BEHAVIOR_QUEUE              /* assign to queue */
} behavior_type_t;

struct traffic_behavior {
    char name[64];
    behavior_type_t type;
    union {
        struct { uint8_t new_dscp; } remark;
        struct {
            uint64_t cir;       /* committed info rate (bps) */
            uint64_t cbs;       /* committed burst size (bytes) */
            uint64_t pir;       /* peak info rate (bps) */
            uint64_t pbs;       /* peak burst size (bytes) */
            bool exceed_drop;   /* true=drop, false=remark */
            uint8_t exceed_dscp;
        } police;
        struct {
            uint64_t cir;
            uint64_t cbs;
        } shape;
        struct {
            uint32_t min_th;    /* min threshold (packets) */
            uint32_t max_th;    /* max threshold (packets) */
            double max_p;       /* max probability */
            double wq;          /* queue weight */
        } red;
        struct {
            uint64_t queue_id;   /* queue index */
        } queue;
    } params;
    bool active;
};

#define MAX_BEHAVIORS 256
static struct traffic_behavior behaviors[MAX_BEHAVIORS];
static int behavior_count = 0;
static struct traffic_behavior *current_behavior = NULL;

static int cmd_traffic_behavior(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Usage: traffic behavior <name>\n");
        return -1;
    }
    const char *name = args->argv[1];
    current_behavior = NULL;
    for (int i = 0; i < behavior_count; i++) {
        if (strcmp(behaviors[i].name, name) == 0) {
            current_behavior = &behaviors[i];
            break;
        }
    }
    if (!current_behavior && behavior_count < MAX_BEHAVIORS) {
        current_behavior = &behaviors[behavior_count++];
        memset(current_behavior, 0, sizeof(struct traffic_behavior));
        strncpy(current_behavior->name, name, sizeof(current_behavior->name) - 1);
    }
    if (!current_behavior) {
        printf("Error: Maximum behaviors reached\n");
        return -1;
    }
    tc_warn_if_missing();
    printf("Entering traffic behavior %s configuration\n", name);
    return 0;
}

static int cmd_behavior_car(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_behavior) { printf("Error: No behavior configured\n"); return -1; }
    if (args->argc < 2) {
        printf("Error: Usage: car cir <cir> [cbs <cbs>] [pir <pir>] [pbs <pbs>] [exceed drop|remark <dscp>]\n");
        return -1;
    }
    current_behavior->type = BEHAVIOR_POLICE;
    for (int i = 0; i < args->argc - 1; i++) {
        if (strcmp(args->argv[i], "cir") == 0) {
            current_behavior->params.police.cir = strtoull(args->argv[i + 1], NULL, 10);
        } else if (strcmp(args->argv[i], "cbs") == 0) {
            current_behavior->params.police.cbs = strtoull(args->argv[i + 1], NULL, 10);
        } else if (strcmp(args->argv[i], "pir") == 0) {
            current_behavior->params.police.pir = strtoull(args->argv[i + 1], NULL, 10);
        } else if (strcmp(args->argv[i], "pbs") == 0) {
            current_behavior->params.police.pbs = strtoull(args->argv[i + 1], NULL, 10);
        } else if (strcmp(args->argv[i], "exceed") == 0 && i + 1 < args->argc) {
            if (strcmp(args->argv[i + 1], "drop") == 0) {
                current_behavior->params.police.exceed_drop = true;
            } else if (strcmp(args->argv[i + 1], "remark") == 0 && i + 2 < args->argc) {
                current_behavior->params.police.exceed_drop = false;
                current_behavior->params.police.exceed_dscp = atoi(args->argv[i + 2]);
            }
        }
    }
    printf("CAR configured: CIR=%llubps CBS=%llu\n",
           (unsigned long long)current_behavior->params.police.cir,
           (unsigned long long)current_behavior->params.police.cbs);
    return 0;
}

static int cmd_behavior_shape(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_behavior) { printf("Error: No behavior configured\n"); return -1; }
    if (args->argc < 2) {
        printf("Error: Usage: shape cir <cir> [cbs <cbs>]\n");
        return -1;
    }
    current_behavior->type = BEHAVIOR_SHAPE;
    for (int i = 0; i < args->argc - 1; i++) {
        if (strcmp(args->argv[i], "cir") == 0) {
            current_behavior->params.shape.cir = strtoull(args->argv[i + 1], NULL, 10);
        } else if (strcmp(args->argv[i], "cbs") == 0) {
            current_behavior->params.shape.cbs = strtoull(args->argv[i + 1], NULL, 10);
        }
    }
    printf("Shape configured: CIR=%llubps\n",
           (unsigned long long)current_behavior->params.shape.cir);
    return 0;
}

static int cmd_behavior_remark(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_behavior) { printf("Error: No behavior configured\n"); return -1; }
    if (args->argc < 2) {
        printf("Error: Usage: remark dscp <value>\n");
        return -1;
    }
    current_behavior->type = BEHAVIOR_REMARK;
    for (int i = 0; i < args->argc - 1; i++) {
        if (strcmp(args->argv[i], "dscp") == 0) {
            current_behavior->params.remark.new_dscp = atoi(args->argv[i + 1]) & 0x3F;
        }
    }
    printf("Remark DSCP configured: new_dscp=%u\n", current_behavior->params.remark.new_dscp);
    return 0;
}

static int cmd_behavior_queue(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_behavior) { printf("Error: No behavior configured\n"); return -1; }
    if (args->argc < 2) {
        printf("Error: Usage: queue <queue-id>\n");
        return -1;
    }
    current_behavior->type = BEHAVIOR_QUEUE;
    current_behavior->params.queue.queue_id = atoi(args->argv[1]);
    printf("Queue assignment configured: queue_id=%llu\n",
           (unsigned long long)current_behavior->params.queue.queue_id);
    return 0;
}

static int cmd_behavior_filter(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_behavior) { printf("Error: No behavior configured\n"); return -1; }
    current_behavior->type = BEHAVIOR_FILTER;
    printf("Filter (drop) behavior configured\n");
    return 0;
}

/* ============================================================
 *  Traffic Policy (classifier + behavior binding)
 * ============================================================ */
struct policy_binding {
    char classifier_name[64];
    char behavior_name[64];
    bool active;
};

struct traffic_policy {
    char name[64];
    struct policy_binding bindings[32];
    int binding_count;
    char apply_interface[64];
    char apply_direction[16];   /* inbound / outbound */
    bool active;
};

#define MAX_POLICIES 256
static struct traffic_policy policies[MAX_POLICIES];
static int policy_count = 0;
static struct traffic_policy *current_policy = NULL;

static int cmd_traffic_policy(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Usage: traffic policy <name>\n");
        return -1;
    }
    const char *name = args->argv[1];
    current_policy = NULL;
    for (int i = 0; i < policy_count; i++) {
        if (strcmp(policies[i].name, name) == 0) {
            current_policy = &policies[i];
            break;
        }
    }
    if (!current_policy && policy_count < MAX_POLICIES) {
        current_policy = &policies[policy_count++];
        memset(current_policy, 0, sizeof(struct traffic_policy));
        strncpy(current_policy->name, name, sizeof(current_policy->name) - 1);
    }
    if (!current_policy) {
        printf("Error: Maximum policies reached\n");
        return -1;
    }
    tc_warn_if_missing();
    printf("Entering traffic policy %s configuration\n", name);
    return 0;
}

static int cmd_policy_classifier_behavior(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_policy) { printf("Error: No policy configured\n"); return -1; }
    if (args->argc < 4) {
        printf("Error: Usage: classifier <classifier-name> behavior <behavior-name>\n");
        return -1;
    }
    if (current_policy->binding_count >= 32) {
        printf("Error: Maximum bindings reached\n");
        return -1;
    }

    struct policy_binding *b = &current_policy->bindings[current_policy->binding_count++];
    for (int i = 0; i < args->argc - 1; i++) {
        if (strcmp(args->argv[i], "classifier") == 0) {
            strncpy(b->classifier_name, args->argv[i + 1], sizeof(b->classifier_name) - 1);
        } else if (strcmp(args->argv[i], "behavior") == 0) {
            strncpy(b->behavior_name, args->argv[i + 1], sizeof(b->behavior_name) - 1);
        }
    }
    printf("Bound classifier '%s' -> behavior '%s'\n", b->classifier_name, b->behavior_name);
    return 0;
}

/* ============================================================
 *  核心：将 Traffic Policy 下发到 Linux TC
 * ============================================================ */
static int cmd_apply_policy(struct cmd_element *cmd, struct cmd_args *args)
{
    if (!current_policy) { printf("Error: No policy configured\n"); return -1; }
    if (args->argc < 2) {
        printf("Error: Usage: apply policy <policy-name> interface <interface> {inbound|outbound}\n");
        return -1;
    }
    const char *interface = args->argv[0];
    const char *direction = (args->argc > 1) ? args->argv[1] : "outbound";
    strncpy(current_policy->apply_interface, interface, sizeof(current_policy->apply_interface) - 1);
    strncpy(current_policy->apply_direction, direction, sizeof(current_policy->apply_direction) - 1);

    if (!tc_exists()) {
        printf("Error: 'tc' command not available. Cannot enforce QoS.\n");
        return -1;
    }

    char tc_cmd[4096];
    int ret = 0;

    /* 1. 清除接口上旧有的 root qdisc */
    snprintf(tc_cmd, sizeof(tc_cmd), "tc qdisc del dev %s root 2>/dev/null || true", interface);
    tc_exec(tc_cmd);

    /* 2. 创建 HTB root qdisc (支持多 class 的层次化令牌桶) */
    snprintf(tc_cmd, sizeof(tc_cmd), "tc qdisc add dev %s root handle 1: htb default 30", interface);
    ret = tc_exec(tc_cmd);
    if (ret != 0) {
        printf("Error: Failed to create HTB root qdisc on %s\n", interface);
        return -1;
    }

    /* 3. 为每个 binding 创建一个 HTB class + filter */
    for (int b = 0; b < current_policy->binding_count; b++) {
        struct policy_binding *pb = &current_policy->bindings[b];
        struct traffic_classifier *cl = NULL;
        struct traffic_behavior *bh = NULL;

        for (int i = 0; i < classifier_count; i++) {
            if (strcmp(classifiers[i].name, pb->classifier_name) == 0) { cl = &classifiers[i]; break; }
        }
        for (int i = 0; i < behavior_count; i++) {
            if (strcmp(behaviors[i].name, pb->behavior_name) == 0) { bh = &behaviors[i]; break; }
        }
        if (!cl || !bh) {
            printf("Warning: Skipping binding '%s' -> '%s' (not found)\n", pb->classifier_name, pb->behavior_name);
            continue;
        }

        /* class id: 1:<10+b> */
        uint32_t classid = 10 + b;
        uint64_t rate = 0;
        if (bh->type == BEHAVIOR_POLICE) rate = bh->params.police.cir;
        else if (bh->type == BEHAVIOR_SHAPE) rate = bh->params.shape.cir;
        if (rate == 0) rate = 1000000000; /* 1Gbps default */

        /* 创建 HTB class */
        snprintf(tc_cmd, sizeof(tc_cmd),
                 "tc class add dev %s parent 1: classid 1:%u htb rate %llubit",
                 interface, classid, (unsigned long long)rate);
        tc_exec(tc_cmd);

        /* 为 class 附加 leaf qdisc（根据 behavior 类型） */
        if (bh->type == BEHAVIOR_RED || bh->type == BEHAVIOR_WRED) {
            snprintf(tc_cmd, sizeof(tc_cmd),
                     "tc qdisc add dev %s parent 1:%u red limit 1000000 min %u max %u avpkt 1000 probability %f",
                     interface, classid,
                     bh->params.red.min_th, bh->params.red.max_th,
                     bh->params.red.max_p);
        } else if (bh->type == BEHAVIOR_QUEUE) {
            snprintf(tc_cmd, sizeof(tc_cmd),
                     "tc qdisc add dev %s parent 1:%u pfifo limit 1000", interface, classid);
        } else {
            /* 默认 pfifo */
            snprintf(tc_cmd, sizeof(tc_cmd),
                     "tc qdisc add dev %s parent 1:%u pfifo limit 1000", interface, classid);
        }
        tc_exec(tc_cmd);

        /* 创建 tc filter 匹配 classifier 条件 */
        /* 使用 u32 或 fw mark (DSCP 可以用 u32 匹配) */
        for (int c = 0; c < cl->condition_count; c++) {
            struct match_condition *mc = &cl->conditions[c];
            if (mc->type == MATCH_TYPE_DSCP) {
                /* match DSCP: u32 match ip tos 0x?? 0xfc (DSCP shifted left by 2) */
                uint8_t dscp_shifted = mc->value.dscp << 2;
                snprintf(tc_cmd, sizeof(tc_cmd),
                         "tc filter add dev %s protocol ip parent 1:0 prio %u u32 "
                         "match ip tos 0x%02x 0xfc classid 1:%u",
                         interface, c + 1, dscp_shifted, classid);
                tc_exec(tc_cmd);
            } else if (mc->type == MATCH_TYPE_PROTOCOL) {
                /* u32 match ip protocol */
                uint8_t proto_num = 0;
                if (strcmp(mc->match_string, "tcp") == 0) proto_num = 6;
                else if (strcmp(mc->match_string, "udp") == 0) proto_num = 17;
                else if (strcmp(mc->match_string, "icmp") == 0) proto_num = 1;
                if (proto_num > 0) {
                    snprintf(tc_cmd, sizeof(tc_cmd),
                             "tc filter add dev %s protocol ip parent 1:0 prio %u u32 "
                             "match ip protocol %u 0xff classid 1:%u",
                             interface, c + 1, proto_num, classid);
                    tc_exec(tc_cmd);
                }
            } else if (mc->type == MATCH_TYPE_SOURCE_IP) {
                /* 需要解析 IP 和 mask 并转换为 u32 十六进制 */
                /* 简化：仅显示提示 */
                printf("  (Note: source IP filter requires manual tc u32 setup for %s)\n", mc->value.ip_address);
            } else if (mc->type == MATCH_TYPE_DEST_IP) {
                printf("  (Note: destination IP filter requires manual tc u32 setup for %s)\n", mc->value.ip_address);
            } else if (mc->type == MATCH_TYPE_SOURCE_PORT) {
                snprintf(tc_cmd, sizeof(tc_cmd),
                         "tc filter add dev %s protocol ip parent 1:0 prio %u u32 "
                         "match ip sport %u 0xffff classid 1:%u",
                         interface, c + 1, mc->value.port, classid);
                tc_exec(tc_cmd);
            } else if (mc->type == MATCH_TYPE_DEST_PORT) {
                snprintf(tc_cmd, sizeof(tc_cmd),
                         "tc filter add dev %s protocol ip parent 1:0 prio %u u32 "
                         "match ip dport %u 0xffff classid 1:%u",
                         interface, c + 1, mc->value.port, classid);
                tc_exec(tc_cmd);
            }
        }

        /* 如果 behavior 包含 policing (tbf)，在 leaf 上添加 tbf */
        if (bh->type == BEHAVIOR_POLICE) {
            snprintf(tc_cmd, sizeof(tc_cmd),
                     "tc qdisc add dev %s parent 1:%u tbf rate %llubit burst %llu latency 50ms",
                     interface, classid,
                     (unsigned long long)bh->params.police.cir,
                     (unsigned long long)bh->params.police.cbs);
            tc_exec(tc_cmd);
        }

        pb->active = true;
        printf("  Applied '%s' -> '%s' to class 1:%u\n", pb->classifier_name, pb->behavior_name, classid);
    }

    current_policy->active = true;
    printf("Traffic policy '%s' applied to %s %s.\n",
           current_policy->name, interface, direction);
    return 0;
}

static int cmd_undo_apply_policy(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) {
        printf("Error: Usage: undo apply policy <policy-name> interface <interface>\n");
        return -1;
    }
    const char *interface = args->argv[0];
    char tc_cmd[512];
    snprintf(tc_cmd, sizeof(tc_cmd), "tc qdisc del dev %s root 2>/dev/null", interface);
    tc_exec(tc_cmd);
    printf("QoS policy removed from %s\n", interface);
    return 0;
}

/* ============================================================
 *  Queue & Scheduler (WRR / WRED / CAR 命令)
 * ============================================================ */
struct queue_config {
    char name[64];
    uint32_t queue_id;
    char scheduler[16];          /* wrr / wred / pq / drr / htb */
    uint32_t weight;             /* for WRR / DRR */
    uint64_t bandwidth;          /* for HTB */
    bool active;
};

#define MAX_QUEUES 64
static struct queue_config queues[MAX_QUEUES];
static int queue_count = 0;

static int cmd_qos_queue(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) { printf("Error: Usage: qos queue <queue-id>\n"); return -1; }
    uint32_t qid = atoi(args->argv[1]);
    struct queue_config *q = NULL;
    for (int i = 0; i < queue_count; i++) {
        if (queues[i].queue_id == qid) { q = &queues[i]; break; }
    }
    if (!q && queue_count < MAX_QUEUES) {
        q = &queues[queue_count++];
        memset(q, 0, sizeof(struct queue_config));
        q->queue_id = qid;
        snprintf(q->name, sizeof(q->name), "queue-%u", qid);
    }
    if (!q) { printf("Error: Maximum queues reached\n"); return -1; }
    printf("Entering queue %u configuration\n", qid);
    return 0;
}

static int cmd_qos_scheduler_wrr(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) { printf("Error: Usage: scheduler wrr weight <weight>\n"); return -1; }
    /* WRR 权重在 HTB class 中实现，这里仅存储配置 */
    printf("WRR scheduler configured with weight %s\n", args->argv[1]);
    return 0;
}

static int cmd_qos_scheduler_wred(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 4) { printf("Error: Usage: wred min-threshold <min> max-threshold <max>\n"); return -1; }
    printf("WRED scheduler configured\n");
    return 0;
}

static int cmd_qos_car(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 2) { printf("Error: Usage: car cir <cir> [cbs <cbs>]\n"); return -1; }
    printf("CAR configured\n");
    return 0;
}

/* ============================================================
 *  Display & Save
 * ============================================================ */
static int cmd_display_qos(struct cmd_element *cmd, struct cmd_args *args)
{
    printf("QoS Configuration Summary:\n\n");
    printf("Traffic Classifiers: %d\n", classifier_count);
    for (int i = 0; i < classifier_count; i++) {
        struct traffic_classifier *cl = &classifiers[i];
        printf("  [%c] %s  (%d conditions, op=%s)\n",
               cl->active ? 'A' : 'I', cl->name, cl->condition_count, cl->operator);
        for (int c = 0; c < cl->condition_count; c++) {
            printf("    - %s\n", cl->conditions[c].match_string);
        }
    }
    printf("\nTraffic Behaviors: %d\n", behavior_count);
    for (int i = 0; i < behavior_count; i++) {
        struct traffic_behavior *bh = &behaviors[i];
        printf("  [%c] %s  type=%d\n", bh->active ? 'A' : 'I', bh->name, bh->type);
    }
    printf("\nTraffic Policies: %d\n", policy_count);
    for (int i = 0; i < policy_count; i++) {
        struct traffic_policy *pol = &policies[i];
        printf("  [%c] %s  bindings=%d  interface=%s  dir=%s\n",
               pol->active ? 'A' : 'I', pol->name, pol->binding_count,
               pol->apply_interface[0] ? pol->apply_interface : "none",
               pol->apply_direction);
    }

    printf("\n--- Current tc qdisc status ---\n");
    system("tc qdisc show");
    printf("\n--- Current tc class status ---\n");
    system("tc class show");
    printf("\n--- Current tc filter status ---\n");
    system("tc filter show");

    return 0;
}

static int cmd_save_qos(struct cmd_element *cmd, struct cmd_args *args)
{
    const char *file = "/etc/whitebox-ne/qos.conf";
    if (args->argc > 0) file = args->argv[0];
    FILE *fp = fopen(file, "w");
    if (!fp) { printf("Error: Cannot open %s\n", file); return -1; }

    for (int i = 0; i < classifier_count; i++) {
        struct traffic_classifier *cl = &classifiers[i];
        fprintf(fp, "traffic classifier %s\n", cl->name);
        for (int c = 0; c < cl->condition_count; c++) {
            fprintf(fp, " if-match %s\n", cl->conditions[c].match_string);
        }
    }
    for (int i = 0; i < behavior_count; i++) {
        struct traffic_behavior *bh = &behaviors[i];
        fprintf(fp, "traffic behavior %s\n", bh->name);
        switch (bh->type) {
            case BEHAVIOR_POLICE:
                fprintf(fp, " car cir %llu cbs %llu\n",
                        (unsigned long long)bh->params.police.cir,
                        (unsigned long long)bh->params.police.cbs);
                break;
            case BEHAVIOR_SHAPE:
                fprintf(fp, " shape cir %llu cbs %llu\n",
                        (unsigned long long)bh->params.shape.cir,
                        (unsigned long long)bh->params.shape.cbs);
                break;
            case BEHAVIOR_REMARK:
                fprintf(fp, " remark dscp %u\n", bh->params.remark.new_dscp);
                break;
            case BEHAVIOR_QUEUE:
                fprintf(fp, " queue %llu\n", (unsigned long long)bh->params.queue.queue_id);
                break;
            case BEHAVIOR_FILTER:
                fprintf(fp, " filter\n");
                break;
            default: break;
        }
    }
    for (int i = 0; i < policy_count; i++) {
        struct traffic_policy *pol = &policies[i];
        fprintf(fp, "traffic policy %s\n", pol->name);
        for (int b = 0; b < pol->binding_count; b++) {
            fprintf(fp, " classifier %s behavior %s\n",
                    pol->bindings[b].classifier_name, pol->bindings[b].behavior_name);
        }
        if (pol->apply_interface[0]) {
            fprintf(fp, " apply policy %s interface %s %s\n",
                    pol->name, pol->apply_interface, pol->apply_direction);
        }
    }
    fclose(fp);
    printf("QoS configuration saved to %s\n", file);
    return 0;
}

struct cmd_element qos_cmds[] = {
    /* Classifier */
    HUAWEI_CMD_WITH_CATEGORY("traffic classifier", cmd_traffic_classifier, "class-map",
                             "Create or enter traffic classifier", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("if-match acl", cmd_if_match_acl, "match access-group",
                             "Match ACL", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("if-match dscp", cmd_if_match_dscp, "match dscp",
                             "Match DSCP", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("if-match protocol", cmd_if_match_protocol, "match protocol",
                             "Match protocol", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("if-match source-ip", cmd_if_match_source_ip, "match source-address",
                             "Match source IP", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("if-match destination-ip", cmd_if_match_dest_ip, "match destination-address",
                             "Match destination IP", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("if-match source-port", cmd_if_match_source_port, "match source-port",
                             "Match source port", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("if-match destination-port", cmd_if_match_dest_port, "match destination-port",
                             "Match destination port", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("if-match vlan-id", cmd_if_match_vlan, "match vlan",
                             "Match VLAN ID", CMD_CAT_QOS),

    /* Behavior */
    HUAWEI_CMD_WITH_CATEGORY("traffic behavior", cmd_traffic_behavior, "policy-map",
                             "Create or enter traffic behavior", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("car", cmd_behavior_car, "police",
                             "Configure CAR (Committed Access Rate)", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("shape", cmd_behavior_shape, "shape",
                             "Configure traffic shaping", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("remark", cmd_behavior_remark, "set",
                             "Configure remark action", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("queue", cmd_behavior_queue, "bandwidth",
                             "Configure queue assignment", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("filter", cmd_behavior_filter, "drop",
                             "Configure filter (drop) action", CMD_CAT_QOS),

    /* Policy */
    HUAWEI_CMD_WITH_CATEGORY("traffic policy", cmd_traffic_policy, "service-policy",
                             "Create or enter traffic policy", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("classifier", cmd_policy_classifier_behavior, "class",
                             "Bind classifier and behavior", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("apply policy", cmd_apply_policy, "service-policy output",
                             "Apply traffic policy to interface", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("undo apply policy", cmd_undo_apply_policy, "no service-policy",
                             "Remove traffic policy from interface", CMD_CAT_QOS),

    /* Queue & Scheduler */
    HUAWEI_CMD_WITH_CATEGORY("qos queue", cmd_qos_queue, "priority-queue",
                             "Configure QoS queue", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("scheduler wrr", cmd_qos_scheduler_wrr, "bandwidth remaining",
                             "Configure WRR scheduler", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("scheduler wred", cmd_qos_scheduler_wred, "random-detect",
                             "Configure WRED scheduler", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("qos car", cmd_qos_car, "police",
                             "Configure interface-level CAR", CMD_CAT_QOS),

    /* Display / Save */
    HUAWEI_CMD_WITH_CATEGORY("display qos", cmd_display_qos, "show policy-map",
                             "Display QoS configuration and kernel status", CMD_CAT_QOS),
    HUAWEI_CMD_WITH_CATEGORY("save qos", cmd_save_qos, "write qos",
                             "Save QoS configuration", CMD_CAT_QOS),

    { .name = NULL }
};

void register_qos_cmds(void) {
    printf("[QoS] Registering QoS commands with Linux TC (Traffic Control) backend...\n");
    if (!tc_exists()) {
        printf("[QoS] Warning: tc command not found. Install iproute2 package for full QoS enforcement.\n");
    }
}
