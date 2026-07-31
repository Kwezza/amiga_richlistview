/**
 * Crash-safe PROGDIR diagnostic logger for the experimental CLV control.
 *
 * Compile and link only when CLV_ENABLE_LOGGING is defined.
 * Each entry: Open → Seek end → Write one line → Close.
 */

#include "rich_listview/clv_control_log.h"

#ifdef CLV_ENABLE_LOGGING

#include <exec/types.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define CLV_LOG_PATH        "PROGDIR:clv_control.log"
#define CLV_LOG_LINE_MAX    384
#define CLV_LOG_MSG_MAX     256

static BOOL g_clv_log_active = FALSE;
static ULONG g_clv_log_seq = 0;

static BOOL clv_log_is_leap(LONG year)
{
    if ((year % 4) != 0) {
        return FALSE;
    }
    if ((year % 100) != 0) {
        return TRUE;
    }
    return ((year % 400) == 0) ? TRUE : FALSE;
}

/* Convert AmigaDOS ds_Days (days since 1978-01-01) to calendar Y-M-D. */
static VOID clv_log_days_to_ymd(LONG days, LONG *year_out, LONG *month_out,
                                LONG *day_out)
{
    static const UBYTE mdays_n[12] =
        { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    static const UBYTE mdays_l[12] =
        { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    LONG year;
    LONG month;
    const UBYTE *mdays;
    LONG ydays;

    if (days < 0) {
        days = 0;
    }

    year = 1978;
    for (;;) {
        ydays = clv_log_is_leap(year) ? 366 : 365;
        if (days < ydays) {
            break;
        }
        days -= ydays;
        year++;
    }

    mdays = clv_log_is_leap(year) ? mdays_l : mdays_n;
    for (month = 0; month < 12; month++) {
        if (days < (LONG)mdays[month]) {
            break;
        }
        days -= (LONG)mdays[month];
    }

    if (year_out != 0) {
        *year_out = year;
    }
    if (month_out != 0) {
        *month_out = month + 1;
    }
    if (day_out != 0) {
        *day_out = days + 1;
    }
}

static VOID clv_log_format_prefix(char *prefix, ULONG prefix_sz, ULONG seq)
{
    struct DateStamp ds;
    LONG year;
    LONG month;
    LONG day;
    LONG hour;
    LONG minute;
    LONG second;

    if (prefix == 0 || prefix_sz < 32) {
        return;
    }

    memset(&ds, 0, sizeof(ds));
    DateStamp(&ds);

    clv_log_days_to_ymd(ds.ds_Days, &year, &month, &day);
    hour = ds.ds_Minute / 60;
    minute = ds.ds_Minute % 60;
    second = ds.ds_Tick / 50;
    if (second > 59) {
        second = 59;
    }

    sprintf(prefix, "%06lu  %04ld-%02ld-%02ld %02ld:%02ld:%02ld",
            (unsigned long)seq,
            (long)year, (long)month, (long)day,
            (long)hour, (long)minute, (long)second);
}

static VOID clv_log_commit_line(CONST_STRPTR line)
{
    BPTR fh;
    LONG len;

    if (!g_clv_log_active || line == 0) {
        return;
    }

    len = (LONG)strlen(line);
    if (len <= 0) {
        return;
    }

    fh = Open(CLV_LOG_PATH, MODE_OLDFILE);
    if (fh == 0) {
        return;
    }

    Seek(fh, 0, OFFSET_END);
    Write(fh, (APTR)line, len);
    Close(fh);
}

static VOID clv_log_emit(CONST_STRPTR message)
{
    char prefix[40];
    char line[CLV_LOG_LINE_MAX];
    ULONG seq;

    if (!g_clv_log_active || message == 0) {
        return;
    }

    g_clv_log_seq++;
    seq = g_clv_log_seq;
    clv_log_format_prefix(prefix, (ULONG)sizeof(prefix), seq);
    sprintf(line, "%s  %s\n", prefix, message);
    clv_log_commit_line(line);
}

static VOID clv_log_library_version(CONST_STRPTR name)
{
    struct Library *lib;

    if (name == 0) {
        return;
    }

    lib = OpenLibrary((STRPTR)name, 0);
    if (lib == 0) {
        clv_log_printf("%s not available", name);
        return;
    }

    clv_log_printf("%s version=%ld revision=%ld",
                   name,
                   (long)lib->lib_Version,
                   (long)lib->lib_Revision);
    CloseLibrary(lib);
}

BOOL clv_log_init(void)
{
    BPTR fh;

    g_clv_log_active = FALSE;
    g_clv_log_seq = 0;

    fh = Open(CLV_LOG_PATH, MODE_NEWFILE);
    if (fh == 0) {
        return FALSE;
    }
    Close(fh);

    g_clv_log_active = TRUE;

    clv_log_emit("CLV diagnostic log started");
    clv_log_emit("compile: CLV_ENABLE_LOGGING=1");
    clv_log_emit("compile: target=+aos68k cpu=68000");
#ifdef __VBCC__
    clv_log_emit("compile: compiler=VBCC");
#else
    clv_log_emit("compile: compiler=unknown");
#endif

    clv_log_library_version("intuition.library");
    clv_log_library_version("gadtools.library");
    clv_log_library_version("graphics.library");
    clv_log_library_version("layers.library");
    clv_log_library_version("dos.library");

    return TRUE;
}

VOID clv_log_shutdown(void)
{
    if (!g_clv_log_active) {
        return;
    }
    clv_log_emit("PROGRAM end");
    g_clv_log_active = FALSE;
}

VOID clv_log_write(CONST_STRPTR message)
{
    if (message == 0) {
        message = "(null)";
    }
    clv_log_emit(message);
}

VOID clv_log_printf(CONST_STRPTR format, ...)
{
    char msg[CLV_LOG_MSG_MAX];
    va_list ap;

    if (format == 0) {
        clv_log_emit("(null format)");
        return;
    }

    va_start(ap, format);
    vsprintf(msg, format, ap);
    va_end(ap);
    msg[CLV_LOG_MSG_MAX - 1] = '\0';
    clv_log_emit(msg);
}

#endif /* CLV_ENABLE_LOGGING */
