/*
 * BGP Flowspec Core Logic
 * 
 * 本文件展示了 BGP 进程如何解析和处理 Flowspec 路由。
 * 开发者可以在此修改流量过滤规则的匹配逻辑或动作。
 */

#include <zebra.h>
#include "bgpd/bgpd.h"
#include "bgpd/bgp_flowspec.h"

/* 
 * 修改点 1: 自定义 Flowspec 动作解析
 * 当收到 Flowspec 路由时，解析其 Extended Community 中的动作（如 Discard, Rate-limit）。
 */
int bgp_fs_parse_action(struct stream *s, struct bgp_fs_action *action) {
    /* 
     * Manus AI 注释: 
     * 华为/华三等厂商可能支持私有的 Flowspec 动作。
     * 开发者可以在此处增加对特定 Type/Subtype 的解析逻辑。
     */
    uint16_t type = stream_getw(s);
    
    if (type == FLOWSPEC_ACTION_TRAFFIC_RATE) {
        action->rate = stream_getf(s);
        zlog_info("Flowspec Action: Rate-limit to %f bps", action->rate);
    } else if (type == FLOWSPEC_ACTION_TRAFFIC_DISCARD) {
        zlog_info("Flowspec Action: Discard traffic");
    }
    
    return 0;
}

/* 
 * 修改点 2: 规则下发至转发面
 * 将解析后的 Flowspec 规则通过 ZAPI 发送给 Zebra。
 */
void bgp_fs_install_zebra(struct prefix_fs *pfs, struct bgp_fs_action *action) {
    /* 
     * 修改建议: 
     * 如果您的白盒网元使用 P4 或 OpenFlow 转发，
     * 可以在此处重定向输出，将规则发送给外部控制器。
     */
    zlog_debug("Sending Flowspec rule to Zebra for installation");
}
