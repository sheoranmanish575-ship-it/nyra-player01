/*****************************************************************************
 * CrashHandler.c : Nyra Player - crash-safe minidump + resume-on-restart
 *****************************************************************************
 * Same DbgHelp-based logic as the original src/modules/crash/crash_handler.c.
 * Requires dbghelp.lib (linked in src/app/CMakeLists.txt).
 *****************************************************************************/

#include "CrashHandler.h"

#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <string.h>

static char g_crash_dir[MAX_PATH];
static char g_state_file[MAX_PATH];

static LONG WINAPI nyra_unhandled_exception_filter(EXCEPTION_POINTERS *ep)
{
    char dump_path[MAX_PATH];
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(dump_path, sizeof(dump_path),
             "%s\\crash-%04d%02d%02d-%02d%02d%02d.dmp",
             g_crash_dir, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    HANDLE hFile = CreateFileA(dump_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei;
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                           MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo,
                           &mei, NULL, NULL);
        CloseHandle(hFile);
    }

    /* Deliberately does not transmit the dump anywhere. */
    return EXCEPTION_EXECUTE_HANDLER;
}

void nyra_crash_handler_install(const char *crash_dir)
{
    strncpy(g_crash_dir, crash_dir, sizeof(g_crash_dir) - 1);
    g_crash_dir[sizeof(g_crash_dir) - 1] = 0;
    CreateDirectoryA(crash_dir, NULL);
    snprintf(g_state_file, sizeof(g_state_file), "%s\\resume-state.txt", crash_dir);
    SetUnhandledExceptionFilter(nyra_unhandled_exception_filter);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

void nyra_crash_handler_update_state(const char *current_file, double position_sec)
{
    if (!g_state_file[0])
        return;
    FILE *f = fopen(g_state_file, "w");
    if (!f)
        return;
    fprintf(f, "%s\n%.3f\n", current_file, position_sec);
    fclose(f);
}

void nyra_crash_handler_mark_clean_exit(void)
{
    if (g_state_file[0])
        DeleteFileA(g_state_file);
}

bool nyra_crash_handler_check_previous_crash(char *out_last_file, size_t out_len,
                                              double *out_position_sec)
{
    if (!g_state_file[0])
        return false;
    FILE *f = fopen(g_state_file, "r");
    if (!f)
        return false; /* no marker => previous session exited cleanly */

    char line[MAX_PATH];
    bool ok = false;
    if (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        strncpy(out_last_file, line, out_len - 1);
        out_last_file[out_len - 1] = 0;
        double pos = 0;
        if (fscanf(f, "%lf", &pos) == 1) {
            *out_position_sec = pos;
            ok = true;
        }
    }
    fclose(f);
    return ok;
}
