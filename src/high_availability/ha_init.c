/*
 * High Availability Module Initialization
 * Copyright (C) 2026 WhiteBox NE Team
 *
 * This module initializes all high availability components
 */

#include <stdio.h>

/* External initialization functions */
extern void vrrp_init(void);
extern void bfd_init(void);
extern void track_init(void);

/*
 * Initialize High Availability module
 */
void high_availability_init(void)
{
    printf("Initializing High Availability module...\n");

    /* Initialize submodules */
    vrrp_init();
    bfd_init();
    track_init();

    printf("High Availability module initialized successfully\n");
}
