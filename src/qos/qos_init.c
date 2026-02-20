/*
 * QoS Module Initialization
 * Copyright (C) 2026 WhiteBox NE Team
 *
 * This module initializes all QoS components
 */

#include <stdio.h>

/* External initialization functions */
extern void qos_classifier_init(void);
extern void qos_behavior_init(void);
extern void qos_policy_init(void);
extern void qos_queue_init(void);

/*
 * Initialize QoS module
 */
void qos_module_init(void)
{
    printf("Initializing QoS module...\n");

    /* Initialize submodules */
    qos_classifier_init();
    qos_behavior_init();
    qos_policy_init();
    qos_queue_init();

    printf("QoS module initialized successfully\n");
}
