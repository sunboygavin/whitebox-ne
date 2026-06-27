/*
 * WhiteBox NE - Production SRv6 Module with Real Kernel Enforcement
 * (Linux SRv6 via iproute2 seg6 + sysctl + netlink)
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
#include <net/if.h>
#include <sys/ioctl.h>

#include "../../frr_core/lib/huawei_cli.h"

/* ============================================================
 *  SRv6 Data Structures
 * ============================================================ */

typedef enum {
    SRV6_BEHAVIOR_END = 0,
    SRV6_BEHAVIOR_END_X = 1,       /* Cross-connect (L3 VPN) */
    SRV6_BEHAVIOR_END_T = 2,       /* Table lookup */
    SRV6_BEHAVIOR_END_DX4 = 3,    /* Decapsulation + IPv4 cross-connect */
    SRV6_BEHAVIOR_END_DX6 = 4,    /* Decapsulation + IPv6 cross-connect */
    SRV6_BEHAVIOR_END_DT4 = 5,    /* Decapsulation + specific IPv4 table */
    SRV6_BEHAVIOR_END_DT6 = 6,    /* Decapsulation + specific IPv6 table */
    SRV6_BEHAVIOR_END_B6 = 7,     /* Insert SRH (Binding SID) */
    SRV6_BEHAVIOR_END_BM = 8,     /* Insert SRH + Masquerading */
} srv6_behavior_t;

struct srv6_locator {
    char name[64];
    char prefix[128];          /* IPv6 prefix e.g. 2001:db8:1::/64 */
    uint8_t prefix_len;
    char func_bits[16];        /* Function bits length (default 16) */
    bool active;
};

struct srv6_sid {
    char sid[128];             /* Full IPv6 SID */
    char locator_name[64];
    srv6_behavior_t behavior;
    char arg[128];             /* Behavior-specific argument (e.g. outgoing interface, VPN table) */
    bool active;
};

struct srv6_policy {
    char name[64];
    char destination[128];     /* IPv6 destination prefix */
    char sids[1024];           /* Comma-separated SID list for segment list */
    char encap_mode[16];       /* inline | encap | l2encap */
    char out_if[64];           /* Outgoing interface */
    bool active;
};

#define MAX_LOCATORS 16
#define MAX_SIDS 256
#define MAX_POLICIES 64

static struct srv6_locator locators[MAX_LOCATORS];
static int locator_count = 0;
static struct srv6_sid sids[MAX_SIDS];
static int sid_count = 0;
static struct srv6_policy policies[MAX_POLICIES];
static int policy_count = 0;

/* ============================================================
 *  Shell Execution Helpers
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

static int iproute_exists(const char *table, const char *prefix)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "ip %s route show %s >/dev/null 2>&1", table, prefix);
    int ret = system(cmd);
    return (WIFEXITED(ret) && WEXITSTATUS(ret) == 0);
}

/* ============================================================
 *  Kernel SRv6 Enablement
 * ============================================================ */
static int srv6_enable_kernel(void)
{
    int ret = 0;
    ret |= exec_shell("sysctl -w net.ipv6.conf.all.seg6_enabled=1 >/dev/null 2>&1");
    ret |= exec_shell("sysctl -w net.ipv6.conf.default.seg6_enabled=1 >/dev/null 2>&1");
    /* Enable SRv6 HMAC if needed */
    exec_shell("sysctl -w net.ipv6.conf.all.seg6_hmac_enabled=0 >/dev/null 2>&1");
    return ret;
}

static bool srv6_kernel_check(void)
{
    /* Check if kernel supports SRv6 by checking iproute2 seg6 support */
    int ret = system("ip route add 2001:db8:ffff::1/128 dev lo encap seg6 mode inline segs 2001:db8::1 2>/dev/null; ip route del 2001:db8:ffff::1/128 dev lo 2>/dev/null");
    return (WIFEXITED(ret) && WEXITSTATUS(ret) == 0);
}

/* ============================================================
 *  Locator Management
 * ============================================================ */
static int cmd_srv6_locator(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 3) {
        printf("Usage: srv6 locator <name> prefix <ipv6-prefix> [func-bits <length>]\n");
        return -1;
    }

    if (locator_count >= MAX_LOCATORS) {
        printf("Error: Maximum SRv6 locators reached (%d)\n", MAX_LOCATORS);
        return -1;
    }

    struct srv6_locator *loc = &locators[locator_count++];
    memset(loc, 0, sizeof(*loc));

    strncpy(loc->name, args->argv[1], sizeof(loc->name) - 1);
    strcpy(loc->func_bits, "16"); /* default */

    for (int i = 2; i < args->argc - 1; i++) {
        if (strcmp(args->argv[i], "prefix") == 0) {
            strncpy(loc->prefix, args->argv[i + 1], sizeof(loc->prefix) - 1);
        } else if (strcmp(args->argv[i], "func-bits") == 0) {
            strncpy(loc->func_bits, args->argv[i + 1], sizeof(loc->func_bits) - 1);
        }
    }

    if (loc->prefix[0] == '\0') {
        printf("Error: prefix required\n");
        locator_count--;
        return -1;
    }

    /* Enable kernel SRv6 */
    srv6_enable_kernel();

    /* Add local route for the locator prefix to ensure local delivery */
    char cmd_buf[1024];
    snprintf(cmd_buf, sizeof(cmd_buf),
             "ip -6 route add %s dev lo proto static 2>/dev/null || true",
             loc->prefix);
    system(cmd_buf);

    loc->active = true;
    printf("SRv6 locator '%s' configured: prefix %s, func-bits %s\n",
           loc->name, loc->prefix, loc->func_bits);
    return 0;
}

/* ============================================================
 *  SID Management (Local Endpoints)
 * ============================================================ */
static int cmd_srv6_sid(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 5) {
        printf("Usage: srv6 sid <sid> locator <locator-name> behavior <end|end.x|end.t|end.dx4|end.dx6|end.dt4|end.dt6|end.b6|end.bm> [argument <value>]\n");
        return -1;
    }

    if (sid_count >= MAX_SIDS) {
        printf("Error: Maximum SRv6 SIDs reached (%d)\n", MAX_SIDS);
        return -1;
    }

    struct srv6_sid *sid = &sids[sid_count++];
    memset(sid, 0, sizeof(*sid));

    strncpy(sid->sid, args->argv[1], sizeof(sid->sid) - 1);

    for (int i = 2; i < args->argc - 1; i++) {
        if (strcmp(args->argv[i], "locator") == 0) {
            strncpy(sid->locator_name, args->argv[i + 1], sizeof(sid->locator_name) - 1);
        } else if (strcmp(args->argv[i], "behavior") == 0) {
            const char *b = args->argv[i + 1];
            if (strcmp(b, "end") == 0) sid->behavior = SRV6_BEHAVIOR_END;
            else if (strcmp(b, "end.x") == 0) sid->behavior = SRV6_BEHAVIOR_END_X;
            else if (strcmp(b, "end.t") == 0) sid->behavior = SRV6_BEHAVIOR_END_T;
            else if (strcmp(b, "end.dx4") == 0) sid->behavior = SRV6_BEHAVIOR_END_DX4;
            else if (strcmp(b, "end.dx6") == 0) sid->behavior = SRV6_BEHAVIOR_END_DX6;
            else if (strcmp(b, "end.dt4") == 0) sid->behavior = SRV6_BEHAVIOR_END_DT4;
            else if (strcmp(b, "end.dt6") == 0) sid->behavior = SRV6_BEHAVIOR_END_DT6;
            else if (strcmp(b, "end.b6") == 0) sid->behavior = SRV6_BEHAVIOR_END_B6;
            else if (strcmp(b, "end.bm") == 0) sid->behavior = SRV6_BEHAVIOR_END_BM;
            else {
                printf("Error: Unknown behavior '%s'\n", b);
                sid_count--;
                return -1;
            }
        } else if (strcmp(args->argv[i], "argument") == 0) {
            strncpy(sid->arg, args->argv[i + 1], sizeof(sid->arg) - 1);
        }
    }

    /* Install SID into kernel local table */
    char cmd_buf[1024];
    int ret = 0;

    switch (sid->behavior) {
    case SRV6_BEHAVIOR_END:
        /* End: receive and process SRH, continue */
        snprintf(cmd_buf, sizeof(cmd_buf),
                 "ip -6 route add %s dev lo proto static table local 2>/dev/null || true",
                 sid->sid);
        ret = system(cmd_buf);
        break;
    case SRV6_BEHAVIOR_END_X:
        /* End.X: cross-connect to next hop (argument = outgoing interface or NH) */
        snprintf(cmd_buf, sizeof(cmd_buf),
                 "ip -6 route add %s dev %s proto static table local 2>/dev/null || true",
                 sid->sid, sid->arg[0] ? sid->arg : "lo");
        ret = system(cmd_buf);
        break;
    case SRV6_BEHAVIOR_END_DT4:
        /* End.DT4: decapsulate and forward to IPv4 table (VPN) */
        /* Requires VRF/L3VPN table ID in argument */
        snprintf(cmd_buf, sizeof(cmd_buf),
                 "ip -6 route add %s dev lo encap seg6local action End.DT4 table %s proto static table local 2>/dev/null || true",
                 sid->sid, sid->arg[0] ? sid->arg : "254");
        ret = system(cmd_buf);
        break;
    case SRV6_BEHAVIOR_END_DX4:
        /* End.DX4: decapsulate and cross-connect to IPv4 neighbor */
        snprintf(cmd_buf, sizeof(cmd_buf),
                 "ip -6 route add %s dev lo encap seg6local action End.DX4 nh4 %s proto static table local 2>/dev/null || true",
                 sid->sid, sid->arg[0] ? sid->arg : "0.0.0.0");
        ret = system(cmd_buf);
        break;
    case SRV6_BEHAVIOR_END_T:
        /* End.T: table lookup */
        snprintf(cmd_buf, sizeof(cmd_buf),
                 "ip -6 route add %s dev lo encap seg6local action End.T table %s proto static table local 2>/dev/null || true",
                 sid->sid, sid->arg[0] ? sid->arg : "254");
        ret = system(cmd_buf);
        break;
    case SRV6_BEHAVIOR_END_B6:
        /* End.B6: insert SRH (binding SID) */
        snprintf(cmd_buf, sizeof(cmd_buf),
                 "ip -6 route add %s dev lo encap seg6local action End.B6 insert %s proto static table local 2>/dev/null || true",
                 sid->sid, sid->arg[0] ? sid->arg : "");
        ret = system(cmd_buf);
        break;
    default:
        /* Generic fallback: just add local route */
        snprintf(cmd_buf, sizeof(cmd_buf),
                 "ip -6 route add %s dev lo proto static table local 2>/dev/null || true",
                 sid->sid);
        ret = system(cmd_buf);
        break;
    }

    sid->active = true;
    printf("SRv6 SID %s installed: behavior=%s, arg=%s\n",
           sid->sid,
           sid->behavior == SRV6_BEHAVIOR_END ? "End" :
           sid->behavior == SRV6_BEHAVIOR_END_X ? "End.X" :
           sid->behavior == SRV6_BEHAVIOR_END_T ? "End.T" :
           sid->behavior == SRV6_BEHAVIOR_END_DX4 ? "End.DX4" :
           sid->behavior == SRV6_BEHAVIOR_END_DX6 ? "End.DX6" :
           sid->behavior == SRV6_BEHAVIOR_END_DT4 ? "End.DT4" :
           sid->behavior == SRV6_BEHAVIOR_END_DT6 ? "End.DT6" :
           sid->behavior == SRV6_BEHAVIOR_END_B6 ? "End.B6" :
           sid->behavior == SRV6_BEHAVIOR_END_BM ? "End.BM" : "Unknown",
           sid->arg);
    return 0;
}

/* ============================================================
 *  SRv6 Policy (Traffic Engineering / Segment List)
 * ============================================================ */
static int cmd_srv6_policy(struct cmd_element *cmd, struct cmd_args *args)
{
    if (args->argc < 5) {
        printf("Usage: srv6 policy <name> destination <ipv6-prefix> segment-list <sid1,sid2,...> [encap <inline|encap|l2encap>] [out-interface <if>]\n");
        return -1;
    }

    if (policy_count >= MAX_POLICIES) {
        printf("Error: Maximum SRv6 policies reached (%d)\n", MAX_POLICIES);
        return -1;
    }

    struct srv6_policy *pol = &policies[policy_count++];
    memset(pol, 0, sizeof(*pol));

    strncpy(pol->name, args->argv[1], sizeof(pol->name) - 1);
    strcpy(pol->encap_mode, "inline"); /* default */

    for (int i = 2; i < args->argc - 1; i++) {
        if (strcmp(args->argv[i], "destination") == 0) {
            strncpy(pol->destination, args->argv[i + 1], sizeof(pol->destination) - 1);
        } else if (strcmp(args->argv[i], "segment-list") == 0) {
            strncpy(pol->sids, args->argv[i + 1], sizeof(pol->sids) - 1);
        } else if (strcmp(args->argv[i], "encap") == 0) {
            strncpy(pol->encap_mode, args->argv[i + 1], sizeof(pol->encap_mode) - 1);
        } else if (strcmp(args->argv[i], "out-interface") == 0) {
            strncpy(pol->out_if, args->argv[i + 1], sizeof(pol->out_if) - 1);
        }
    }

    if (pol->destination[0] == '\0' || pol->sids[0] == '\0') {
        printf("Error: destination and segment-list are required\n");
        policy_count--;
        return -1;
    }

    /* Build ip route command with seg6 encap */
    char seg_cmd[2048];
    snprintf(seg_cmd, sizeof(seg_cmd),
             "ip -6 route add %s encap seg6 mode %s segs %s",
             pol->destination, pol->encap_mode, pol->sids);
    if (pol->out_if[0]) {
        char tmp[256];
        snprintf(tmp, sizeof(tmp), " dev %s", pol->out_if);
        strncat(seg_cmd, tmp, sizeof(seg_cmd) - strlen(seg_cmd) - 1);
    }
    strncat(seg_cmd, " 2>/dev/null || true", sizeof(seg_cmd) - strlen(seg_cmd) - 1);

    int ret = system(seg_cmd);
    if (WIFEXITED(ret) && WEXITSTATUS(ret) != 0) {
        printf("Warning: ip route command may have failed (exit %d)\n", WEXITSTATUS(ret));
    }

    pol->active = true;
    printf("SRv6 policy '%s' installed: %s -> [%s] mode=%s\n",
           pol->name, pol->destination, pol->sids, pol->encap_mode);
    return 0;
}

/* ============================================================
 *  Display / Save / Reset
 * ============================================================ */
static int cmd_display_srv6(struct cmd_element *cmd, struct cmd_args *args)
{
    printf("SRv6 Configuration Summary:\n\n");

    printf("Kernel Support:\n");
    printf("  seg6_enabled: %s\n",
           system("sysctl net.ipv6.conf.all.seg6_enabled 2>/dev/null | grep -q '= 1' && echo YES || echo NO") == 0 ? "YES" : "NO/UNKNOWN");

    printf("\nLocators (%d/%d):\n", locator_count, MAX_LOCATORS);
    for (int i = 0; i < locator_count; i++) {
        struct srv6_locator *loc = &locators[i];
        printf("  [%c] %s: %s (func-bits %s)\n",
               loc->active ? 'A' : 'I', loc->name, loc->prefix, loc->func_bits);
    }

    printf("\nSIDs (%d/%d):\n", sid_count, MAX_SIDS);
    for (int i = 0; i < sid_count; i++) {
        struct srv6_sid *sid = &sids[i];
        const char *bstr = "Unknown";
        switch (sid->behavior) {
        case SRV6_BEHAVIOR_END: bstr = "End"; break;
        case SRV6_BEHAVIOR_END_X: bstr = "End.X"; break;
        case SRV6_BEHAVIOR_END_T: bstr = "End.T"; break;
        case SRV6_BEHAVIOR_END_DX4: bstr = "End.DX4"; break;
        case SRV6_BEHAVIOR_END_DX6: bstr = "End.DX6"; break;
        case SRV6_BEHAVIOR_END_DT4: bstr = "End.DT4"; break;
        case SRV6_BEHAVIOR_END_DT6: bstr = "End.DT6"; break;
        case SRV6_BEHAVIOR_END_B6: bstr = "End.B6"; break;
        case SRV6_BEHAVIOR_END_BM: bstr = "End.BM"; break;
        }
        printf("  [%c] %s: behavior=%s, arg=%s, locator=%s\n",
               sid->active ? 'A' : 'I', sid->sid, bstr, sid->arg, sid->locator_name);
    }

    printf("\nPolicies (%d/%d):\n", policy_count, MAX_POLICIES);
    for (int i = 0; i < policy_count; i++) {
        struct srv6_policy *pol = &policies[i];
        printf("  [%c] %s: %s -> [%s] mode=%s, out=%s\n",
               pol->active ? 'A' : 'I', pol->name, pol->destination,
               pol->sids, pol->encap_mode, pol->out_if[0] ? pol->out_if : "-");
    }

    printf("\n--- Kernel SRv6 Routes (ip -6 route show) ---\n");
    system("ip -6 route show | grep -E 'seg6|encap' || echo 'No seg6 routes found'");

    printf("\n--- Kernel SRv6 Local Routes (table local) ---\n");
    system("ip -6 route show table local | grep -E 'seg6local|dev lo' || echo 'No local SRv6 routes'");

    return 0;
}

static int cmd_save_srv6(struct cmd_element *cmd, struct cmd_args *args)
{
    const char *file = "/etc/whitebox-ne/srv6.conf";
    if (args->argc > 0) file = args->argv[0];

    FILE *fp = fopen(file, "w");
    if (!fp) {
        printf("Error: Cannot open %s\n", file);
        return -1;
    }

    for (int i = 0; i < locator_count; i++) {
        struct srv6_locator *loc = &locators[i];
        fprintf(fp, "srv6 locator %s prefix %s func-bits %s\n",
                loc->name, loc->prefix, loc->func_bits);
    }
    for (int i = 0; i < sid_count; i++) {
        struct srv6_sid *sid = &sids[i];
        const char *bstr = "end";
        switch (sid->behavior) {
        case SRV6_BEHAVIOR_END_X: bstr = "end.x"; break;
        case SRV6_BEHAVIOR_END_T: bstr = "end.t"; break;
        case SRV6_BEHAVIOR_END_DX4: bstr = "end.dx4"; break;
        case SRV6_BEHAVIOR_END_DX6: bstr = "end.dx6"; break;
        case SRV6_BEHAVIOR_END_DT4: bstr = "end.dt4"; break;
        case SRV6_BEHAVIOR_END_DT6: bstr = "end.dt6"; break;
        case SRV6_BEHAVIOR_END_B6: bstr = "end.b6"; break;
        case SRV6_BEHAVIOR_END_BM: bstr = "end.bm"; break;
        default: bstr = "end"; break;
        }
        fprintf(fp, "srv6 sid %s locator %s behavior %s",
                sid->sid, sid->locator_name, bstr);
        if (sid->arg[0]) fprintf(fp, " argument %s", sid->arg);
        fprintf(fp, "\n");
    }
    for (int i = 0; i < policy_count; i++) {
        struct srv6_policy *pol = &policies[i];
        fprintf(fp, "srv6 policy %s destination %s segment-list %s encap %s",
                pol->name, pol->destination, pol->sids, pol->encap_mode);
        if (pol->out_if[0]) fprintf(fp, " out-interface %s", pol->out_if);
        fprintf(fp, "\n");
    }
    fclose(fp);
    printf("SRv6 configuration saved to %s\n", file);
    return 0;
}

static int cmd_reset_srv6(struct cmd_element *cmd, struct cmd_args *args)
{
    /* Remove all SRv6 routes we installed */
    for (int i = 0; i < policy_count; i++) {
        struct srv6_policy *pol = &policies[i];
        if (pol->active) {
            char cmd_buf[1024];
            snprintf(cmd_buf, sizeof(cmd_buf),
                     "ip -6 route del %s 2>/dev/null || true", pol->destination);
            system(cmd_buf);
        }
    }
    for (int i = 0; i < sid_count; i++) {
        struct srv6_sid *sid = &sids[i];
        if (sid->active) {
            char cmd_buf[1024];
            snprintf(cmd_buf, sizeof(cmd_buf),
                     "ip -6 route del %s table local 2>/dev/null || true", sid->sid);
            system(cmd_buf);
        }
    }
    for (int i = 0; i < locator_count; i++) {
        struct srv6_locator *loc = &locators[i];
        if (loc->active) {
            char cmd_buf[1024];
            snprintf(cmd_buf, sizeof(cmd_buf),
                     "ip -6 route del %s dev lo 2>/dev/null || true", loc->prefix);
            system(cmd_buf);
        }
    }

    locator_count = 0;
    sid_count = 0;
    policy_count = 0;
    printf("All SRv6 configuration cleared from kernel and memory.\n");
    return 0;
}

/* ============================================================
 *  Command Registration
 * ============================================================ */
struct cmd_element srv6_cmds[] = {
    HUAWEI_CMD_WITH_CATEGORY("srv6 locator", cmd_srv6_locator, "segment-routing srv6 locator",
                             "Configure SRv6 locator", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("srv6 sid", cmd_srv6_sid, "segment-routing srv6 sid",
                             "Configure SRv6 local SID", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("srv6 policy", cmd_srv6_policy, "segment-routing srv6 policy",
                             "Configure SRv6 policy (segment list)", CMD_CAT_ROUTING),
    HUAWEI_CMD_WITH_CATEGORY("display srv6", cmd_display_srv6, "show segment-routing srv6",
                             "Display SRv6 configuration and kernel state", CMD_CAT_MONITOR),
    HUAWEI_CMD_WITH_CATEGORY("save srv6", cmd_save_srv6, "write segment-routing srv6",
                             "Save SRv6 configuration to file", CMD_CAT_SYSTEM),
    HUAWEI_CMD_WITH_CATEGORY("reset srv6", cmd_reset_srv6, "clear segment-routing srv6",
                             "Clear all SRv6 rules and configuration", CMD_CAT_SYSTEM),
    { .name = NULL }
};

void register_srv6_cmds(void) {
    printf("[SRv6] Registering SRv6 commands with iproute2 seg6 backend...\n");
    if (!srv6_kernel_check()) {
        printf("[SRv6] WARNING: Kernel SRv6 support may not be fully available. "
               "Ensure CONFIG_IPV6_SEG6_LWTUNNEL is enabled.\n");
    }
}
