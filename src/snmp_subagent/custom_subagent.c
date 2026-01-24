#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

/* OID for our custom MIB: .1.3.6.1.4.1.9999.1 (iso.org.dod.internet.private.enterprise.9999.1) */
static oid custom_mib_oid[] = { 1, 3, 6, 1, 4, 1, 9999, 1 };

/* 
 * 注册自定义 MIB 节点
 * OID: .1.3.6.1.4.1.9999.1.1.0 (CustomUptime)
 * OID: .1.3.6.1.4.1.9999.1.2.0 (CustomFlowCount)
 */
void init_custom_subagent(void) {
    netsnmp_handler_registration *reg;

    /* 注册 CustomUptime (.1.3.6.1.4.1.9999.1.1) */
    reg = netsnmp_create_handler_registration(
        "customUptime", handle_custom_mib,
        custom_mib_oid, OID_LENGTH(custom_mib_oid),
        HANDLER_CAN_RWRITE
    );
    netsnmp_register_scalar(reg);

    /* 注册 CustomFlowCount (.1.3.6.1.4.1.9999.1.2) */
    reg = netsnmp_create_handler_registration(
        "customFlowCount", handle_custom_mib,
        custom_mib_oid, OID_LENGTH(custom_mib_oid),
        HANDLER_CAN_RWRITE
    );
    netsnmp_register_scalar(reg);
}

int handle_custom_mib(netsnmp_mib_handler *handler,
                      netsnmp_handler_registration *reginfo,
                      netsnmp_agent_request_info *reqinfo,
                      netsnmp_request_info *requests) {
    
    switch (reqinfo->mode) {
        case MODE_GET:
            if (requests->requestid == 1) { /* CustomUptime */
                long uptime = time(NULL) - 1672531200; /* 模拟一个自定义的启动时间 */
                snmp_set_var_typed_value(requests->requestvb, ASN_TIMETICKS,
                                         (u_char *) &uptime, sizeof(uptime));
            } else if (requests->requestid == 2) { /* CustomFlowCount */
                long flow_count = 12345; /* 模拟一个 Flowspec 匹配的流量计数 */
                snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
                                         (u_char *) &flow_count, sizeof(flow_count));
            }
            break;

        default:
            /* 忽略其他模式 (SET, GETNEXT, etc.) */
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

int main(int argc, char **argv) {
    /* 成为 AgentX 子代理 */
    netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, 1);
    
    /* 初始化 SNMP 库 */
    init_agent("custom_subagent");
    
    /* 注册自定义 MIB */
    init_custom_subagent();

    /* 启动 AgentX 监听 */
    if (init_master_agent() != 0) {
        snmp_log(LOG_ERR, "Error starting master agent\n");
        return 1;
    }

    /* 主循环 */
    while (1) {
        agent_check_and_process(1); /* 检查并处理 SNMP 请求 */
    }

    /* 永远不会到达这里 */
    return 0;
}
