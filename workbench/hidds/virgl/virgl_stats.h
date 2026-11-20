/* Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * Time spent in the VirGL client, per kind of work, measured with the
 * E-clock and printed to the debug output when a context ends. */
#ifndef AROS_VIRGL_STATS_H
#define AROS_VIRGL_STATS_H
#include <exec/types.h>
enum avgl_stat {
    AVGL_ST_COMMAND,      /* every command sent to the device, waiting included */
    AVGL_ST_SUBMIT,       /* command stream batches */
    AVGL_ST_PUT,          /* uploads to the host (textures, buffers) */
    AVGL_ST_GET,          /* downloads from the host (frame readback, glReadPixels) */
    AVGL_ST_CREATE,       /* resource creation (three commands each) */
    AVGL_ST_PRESENT,      /* DisplayResource as a whole */
    AVGL_ST_PRESENT_READ, /* its readback part */
    AVGL_ST_PRESENT_COPY, /* its WritePixelArray part */
    AVGL_ST_COUNT
};
UQUAD avgl_stat_now(void);
void avgl_stat_add(enum avgl_stat which, UQUAD start, ULONG bytes);
void avgl_stat_report(const char *why);
#endif
