/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#include <exec/semaphores.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <aros/debug.h>
#include "virgl_stats.h"

struct Device *TimerBase;
static struct timerequest timer_request;
static struct SignalSemaphore lock;
static BOOL ready, tried;
static ULONG frequency;
static struct { UQUAD ticks, bytes; ULONG count; } stats[AVGL_ST_COUNT];
static const char *const names[AVGL_ST_COUNT] = {
    "command", "submit", "put", "get", "create", "present", "present-read", "present-copy"
};

static BOOL start(void)
{
    if (ready) return TRUE;
    Forbid();
    if (!tried) {
        tried = TRUE;
        InitSemaphore(&lock);
        if (OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ, &timer_request.tr_node, 0) == 0) {
            struct EClockVal v;
            TimerBase = timer_request.tr_node.io_Device;
            frequency = ReadEClock(&v);
            ready = frequency != 0;
        }
    }
    Permit();
    return ready;
}
UQUAD avgl_stat_now(void)
{
    struct EClockVal v;
    if (!start()) return 0;
    ReadEClock(&v);
    return ((UQUAD)v.ev_hi << 32) | v.ev_lo;
}
void avgl_stat_add(enum avgl_stat which, UQUAD begin, ULONG bytes)
{
    UQUAD end;
    if (!ready || !begin || which >= AVGL_ST_COUNT) return;
    end = avgl_stat_now();
    ObtainSemaphore(&lock);
    stats[which].ticks += end - begin;
    stats[which].bytes += bytes;
    stats[which].count++;
    ReleaseSemaphore(&lock);
}
void avgl_stat_report(const char *why)
{
    unsigned i;
    if (!ready) return;
    ObtainSemaphore(&lock);
    bug("[virgl-stats] %s\n", why);
    for (i = 0; i < AVGL_ST_COUNT; i++) {
        if (!stats[i].count) continue;
        bug("[virgl-stats] %-13s n=%lu ms=%lu avg_us=%lu kb=%lu\n", names[i],
            (unsigned long)stats[i].count,
            (unsigned long)(stats[i].ticks * 1000 / frequency),
            (unsigned long)(stats[i].ticks * 1000000 / frequency / stats[i].count),
            (unsigned long)(stats[i].bytes / 1024));
        stats[i].ticks = stats[i].bytes = 0; stats[i].count = 0;
    }
    ReleaseSemaphore(&lock);
}
