#ifndef _ACER_DIAG_COMMANDS_H
#define _ACER_DIAG_COMMANDS_H

int cmd_os_unlock();
int cmd_os_lock();

int cmd_os_get_os_version();
int cmd_os_get_amss_version();

int cmd_amss_get_hw_version();
int cmd_amss_get_os_version();

int cmd_os_switch_to_amss();
int cmd_amss_switch_to_recovery();

int cmd_os_reset();
int cmd_amss_reset();

int cmd_amss_test();

int cmd_os_test();

struct {
    char name[32];
    char description[64];
    int mode;
    int (*callback)(void);
} acer_diag_commands[] = {
    { "unlock", "Unlock DIAG mode", ACER_MODE_OS, cmd_os_unlock },
    { "lock", "Lock DIAG mode", ACER_MODE_OS, cmd_os_lock },

    { "get-amss-version", "Get AMSS version string", ACER_MODE_OS, cmd_os_get_amss_version },
    { "get-os-version", "Get OS version string", ACER_MODE_OS, cmd_os_get_os_version },

    { "switch-to-amss", "Switch to AMSS", ACER_MODE_OS, cmd_os_switch_to_amss },
    { "reset", "Reset", ACER_MODE_OS, cmd_os_reset },

    /* Working ?*/
    { "get-hw-version", "Get hardware version string", ACER_MODE_AMSS, cmd_amss_get_hw_version },
    { "switch-to-fastboot", "Switch to Fastboot", ACER_MODE_AMSS, cmd_amss_reset },
    { "test", "Test connection" , ACER_MODE_OS, cmd_os_test },

    /* Not working */
    { "switch-to-recovery", "Switch to Recovery", ACER_MODE_AMSS, cmd_amss_switch_to_recovery },
    { "get-os-version", "Get OS version string", ACER_MODE_AMSS, cmd_amss_get_os_version },

    { "reset", "Reset", ACER_MODE_AMSS, cmd_amss_reset },
    { "amss-test", "Test code", ACER_MODE_AMSS, cmd_amss_test },
};

#endif // _ACER_DIAG_COMMANDS_H
