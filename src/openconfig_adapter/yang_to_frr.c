/**
 * YANG to FRR Configuration Adapter
 * 
 * This module provides functionality to convert OpenConfig YANG
 * model data to FRR configuration using Sysrepo API.
 * 
 * Author: WhiteBox NE Team
 * Date: 2024-02-20
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysrepo.h>
#include <sysrepo/xpath.h>

#define MAX_BUFFER_SIZE 4096
#define MAX_COMMAND_LENGTH 8192

/* Forward declarations */
static int yang_interface_to_frr(sr_session_ctx_t *session);
static int yang_bgp_to_frr(sr_session_ctx_t *session);
static int yang_system_to_frr(sr_session_ctx_t *session);
static int execute_vtysh_command(const char *command);
static int apply_frr_config(void);

/**
 * Convert all YANG configuration to FRR format
 */
int yang_to_frr(sr_session_ctx_t *session) {
    if (session == NULL) {
        fprintf(stderr, "Error: Invalid session pointer\n");
        return SR_ERR_INVAL_ARG;
    }

    fprintf(stdout, "Converting YANG configuration to FRR format...\n");

    int rc = SR_ERR_OK;

    /* Convert interface configuration */
    rc = yang_interface_to_frr(session);
    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Error converting interface configuration: %d\n", rc);
        return rc;
    }

    /* Convert BGP configuration */
    rc = yang_bgp_to_frr(session);
    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Error converting BGP configuration: %d\n", rc);
        return rc;
    }

    /* Convert system configuration */
    rc = yang_system_to_frr(session);
    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Error converting system configuration: %d\n", rc);
        return rc;
    }

    /* Apply configuration to FRR */
    rc = apply_frr_config();
    if (rc != 0) {
        fprintf(stderr, "Error applying FRR configuration: %d\n", rc);
        return SR_ERR_INTERNAL;
    }

    fprintf(stdout, "YANG configuration converted successfully\n");
    return SR_ERR_OK;
}

/**
 * Convert YANG interface configuration to FRR
 */
static int yang_interface_to_frr(sr_session_ctx_t *session) {
    sr_val_t *values = NULL;
    size_t count = 0;
    char xpath[MAX_BUFFER_SIZE];
    int rc;

    fprintf(stdout, "Converting interface configuration from YANG...\n");

    /* Get all interface configurations */
    snprintf(xpath, sizeof(xpath), 
            "/openconfig-interfaces:interfaces/interface/config");
    
    rc = sr_get_items(session, xpath, &values, &count);
    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Error getting interface configurations: %d\n", rc);
        return rc;
    }

    if (count == 0) {
        fprintf(stdout, "No interface configurations found\n");
        sr_free_values(values, count);
        return SR_ERR_OK;
    }

    /* Process each interface */
    for (size_t i = 0; i < count; i++) {
        if (values[i].type == SR_STRING_T && strstr(values[i].xpath, "/config/name")) {
            const char *ifname = values[i].data.string_val;
            char cmd[MAX_COMMAND_LENGTH];

            /* Configure interface in FRR */
            snprintf(cmd, sizeof(cmd),
                    "vtysh -c 'configure terminal' -c 'interface %s'",
                    ifname);
            
            rc = execute_vtysh_command(cmd);
            if (rc != 0) {
                fprintf(stderr, "Error configuring interface %s: %d\n", 
                        ifname, rc);
            }
        }
    }

    /* Get interface enabled state */
    snprintf(xpath, sizeof(xpath),
            "/openconfig-interfaces:interfaces/interface/config/enabled");
    
    sr_free_values(values, count);
    rc = sr_get_items(session, xpath, &values, &count);
    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Error getting interface enabled states: %d\n", rc);
        return rc;
    }

    /* Process enabled states */
    for (size_t i = 0; i < count; i++) {
        if (values[i].type == SR_BOOL_T && strstr(values[i].xpath, "/config/enabled")) {
            bool enabled = values[i].data.bool_val;
            
            /* Extract interface name from xpath */
            char *ifname_start = strstr(values[i].xpath, "interface[name='");
            if (ifname_start != NULL) {
                ifname_start += strlen("interface[name='");
                char *ifname_end = strchr(ifname_start, '\'');
                if (ifname_end != NULL) {
                    *ifname_end = '\0';
                    
                    char cmd[MAX_COMMAND_LENGTH];
                    snprintf(cmd, sizeof(cmd),
                            "vtysh -c 'configure terminal' -c 'interface %s' -c '%s'",
                            ifname_start, enabled ? "no shutdown" : "shutdown");
                    
                    rc = execute_vtysh_command(cmd);
                    if (rc != 0) {
                        fprintf(stderr, "Error setting interface %s state: %d\n",
                                ifname_start, rc);
                    }
                }
            }
        }
    }

    sr_free_values(values, count);
    fprintf(stdout, "Interface configuration converted successfully\n");
    return SR_ERR_OK;
}

/**
 * Convert YANG BGP configuration to FRR
 */
static int yang_bgp_to_frr(sr_session_ctx_t *session) {
    sr_val_t *values = NULL;
    size_t count = 0;
    char xpath[MAX_BUFFER_SIZE];
    int rc;
    uint32_t as_number = 0;
    const char *router_id = NULL;

    fprintf(stdout, "Converting BGP configuration from YANG...\n");

    /* Get BGP global AS number */
    snprintf(xpath, sizeof(xpath),
            "/openconfig-bgp:bgp/global/config/as");
    
    rc = sr_get_item(session, xpath, &values);
    if (rc == SR_ERR_OK && values != NULL) {
        if (values->type == SR_UINT32_T) {
            as_number = values->data.uint32_val;
        }
        sr_free_val(values);
    }

    if (as_number == 0) {
        fprintf(stdout, "No BGP AS number configured\n");
        return SR_ERR_OK;
    }

    /* Configure BGP in FRR */
    char cmd[MAX_COMMAND_LENGTH];
    snprintf(cmd, sizeof(cmd),
            "vtysh -c 'configure terminal' -c 'router bgp %u'",
            as_number);
    
    rc = execute_vtysh_command(cmd);
    if (rc != 0) {
        fprintf(stderr, "Error configuring BGP AS: %d\n", rc);
        return SR_ERR_INTERNAL;
    }

    /* Get and configure router ID */
    snprintf(xpath, sizeof(xpath),
            "/openconfig-bgp:bgp/global/config/router-id");
    
    rc = sr_get_item(session, xpath, &values);
    if (rc == SR_ERR_OK && values != NULL) {
        if (values->type == SR_STRING_T) {
            router_id = values->data.string_val;
            snprintf(cmd, sizeof(cmd),
                    "vtysh -c 'configure terminal' -c 'router bgp %u' -c 'bgp router-id %s'",
                    as_number, router_id);
            
            rc = execute_vtysh_command(cmd);
            if (rc != 0) {
                fprintf(stderr, "Error configuring BGP router ID: %d\n", rc);
            }
        }
        sr_free_val(values);
    }

    /* Get and configure BGP neighbors */
    snprintf(xpath, sizeof(xpath),
            "/openconfig-bgp:bgp/neighbors/neighbor/config/neighbor-address");
    
    rc = sr_get_items(session, xpath, &values, &count);
    if (rc == SR_ERR_OK) {
        for (size_t i = 0; i < count; i++) {
            if (values[i].type == SR_STRING_T && strstr(values[i].xpath, "/config/neighbor-address")) {
                const char *neighbor_ip = values[i].data.string_val;
                uint32_t peer_as = 0;

                /* Get peer AS for this neighbor */
                char peer_as_xpath[MAX_BUFFER_SIZE];
                snprintf(peer_as_xpath, sizeof(peer_as_xpath),
                        "/openconfig-bgp:bgp/neighbors/neighbor[neighbor-address='%s']/config/peer-as",
                        neighbor_ip);
                
                sr_val_t *peer_as_val = NULL;
                rc = sr_get_item(session, peer_as_xpath, &peer_as_val);
                if (rc == SR_ERR_OK && peer_as_val != NULL) {
                    if (peer_as_val->type == SR_UINT32_T) {
                        peer_as = peer_as_val->data.uint32_val;
                    }
                    sr_free_val(peer_as_val);
                }

                if (peer_as > 0) {
                    /* Configure neighbor in FRR */
                    snprintf(cmd, sizeof(cmd),
                            "vtysh -c 'configure terminal' -c 'router bgp %u' -c 'neighbor %s remote-as %u'",
                            as_number, neighbor_ip, peer_as);
                    
                    rc = execute_vtysh_command(cmd);
                    if (rc != 0) {
                        fprintf(stderr, "Error configuring BGP neighbor %s: %d\n",
                                neighbor_ip, rc);
                    }
                }
            }
        }
        sr_free_values(values, count);
    }

    fprintf(stdout, "BGP configuration converted successfully\n");
    return SR_ERR_OK;
}

/**
 * Convert YANG system configuration to FRR
 */
static int yang_system_to_frr(sr_session_ctx_t *session) {
    sr_val_t *value = NULL;
    char xpath[MAX_BUFFER_SIZE];
    int rc;

    fprintf(stdout, "Converting system configuration from YANG...\n");

    /* Get and configure hostname */
    snprintf(xpath, sizeof(xpath),
            "/openconfig-system:system/config/hostname");
    
    rc = sr_get_item(session, xpath, &value);
    if (rc == SR_ERR_OK && value != NULL) {
        if (value->type == SR_STRING_T) {
            const char *hostname = value->data.string_val;
            
            /* Set system hostname */
            char cmd[MAX_COMMAND_LENGTH];
            snprintf(cmd, sizeof(cmd), "hostname %s", hostname);
            system(cmd);
            
            /* Also set in FRR */
            snprintf(cmd, sizeof(cmd),
                    "vtysh -c 'configure terminal' -c 'hostname %s'",
                    hostname);
            
            rc = execute_vtysh_command(cmd);
            if (rc != 0) {
                fprintf(stderr, "Error setting hostname: %d\n", rc);
            }
        }
        sr_free_val(value);
    }

    fprintf(stdout, "System configuration converted successfully\n");
    return SR_ERR_OK;
}

/**
 * Execute vtysh command
 */
static int execute_vtysh_command(const char *command) {
    int rc = system(command);
    return WEXITSTATUS(rc);
}

/**
 * Apply FRR configuration
 */
static int apply_frr_config(void) {
    fprintf(stdout, "Applying FRR configuration...\n");
    
    /* Save FRR configuration */
    int rc = execute_vtysh_command("vtysh -c 'write'");
    if (rc != 0) {
        fprintf(stderr, "Error saving FRR configuration: %d\n", rc);
        return rc;
    }

    /* Restart FRR service to apply changes */
    rc = system("systemctl restart frr");
    if (rc != 0) {
        fprintf(stderr, "Error restarting FRR service: %d\n", rc);
        return rc;
    }

    fprintf(stdout, "FRR configuration applied successfully\n");
    return 0;
}

/**
 * Main function for testing
 */
int main(int argc, char **argv) {
    sr_conn_ctx_t *connection = NULL;
    sr_session_ctx_t *session = NULL;
    int rc = SR_ERR_OK;

    /* Connect to Sysrepo */
    rc = sr_connect("whitebox-ne", SR_CONN_DEFAULT, &connection);
    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Error connecting to Sysrepo: %d\n", rc);
        return rc;
    }

    /* Start session */
    rc = sr_session_start(connection, &session, SR_DS_RUNNING);
    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Error starting session: %d\n", rc);
        sr_disconnect(connection);
        return rc;
    }

    /* Convert YANG configuration to FRR */
    rc = yang_to_frr(session);
    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Error converting YANG to FRR: %d\n", rc);
        sr_session_stop(session);
        sr_disconnect(connection);
        return rc;
    }

    /* Cleanup */
    sr_session_stop(session);
    sr_disconnect(connection);

    fprintf(stdout, "YANG to FRR conversion completed successfully\n");
    return 0;
}
