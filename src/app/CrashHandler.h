/*****************************************************************************
 * CrashHandler.h : Nyra Player - crash-safe minidump + resume-on-restart
 *****************************************************************************
 * Moved unchanged (logic-wise) from src/modules/crash/crash_handler.{c,h}.
 * FIXED: it is now actually called - nyra_crash_handler_install() from
 * main.cpp at startup, and nyra_crash_handler_mark_clean_exit() from
 * MainWindow::closeEvent(). In the original project this file existed but
 * nothing in the Qt shell ever called it: MainWindow instead kept its own,
 * separate "cleanExit" flag in QSettings that had nothing to do with this
 * module's own resume-state.txt file. That meant there were two different,
 * disconnected "did we crash last time" mechanisms that could disagree
 * with each other. See AUDIT.md Finding #6.
 *****************************************************************************/

#ifndef NYRA_CRASH_HANDLER_H
#define NYRA_CRASH_HANDLER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void nyra_crash_handler_install(const char *crash_dir);
void nyra_crash_handler_update_state(const char *current_file, double position_sec);
void nyra_crash_handler_mark_clean_exit(void);
bool nyra_crash_handler_check_previous_crash(char *out_last_file, size_t out_len,
                                              double *out_position_sec);

#ifdef __cplusplus
}
#endif

#endif /* NYRA_CRASH_HANDLER_H */
