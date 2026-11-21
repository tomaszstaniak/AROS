/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#ifndef AROS_VIRGL_REGISTRY_H
#define AROS_VIRGL_REGISTRY_H
#include <exec/types.h>
#include <exec/semaphores.h>
struct avgl_ws;
/* Every open winsys, so that what a program left behind can be released.
 * A program that ends without destroying its OpenGL context never calls the
 * driver again: nothing in the system closes OpenGL for it. Its context,
 * textures and buffers would stay on the host until the guest is restarted.
 * Each winsys therefore remembers the program that last used it. A small
 * process looks once a second for programs that have ended, and opening a
 * new winsys looks too; what such a program left is released on the host. */
struct avgl_registry {
    struct SignalSemaphore lock;
    struct avgl_ws *first;
    struct Task *watcher;
    /* The watcher's view of the host: commands finished at its last look
     * and looks without progress. */
    ULONG seen, same;
    /* Totals of what was released for ended programs. */
    ULONG contexts, resources;
    UQUAD bytes;
};
void avgl_registry_init(struct avgl_registry *r);
#endif
