/*
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */
#ifndef DOSEMU_CONFIG_H
#define DOSEMU_CONFIG_H

extern void parse_dosemu_users(void);
extern void secure_option_preparse(int *argc, char **argv);
extern void keyb_layout(int value);

typedef void (*config_scrub_t)(void);
int register_config_scrub(config_scrub_t config_scrub);
void unregister_config_scrub(config_scrub_t old_config_scrub);
extern char dosemu_conf[];
extern char global_conf[];

#endif
