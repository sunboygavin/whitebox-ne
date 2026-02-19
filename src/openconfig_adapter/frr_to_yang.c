/**
 * FRR to YANG Configuration Adapter
 * 
 * This module provides functionality to convert FRR configuration
 * to OpenConfig YANG model data using Sysrepo API.
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
#define MAX_INTERFACE_NAME 256

/* Forward declarations */
static int frr_interface_to_yang(sr_session_ctx_t *session, const char *ifname);
static int frr_bgp_global_to_yang(sr_session_ctx_t *session);
static int frr_bgp_neighbor_to_yang(sr_session_ctx_t *session);
static int frr_system_to_yang(sr_session_ctx_t *session);
static char *execute_vtysh_command(const char *command);

/**
 * Convert all FRR configuration to YANG format
 */
int frr_to_yang(sr_session_ctx_t *session) {
    if (session == NULL) {
        fprintf(stderr, "Error: Invalid session pointer\n");
        return SR_ERR_INVAL_ARG;
    }

    fprintf(stdout, "Converting FRR configuration to YANG format...\n");

    int rc = SR_ERR_OK;

    /* Convert interface configuration */
    rc = frr_interface_to_yang(session, "eth0");
    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Error converting interface configuration: %d\n", rc);
        return rc;
    }

    /* Convert BGP global configuration */
    rc = frr_bgp_global_to_yang(session);
    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Error converting BGP global configuration: %d\n", rc);
        return rc;
    }

    /* Convert BGP neighbor configuration */
    rc = frr_bgp_neighbor_to_yang(session);
    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Error converting BGP neighbor configuration: %d\n", rc);
        return rc;
    }

    /* Convert system configuration */
    rc = frr_system_to_yang(session);
    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Error converting system configuration: %d\n", rc);
        return rc;
    }

    /* Apply changes to Sysrepo */
    rc = sr_apply_changes(session, 0);
    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Error applying changes: %d\n", rc);
        return rc;
    }

    fprintf(stdout, "FRR configuration converted successfully\n");
    return SR_ERR_OK;
}

/**
 * Convert FRR interface configuration to YANG
 */
static int frr_interface_to_yang(sr_session_ctx_t *session, const char *ifname) {
    char cmd[MAX_BUFFER_SIZE];
    char *output;
    sr_val_t *value = NULL;
    int rc;

    if (ifname == NULL) {
        fprintf(stderr, "Error: Invalid interface name\n");
        return SR_ERR_INVAL_ARG;
    }

    fprintf(stdout, "Converting interface %s configuration...\n", ifname);

    /* Get interface description from FRR */
    snprintf(cmd, sizeof(cmd), "vtysh -c 'show interface %s'", ifname);
    output = execute_vtysh_command(cmd);

    if (output != NULL) {
        /* Parse and convert to YANG */
        /* This is a simplified example - in production, proper parsing is needed */

        /* Set interface name */
        sr_new_val(&value);
        snprintf(value->xpath, MAX_BUFFER_SIZE, 
                "/openconfig-interfaces:interfaces/interface[name='%s']/config/name", 
                ifname);
        value->type = SR_STRING_T;
        value->data.string_val = strdup(ifname);
        rc = sr_set_item(session, value->xpath, value, 0);
        sr_free_val(value);

        if (rc != SR_ERR_OK) {
            fprintf(stderr, "Error setting interface name: %d\n", rc);
            free(output);
            return rc;
        }

        /* Set interface enabled state */
        sr_new_val(&value);
        snprintf(value->xpath, MAX_BUFFER_SIZE,
                "/openconfig-interfaces:interfaces/interface[name='%s']/config/enabled",
                ifname);
        value->type = SR_BOOL_T;
        value->data.bool_val = 1; /* Default enabled */
        rc = sr_set_item(session, value->xpath, value, 0);
        sr_free_val(value);

        if (rc != SR_ERR_OK) {
            fprintf(stderr, "Error setting interface enabled state: %d\n", rc);
            free(output);
            return rc;
        }

        free(output);
    }

    /* Get operational state */
    sr_new_val(&value);
    snprintf(value->xpath, MAX_BUFFER_SIZE,
            "/openconfig-interfaces:interfaces/interface[name='%s']/state/oper-status",
            ifname);
    value->type = SR_ENUM_T;
    value->data.enum_val = strdup("UP"); /* Parse actual status from FRR */
    rc = sr_set_item(session, value->xpath, value, 0);
    sr_free_val(value);

    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Error setting interface operational status: %d\n", rc);
        return rc;
    }

    fprintf(stdout, "Interface %s converted successfully\n", ifname);
    return SR_ERR_OK;
}

/**
 * Convert FRR BGP global configuration to YANG
 */
static int frr_bgp_global_to_yang(sr_session_ctx_t *session) {
    char *output;
    sr_val_t *value = NULL;
    int rc;

    fprintf(stdout, "Converting BGP global configuration...\n");

    /* Get BGP AS number from FRR */
    output = execute_vtysh_command("vtysh -c 'show running-config' | grep 'router bgp'");

    if (output != NULL) {
        /* Parse AS number from output */
        /* Simplified example - extract AS number */
        uint32_t as_number = 65001; /* Default or parsed value */

        sr_new_val(&value);
        snprintf(value->xpath, MAX_BUFFER_SIZE,
                "/openconfig-bgp:bgp/global/config/as");
        value->type = SR_UINT32_T;
        value->data.uint32_val = as_number;
        rc = sr_set_item(session, value->xpath, value, 0);
        sr sr_free_val(value);

        if (rc != SR_ERR_OK) {
            fprintf(stderr, "Error setting BGP AS number: %d\n", rc);
            free(output);
            return rc;
        }

        free(output);
    }

    /* Get router ID */
    output = execute_vtysh_command("vtysh -c 'show ip bgp' | grep 'Router ID'");

    if (output != NULL) {
        /* Parse router ID */
        /* Simplified example */
        const char *router_id = "192.168.1.1";

        sr_new_val(&value);
        snprintf(value->xpath, MAX_BUFFER_SIZE,
                "/openconfig-bgp:bgp/global/config/router-id");
        value->type = SR_STRING_T;
        value->data.string_val = strdup(router_id);
        rc = sr_set_item(session, value->xpath, value, 0);
        sr_free_val(value);

        if (rc != SR_ERR_OK) {
            fprintf(stderr, "Error setting BGP router ID: %d\n", rc);
            free(output);
            return rc;
        }

        free(output);
    }

    fprintf(stdout, "BGP global configuration converted successfully\n");
    return SR_ERR_OK;
}

/**
 * Convert FRR BGP neighbor configuration to YANG
 */
static
int frr_bgp_neighbor_to_yang(sr_session_ctx_t *session) {
    char *output;
    sr_val_t *value = NULL;
    int rc;

    fprintf(stdout, "Converting BGP neighbor configuration...\n");

    /* Get BGP neighbors from FRR */
    output = execute_vtysh_command("vtysh -c 'show ip bgp summary'");

    if (output != NULL) {
        /* Parse neighbor information */
        /* Simplified example - parse and convert each neighbor */

        /* Example neighbor */
        const char *neighbor_ip = "192.168.1.2";
        uint32_t peer_as = 65002;

        /* Set neighbor address */
        sr_new_val(&value);
        snprintf(value->xpath, MAX_BUFFER_SIZE,
                "/openconfig-bgp:bgp/neighbors/neighbor[neighbor-address='%s']/config/neighbor-address",
                neighbor_ip);
        value->type = SR_STRING_T;
        value->data.string_val = strdup(neighbor_ip);
        rc = sr_set_item(session, value->xpath, value, 0);
        sr_free_val(value);

        if (rc != SR_ERR_OK) {
            fprintf(stderr, "Error setting neighbor address: %d\n", rc);
            free(output);
            return rc;
        }

        /* Set peer AS */
        sr_new_val(&value);
        snprintf(value->xpath, MAX_BUFFER_SIZE,
                "/openconfig-bgp:bgp/neighbors/neighbor[neighbor-address='%s']/config/peer-as",
                neighbor_ip);
        value->type = SR_UINT32_T;
        value->data.uint32_val = peer_as;
        rc = sr_set_item(session, value->xpath, value, 0);
        sr_free_val(value);

        if (rc != SR_ERR_OK) {
            fprintf(stderr, "Error setting peer AS: %d\n", rc);
            free(output);
            return rc;
        }

        /* Set operational state */
        sr_new_val(&value);
        snprintf(value->xpath, MAX_BUFFER_SIZE,
                "/openconfig-bgp:bgp/neighbors/neighbor[neighbor-address='%s']/state/session-state",
                neighbor_ip);
        value->type = SR_ENUM_T;
        value->data.enum_val = strdup("ESTABLISHED"); /* Parse actual state */
        rc = sr_set_item(session, value->xpath, value, 0);
        sr_free_val(value);

        if (rc != SR_ERR_OK) {
            fprintf(stderr, "Error setting neighbor session state: %d\n", rc);
            free(output);
            return rc;
        }

        free(output);
    }

    fprintf(stdout, "BGP neighbor configuration converted successfully\n");
    return SR_ERR_OK;
}

/**
 * Convert system configuration to YANG
 */
static int frr_system_to_yang(sr_session_ctx_t *session) {
    char hostname[MAX_BUFFER_SIZE];
    sr_val_t *value = NULL;
    int rc;
    FILE *fp;

    fprintf(stdout, "Converting system configuration...\n");

    /* Get hostname from system */
    fp = fopen("/etc/hostname", "r");
    if (fp != NULL) {
        if (fgets(hostname, sizeof(hostname), fp) != NULL) {
            /* Remove newline */
            hostname[strcspn(hostname, "\n")] = 0;

            sr_new_val(&value);
            snprintf(value->xpath, MAX_BUFFER_SIZE,
                    "/openconfig-system:system/config/hostname");
            value->type = SR_STRING_T;
            value->data.string_val = strdup(hostname);
            rc = sr_set_item(session, value->xpath, value, 0);
            sr_free_val(value);

            if (rc != SR_ERR_OK) {
                fprintf(stderr, "Error setting hostname: %d\n", rc);
                fclose(fp);
                return rc;
            }
        }
        fclose(fp);
    }

    fprintf(stdout, "System configuration converted successfully\n");
    return SR_ERR_OK;
}

/**
 * Execute vtysh command and return output
 */
static char *execute_vtysh_command(const char *command) {
    FILE *pipe;
    char buffer[MAX_BUFFER_SIZE];
    char *result = NULL;
    size_t result_size = 0;
    size_t buffer_size = 0;

    if (command == NULL) {
        return NULL;
    }

    pipe = popen(command, "r");
    if (pipe == NULL) {
        return NULL;
    }

    result = malloc(MAX_BUFFER_SIZE);
    if (result == NULL) {
        pclose(pipe);
        return NULL;
    }
    result_size = MAX_BUFFER_SIZE;
    result[0] = '\0';

    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        buffer_size = strlen(buffer);
        if (result_size - strlen(result) < buffer_size + 1) {
            char *new_result = realloc(result, result_size * 2);
            if (new_result == NULL) {
                free(result);
                pclose(pipe);
                return NULL;
            }
            result = new_result;
            result_size *= 2;
        }
        strcat(result, buffer);
    }

    pclose(pipe);
    return result;
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
    rc = sr_session_start(connection, &session, SR_DS_STARTUP);
    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Error starting session: %d\n", rc);
        sr_disconnect(connection);
        return rc;
    }

    /* Convert FRR configuration to YANG */
    rc = frr_to_yang(session);
    if (rc != SR_ERR_OK) {
        fprintf(stderr, "Error converting FRR to YANG: %d\n", rc);
        sr_session_stop(session);
        sr_disconnect(connection);
        return rc;
    }

    /* Cleanup */
    sr_session_stop(session);
    sr_disconnect(connection);

    fprintf(stdout, "FRR to YANG conversion completed successfully\n");
    return 0;
}
