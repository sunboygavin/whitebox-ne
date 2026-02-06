/*
 * SRv6 (Segment Routing over IPv6) Core Logic for Zebra
 * 
 * 本文件展示了 Zebra 进程如何处理 SRv6 Locator 和 SID 的核心逻辑。
 * 开发者可以在此修改 SID 的分配策略或封装行为。
 */

#include <zebra.h>
#include "stream.h"
#include "prefix.h"
#include "srv6.h"

/* 
 * 修改点 1: 自定义 SRv6 Locator 处理
 * 当控制面（如 BGP）下发 Locator 配置时，此函数负责在 Zebra 中创建对应的结构。
 */
int zebra_srv6_locator_add(struct srv6_locator *locator) {
    // Manus AI 注释: 此处可以增加对 Locator 前缀合法性的额外校验
    zlog_debug("SRv6 Locator Added: %pFX", &locator->prefix);
    
    /* 
     * 开发者可以在此处对接底层硬件驱动 (ASIC/DPDK)，
     * 将 Locator 范围下发到硬件的查找表中。
     */
    return 0;
}

/* 
 * 修改点 2: 自定义 End.SID 行为处理
 * 处理不同的 SRv6 Endpoint 行为（如 End, End.X, End.DT4 等）。
 */
int zebra_srv6_sid_install(struct srv6_sid *sid) {
    zlog_debug("Installing SRv6 SID: %pI6, Behavior: %d", &sid->sid, sid->behavior);

    switch (sid->behavior) {
        case SRV6_ENDPOINT_BEHAVIOR_END:
            /* 
             * 修改建议: 可以在此处增加自定义的计数器或监控逻辑，
             * 统计特定 SID 的命中次数。
             */
            break;
        case SRV6_ENDPOINT_BEHAVIOR_END_DT4:
            // 处理 IPv4 VPN 实例的解封装
            break;
        default:
            zlog_warn("Unsupported SRv6 behavior: %d", sid->behavior);
            break;
    }
    return 0;
}
