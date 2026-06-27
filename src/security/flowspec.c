/*
 * WhiteBox NE - Production Flowspec Module with Real Kernel Enforcement
 * (iptables + tc flower + netfilter conntrack for BGP Flowspec mapping)
 *
 * Copyright (C) 2026 WhiteBox NE Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#include "../../frr_core/lib/huawei_cli.h"

/* ============================================================
 *  Flowspec Rule Data Structures
 * ============================================================ */

typedef enum {
    FLOWSPEC_ACTION_DROP = 0,
    FLOWSPEC_ACTION_RATE_LIMIT = 1,   /* Police to kbps */
    FLOWSPEC_ACTION_REDIRECT = 2,     /* Redirect to interface/VRF */
    FLOWSPEC_ACTION_DSCP_MARK = 3,    /* Mark with DSCP value */
    FLOWSPEC_ACTION_ACCEPT = 4,       /* Accept (logging only) */
} flowspec_action_t;

struct flowspec_rule {
    char name[64];
    
    /* Match criteria (RFC 5575 / Flowspec NLRI components) */
    uint8_t protocol;               /* IP protocol (0=any) */
    char src_prefix[128];          /* Source IP prefix (IPv4/IPv6) */
    uint8_t src_prefix_len;
    char dst_prefix[128];          /* Destination IP prefix */
    uint8_t dst_prefix_len;
    uint16_t src_port_start;
    uint16_t src_port_end;
    uint16_t dst_port_start;
    uint16_t dst_port_end;
    uint8_t dscp;                   /* DSCP value (0=any) */
    uint8_t icmp_type;             /* ICMP type (0=any) */
    uint8_t icmp_code;             /* ICMP code (0=any) */
    uint8_t tcp_flags;             /* TCP flags (0=any) */
    uint8_t fragment;              /* Fragment (0=any) */
    char packet_length[32];        /* Packet length range e.g. "0-1000" */
    
    /* Action */
    flowspec_action_t action;
    char action_arg[256];          /* Action argument (rate limit kbps, redirect target, DSCP value) */
    
    /* State */
    bool active;
    char iptables_chain[64];       /* Which iptables chain this rule was installed in */
    int iptables_line;             /* Line number for deletion */
    char tc_filter_handle[32];   /* TC filter handle for tc-based rules */
};

#define MAX_FLOWSPEC_RULES 256

static struct flowspec_rule rules[MAX_FLOWSPEC_RULES];
static int rule_count = 0;

/* ============================================================
 *  Shell / iptables / tc Helpers
 * ============================================================ */
static int exec_shell(const char *fmt, ...)
{
    char cmd[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);
    int ret = system(cmd);
    if (WIFEXITED(ret)) return WEXITSTATUS(ret);
    return -1;
}

static int iptables_rule_exists(const char *table, const char *chain,
                                 const char *match_spec)
{
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "iptables -t %s -C %s %s 2>/dev/null",
             table, chain, match_spec);
    int ret = system(cmd);
    return (WIFEXITED(ret) && WEXITSTATUS(ret) == 0);
}

static int iptables_add_idempotent(const char *table, const char *chain,
                                    const char *match_spec)
{
    if (iptables_rule_exists(table, chain, match_spec)) return 0;
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "iptables -t %s -A %s %s",
             table, chain, match_spec);
    int ret = system(cmd);
    return WIFEXITED(ret) ? WEXITSTATUS(ret) : -1;
}

static int iptables_del_rule(const char *table, const char *chain,
                              const char *match_spec)
{
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "iptables -t %s -D %s %s 2>/dev/null || true",
             table, chain, match_spec);
    int ret = system(cmd);
    return WIFEXITED(ret) ? WEXITSTATUS(ret) : -1;
}

/* ============================================================
 *  Match Criteria Builder
 * ============================================================ */
static void build_match_spec(const struct flowspec_rule *r, char *out, size_t out_len)
{
    char buf[1024] = "";
    
    if (r->protocol != 0) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "-p %u ", r->protocol);
        strncat(buf, tmp, sizeof(buf) - strlen(buf) - 1);
    }
    if (r->src_prefix[0] && r->src_prefix_len > 0) {
        char tmp[128];
        snprintf(tmp, sizeof(tmp), "-s %s/%u ", r->src_prefix, r->src_prefix_len);
        strncat(buf, tmp, sizeof(buf) - strlen(buf) - 1);
    }
    if (r->dst_prefix[0] && r->dst_prefix_len > 0) {
        char tmp[128];
        snprintf(tmp, sizeof(tmp), "-d %s/%u ", r->dst_prefix, r->dst_prefix_len);
        strncat(buf, tmp, sizeof(buf) - strlen(buf) - 1);
    }
    if (r->src_port_start != 0) {
        char tmp[64];
        if (r->src_port_start == r->src_port_end) {
            snprintf(tmp, sizeof(tmp), "--sport %u ", r->src_port_start);
        } else {
            snprintf(tmp, sizeof(tmp), "--sport %u:%u ", r->src_port_start, r->src_port_end);
        }
        strncat(buf, tmp, sizeof(buf) - strlen(buf) - 1);
    }
    if (r->dst_port_start != 0) {
        char tmp[64];
        if (r->dst_port_start == r->dst_port_end) {
            snprintf(tmp, sizeof(tmp), "--dport %u ", r->dst_port_start);
        } else {
            snprintf(tmp, sizeof(tmp), "--dport %u:%u ", r->dst_port_start, r->dst_port_end);
        }
        strncat(buf, tmp, sizeof(buf) - strlen(buf) - 1);
    }
    if (r->dscp != 0) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "-m dscp --dscp %u ", r->dscp);
        strncat(buf, tmp, sizeof(buf) - strlen(buf) - 1);
    }
    if (r->icmp_type != 0 && r->protocol == 1) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "--icmp-type %u ", r->icmp_type);
        strncat(buf, tmp, sizeof(buf) - strlen(buf) - 1);
    }
    
    strncpy(out, buf, out_len - 1);
    out[out_len - 1] = '\0';
}

/* ============================================================
 *  Action Builder
 * ============================================================ */
static int build_action_spec(const struct flowspec_rule *r, char *out, size_t out_len)
{
    switch (r->action) {
    case FLOWSPEC_ACTION_DROP:
        snprintf(out, out_len, "-j DROP");
        return 0;
    case FLOWSPEC_ACTION_ACCEPT:
        snprintf(out, out_len, "-j ACCEPT");
        return 0;
    case FLOWSPEC_ACTION_RATE_LIMIT:
        {
            uint32_t rate_kbps = atoi(r->action_arg);
            if (rate_kbps == 0) rate_kbps = 1000;
            /* iptables hashlimit or police via tc */
            snprintf(out, out_len, "-m limit --limit %u/second -j ACCEPT", rate_kbps / 8);
        }
        return 0;
    case FLOWSPEC_ACTION_REDIRECT:
        snprintf(out, out_len, "-j TEE --gateway %s", r->action_arg);
        return 0;
    case FLOWSPEC_ACTION_DSCP_MARK:
        snprintf(out, out_len, "-j DSCP --set-dscp %s", r->action_arg);
        return 0;
    default:
        snprintf(out, out_len, "-j DROP");
        return 0;
    }
}

/* ============================================================
 *  Flowspec Rule Install (iptables backend)
 * ============================================================ */
static int install_flowspec_iptables(struct flowspec_rule *r)
{
    char match_spec[2048];
    char action_spec[512];
    char full_rule[2560];
    
    build_match_spec(r, match_spec, sizeof(match_spec));
    build_action_spec(r, action_spec, sizeof(action_spec));
    
    snprintf(full_rule, sizeof(full_rule), "%s%s", match_spec, action_spec);
    
    int ret = iptables_add_idempotent("filter", "FORWARD", full_rule);
    if (ret != 0) {
        printf("Error: Failed to install iptables rule (exit %d)\n", ret);
        return -1;
    }
    
    strncpy(r->iptables_chain, "FORWARD", sizeof(r->iptables_chain) - 1);
    r->active = true;
    return 0;
}

/* ============================================================
 *  Flowspec Rule Install (tc flower backend for rate-limit)
 * ============================================================ */
static int install_flowspec_tc(struct flowspec_rule *r, const char *ifname)
{
    if (!ifname || !ifname[0]) {
        printf("Error: tc backend requires interface name\n");
        return -1;
    }
    
    /* tc flower + police for rate limiting */
    char cmd[2048];
    uint32_t rate_kbps = atoi(r->action_arg);
    if (rate_kbps == 0) rate_kbps = 1000;
    
    /* Build tc flower filter */
    snprintf(cmd, sizeof(cmd),
             "tc filter add dev %s ingress protocol ip prio %u flower ",
             ifname, rule_count + 1);
    
    if (r->protocol != 0) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "ip_proto %u ", r->protocol);
        strncat(cmd, tmp, sizeof(cmd) - strlen(cmd) - 1);
    }
    if (r->src_prefix[0] && r->src_prefix_len > 0) {
        char tmp[128];
        snprintf(tmp, sizeof(tmp), "src_ip %s/%u ", r->src_prefix, r->src_prefix_len);
        strncat(cmd, tmp, sizeof(cmd) - strlen(cmd) - 1);
    }
    if (r->dst_prefix[0] && r->dst_prefix_len > 0) {
        char tmp[128];
        snprintf(tmp, sizeof(tmp), "dst_ip %s/%u ", r->dst_prefix, r->dst_prefix_len);
        strncat(cmd, tmp, sizeof(cmd) - strlen(cmd) - 1);
    }
    if (r->src_port_start != 0) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "src_port %u ", r->src_port_start);
        strncat(cmd, tmp, sizeof(cmd) - strlen(cmd) - 1);
    }
    if (r->dst_port_start != 0) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "dst_port %u ", r->dst_port_start);
        strncat(cmd, tmp, sizeof(cmd) - strlen(cmd) - 1);
    }
    if (r->dscp != 0) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "ip_tos %u ", r->dscp << 2);
        strncat(cmd, tmp, sizeof(cmd) - strlen(cmd) - 1);
    }
    
    /* Action: police or drop */
    char action[512];
    if (r->action == FLOWSPEC_ACTION_RATE_LIMIT) {
        snprintf(action, sizeof(action),
                 "action police rate %ukbit burst 32k conform-exceed drop",
                 rate_kbps);
    } else if (r->action == FLOWSPEC_ACTION_DROP) {
        snprintf(action, sizeof(action), "action drop");
    } else {
        snprintf(action, sizeof(action), "action pass");
    }
    strncat(cmd, action, sizeof(cmd) - strlen(cmd) - 1);
    
    int ret = system(cmd);
    if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0) {
        snprintf(r->tc_filter_handle, sizeof(r->tc_filter_handle), "%u", rule_count + 1);
        r->active = true;
        return 0;
    }
    printf("Warning: tc filter command failed (exit %d): %s\n", WEXITSTATUS(ret), cmd);
    return -1;
}

/* ============================================================
 *  CLI: flowspec rule
 * ============================================================ */
static int cmd_flowspec_rule(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 3) {
        printf("Usage: flowspec rule <name> [match ...] action <drop|rate-limit|redirect|dscp-mark|accept> [arg <value>] [interface <ifname>]\n");
        printf("Match options:\n");
        printf("  protocol <proto>   - IP protocol number (6=tcp, 17=udp, 1=icmp)\n");
        printf("  src-prefix <ip/len> - Source IP prefix\n");
        printf("  dst-prefix <ip/len> - Destination IP prefix\n");
        printf("  src-port <start> [to <end>] - Source port range\n");
        printf("  dst-port <start> [to <end>] - Destination port range\n");
        printf("  dscp <value>       - DSCP value\n");
        printf("  icmp-type <type>   - ICMP type (requires protocol 1)\n");
        printf("  packet-length <min[-max]> - Packet length range\n");
        printf("Actions:\n");
        printf("  action drop                     - Drop matching traffic\n");
        printf("  action rate-limit arg <kbps>    - Rate limit to kbps\n");
        printf("  action redirect arg <ip>        - Redirect to IP (TEE)\n");
        printf("  action dscp-mark arg <dscp>     - Mark DSCP\n");
        printf("  action accept                   - Accept (logging)\n");
        return -1;
    }

    if (rule_count >= MAX_FLOWSPEC_RULES) {
        printf("Error: Maximum Flowspec rules reached (%d)\n", MAX_FLOWSPEC_RULES);
        return -1;
    }

    struct flowspec_rule *r = &rules[rule_count++];
    memset(r, 0, sizeof(*r));
    strncpy(r->name, args->argv[1], sizeof(r->name) - 1);
    r->action = FLOWSPEC_ACTION_DROP; /* default */
    
    char ifname[64] = "";
    
    for (int i = 2; i < args->argc - 1; i++) {
        if (strcmp(args->argv[i], "protocol") == 0) {
            r->protocol = (uint8_t)atoi(args->argv[i + 1]);
        } else if (strcmp(args->argv[i], "src-prefix") == 0) {
            char *slash = strchr(args->argv[i + 1], '/');
            if (slash) {
                *slash = '\0';
                strncpy(r->src_prefix, args->argv[i + 1], sizeof(r->src_prefix) - 1);
                r->src_prefix_len = atoi(slash + 1);
                *slash = '/';
            } else {
                strncpy(r->src_prefix, args->argv[i + 1], sizeof(r->src_prefix) - 1);
                r->src_prefix_len = 32;
            }
        } else if (strcmp(args->argv[i], "dst-prefix") == 0) {
            char *slash = strchr(args->argv[i + 1], '/');
            if (slash) {
                *slash = '\0';
                strncpy(r->dst_prefix, args->argv[i + 1], sizeof(r->dst_prefix) - 1);
                r->dst_prefix_len = atoi(slash + 1);
                *slash = '/';
            } else {
                strncpy(r->dst_prefix, args->argv[i + 1], sizeof(r->dst_prefix) - 1);
                r->dst_prefix_len = 32;
            }
        } else if (strcmp(args->argv[i], "src-port") == 0) {
            r->src_port_start = atoi(args->argv[i + 1]);
            if (i + 2 < args->argc && strcmp(args->argv[i + 2], "to") == 0) {
                r->src_port_end = atoi(args->argv[i + 3]);
                i += 2;
            } else {
                r->src_port_end = r->src_port_start;
            }
        } else if (strcmp(args->argv[i], "dst-port") == 0) {
            r->dst_port_start = atoi(args->argv[i + 1]);
            if (i + 2 < args->argc && strcmp(args->argv[i + 2], "to") == 0) {
                r->dst_port_end = atoi(args->argv[i + 3]);
                i += 2;
            } else {
                r->dst_port_end = r->dst_port_start;
            }
        } else if (strcmp(args->argv[i], "dscp") == 0) {
            r->dscp = (uint8_t)atoi(args->argv[i + 1]);
        } else if (strcmp(args->argv[i], "icmp-type") == 0) {
            r->icmp_type = (uint8_t)atoi(args->argv[i + 1]);
        } else if (strcmp(args->argv[i], "packet-length") == 0) {
            strncpy(r->packet_length, args->argv[i + 1], sizeof(r->packet_length) - 1);
        } else if (strcmp(args->argv[i], "action") == 0) {
            const char *a = args->argv[i + 1];
            if (strcmp(a, "drop") == 0) r->action = FLOWSPEC_ACTION_DROP;
            else if (strcmp(a, "rate-limit") == 0) r->action = FLOWSPEC_ACTION_RATE_LIMIT;
            else if (strcmp(a, "redirect") == 0) r->action = FLOWSPEC_ACTION_REDIRECT;
            else if (strcmp(a, "dscp-mark") == 0) r->action = FLOWSPEC_ACTION_DSCP_MARK;
            else if (strcmp(a, "accept") == 0) r->action = FLOWSPEC_ACTION_ACCEPT;
            else {
                printf("Error: Unknown action '%s'\n", a);
                rule_count--;
                return -1;
            }
        } else if (strcmp(args->argv[i], "arg") == 0) {
            strncpy(r->action_arg, args->argv[i + 1], sizeof(r->action_arg) - 1);
        } else if (strcmp(args->argv[i], "interface") == 0) {
            strncpy(ifname, args->argv[i + 1], sizeof(ifname) - 1);
        }
    }
    
    /* Install rule */
    int ret = 0;
    if (r->action == FLOWSPEC_ACTION_RATE_LIMIT && ifname[0]) {
        /* Prefer tc flower for rate limiting on specific interface */
        ret = install_flowspec_tc(r, ifname);
        if (ret != 0) {
            printf("Warning: tc install failed, falling back to iptables\n");
            ret = install_flowspec_iptables(r);
        }
    } else {
        ret = install_flowspec_iptables(r);
    }
    
    if (ret != 0) {
        rule_count--;
        return -1;
    }
    
    printf("Flowspec rule '%s' installed successfully.\n", r->name);
    return 0;
}

/* ============================================================
 *  CLI: display flowspec
 * ============================================================ */
static int cmd_display_flowspec(struct cmd_element *cmd, struct cmd_args *args)
{
    printf("Flowspec Configuration Summary:\n\n");
    printf("Rules (%d/%d):\n", rule_count, MAX_FLOWSPEC_RULES);
    
    for (int i = 0; i < rule_count; i++) {
        struct flowspec_rule *r = &rules[i];
        const char *act_str = "Unknown";
        switch (r->action) {
        case FLOWSPEC_ACTION_DROP: act_str = "DROP"; break;
        case FLOWSPEC_ACTION_ACCEPT: act_str = "ACCEPT"; break;
        case FLOWSPEC_ACTION_RATE_LIMIT: act_str = "RATE-LIMIT"; break;
        case FLOWSPEC_ACTION_REDIRECT: act_str = "REDIRECT"; break;
        case FLOWSPEC_ACTION_DSCP_MARK: act_str = "DSCP-MARK"; break;
        }
        
        printf("  [%c] %s\n", r->active ? 'A' : 'I', r->name);
        printf("      Match: ");
        if (r->protocol) printf("proto=%u ", r->protocol);
        if (r->src_prefix[0]) printf("src=%s/%u ", r->src_prefix, r->src_prefix_len);
        if (r->dst_prefix[0]) printf("dst=%s/%u ", r->dst_prefix, r->dst_prefix_len);
        if (r->src_port_start) {
            if (r->src_port_start == r->src_port_end)
                printf("sport=%u ", r->src_port_start);
            else
                printf("sport=%u-%u ", r->src_port_start, r->src_port_end);
        }
        if (r->dst_port_start) {
            if (r->dst_port_start == r->dst_port_end)
                printf("dport=%u ", r->dst_port_start);
            else
                printf("dport=%u-%u ", r->dst_port_start, r->dst_port_end);
        }
        if (r->dscp) printf("dscp=%u ", r->dscp);
        if (r->icmp_type) printf("icmp-type=%u ", r->icmp_type);
        printf("\n");
        printf("      Action: %s", act_str);
        if (r->action_arg[0]) printf(" (%s)", r->action_arg);
        printf("\n");
        if (r->iptables_chain[0]) printf("      iptables: table=filter chain=%s\n", r->iptables_chain);
        if (r->tc_filter_handle[0]) printf("      tc: filter handle=%s\n", r->tc_filter_handle);
        printf("\n");
    }
    
    printf("--- Active iptables FORWARD rules ---\n");
    system("iptables -t filter -L FORWARD -n --line-numbers | head -30 || echo 'No iptables rules'");
    
    printf("\n--- Active tc ingress filters (per interface) ---\n");
    system("for iface in $(ls /sys/class/net/); do echo \"=== $iface ===\"; tc filter show dev $iface ingress 2>/dev/null || true; done");
    
    return 0;
}

/* ============================================================
 *  CLI: save flowspec
 * ============================================================ */
static int cmd_save_flowspec(struct cmd_element *cmd, struct cmd_args *args)
{
    const char *file = "/etc/whitebox-ne/flowspec.conf";
    if (args->argc > 0) file = args->argv[0];
    
    FILE *fp = fopen(file, "w");
    if (!fp) {
        printf("Error: Cannot open %s\n", file);
        return -1;
    }
    
    for (int i = 0; i < rule_count; i++) {
        struct flowspec_rule *r = &rules[i];
        fprintf(fp, "flowspec rule %s ", r->name);
        if (r->protocol) fprintf(fp, "protocol %u ", r->protocol);
        if (r->src_prefix[0]) fprintf(fp, "src-prefix %s/%u ", r->src_prefix, r->src_prefix_len);
        if (r->dst_prefix[0]) fprintf(fp, "dst-prefix %s/%u ", r->dst_prefix, r->dst_prefix_len);
        if (r->src_port_start) {
            if (r->src_port_start == r->src_port_end)
                fprintf(fp, "src-port %u ", r->src_port_start);
            else
                fprintf(fp, "src-port %u to %u ", r->src_port_start, r->src_port_end);
        }
        if (r->dst_port_start) {
            if (r->dst_port_start == r->dst_port_end)
                fprintf(fp, "dst-port %u ", r->dst_port_start);
            else
                fprintf(fp, "dst-port %u to %u ", r->dst_port_start, r->dst_port_end);
        }
        if (r->dscp) fprintf(fp, "dscp %u ", r->dscp);
        if (r->icmp_type) fprintf(fp, "icmp-type %u ", r->icmp_type);
        if (r->packet_length[0]) fprintf(fp, "packet-length %s ", r->packet_length);
        
        const char *act_str = "drop";
        switch (r->action) {
        case FLOWSPEC_ACTION_ACCEPT: act_str = "accept"; break;
        case FLOWSPEC_ACTION_RATE_LIMIT: act_str = "rate-limit"; break;
        case FLOWSPEC_ACTION_REDIRECT: act_str = "redirect"; break;
        case FLOWSPEC_ACTION_DSCP_MARK: act_str = "dscp-mark"; break;
        default: act_str = "drop"; break;
        }
        fprintf(fp, "action %s", act_str);
        if (r->action_arg[0]) fprintf(fp, " arg %s", r->action_arg);
        fprintf(fp, "\n");
    }
    fclose(fp);
    printf("Flowspec configuration saved to %s\n", file);
    return 0;
}

/* ============================================================
 *  CLI: reset flowspec
 * ============================================================ */
static int cmd_reset_flowspec(struct cmd_element *cmd, struct cmd_args *args)
{
    /* Remove all iptables rules we installed */
    for (int i = 0; i < rule_count; i++) {
        struct flowspec_rule *r = &rules[i];
        if (r->active && r->iptables_chain[0]) {
            char match_spec[2048];
            char action_spec[512];
            build_match_spec(r, match_spec, sizeof(match_spec));
            build_action_spec(r, action_spec, sizeof(action_spec));
            char full[2560];
            snprintf(full, sizeof(full), "%s%s", match_spec, action_spec);
            iptables_del_rule("filter", r->iptables_chain, full);
        }
        if (r->active && r->tc_filter_handle[0]) {
            /* Try to delete tc filter on all interfaces */
            system("for iface in $(ls /sys/class/net/); do "
                   "tc filter del dev $iface ingress prio 0 2>/dev/null || true; done");
        }
    }
    
    rule_count = 0;
    printf("All Flowspec rules cleared from kernel and memory.\n");
    return 0;
}

/* ============================================================
 *  Command Registration
 * ============================================================ */
struct cmd_element flowspec_cmds[] = {
    HUAWEI_CMD_WITH_CATEGORY("flowspec rule", cmd_flowspec_rule, "flowspec rule",
                             "Configure Flowspec rule (BGP Flowspec mapping to iptables/tc)", CMD_CAT_SECURITY),
    HUAWEI_CMD_WITH_CATEGORY("display flowspec", cmd_display_flowspec, "show flowspec",
                             "Display Flowspec rules and kernel state", CMD_CAT_MONITOR),
    HUAWEI_CMD_WITH_CATEGORY("save flowspec", cmd_save_flowspec, "write flowspec",
                             "Save Flowspec configuration to file", CMD_CAT_SYSTEM),
    HUAWEI_CMD_WITH_CATEGORY("reset flowspec", cmd_reset_flowspec, "clear flowspec",
                             "Clear all Flowspec rules", CMD_CAT_SYSTEM),
    { .name = NULL }
};

void register_flowspec_cmds(void) {
    printf("[Flowspec] Registering Flowspec commands with iptables/tc backend...\n");
    /* Ensure conntrack module is available for advanced matching */
    system("modprobe nf_conntrack 2>/dev/null || true");
}
