/* Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * Loads textures like a game does, draws with them, then ends either with
 * glADestroyContext or, with "noclean", without it, as a program that is
 * stopped or crashes does. Used to look for resources left behind.
 * usage: glstress [textures] [frames] [clean|noclean] [window side] */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <GL/gl.h>
#include <GL/gla.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIDE 256

int main(int argc, char **argv)
{
    int count = argc > 1 ? atoi(argv[1]) : 64;
    int frames = argc > 2 ? atoi(argv[2]) : 60;
    int clean = !(argc > 3 && !strcmp(argv[3], "noclean"));
    int side = argc > 4 ? atoi(argv[4]) : 256;
    static unsigned char pixels[SIDE * SIDE * 4];
    struct DateStamp t0, t1;
    struct Window *win;
    GLAContext ctx;
    GLuint *names;
    LONG ticks;
    int i, f;

    if (count < 1 || count > 4096 || frames < 0 || side < 64 || side > 2048) return 20;
    names = calloc(count, sizeof(*names));
    if (!names) return 20;
    win = OpenWindowTags(NULL, WA_Title, (IPTR)"glstress", WA_Left, 20, WA_Top, 20,
        WA_InnerWidth, side, WA_InnerHeight, side, WA_DragBar, TRUE,
        WA_Activate, FALSE, TAG_DONE);
    if (!win) { printf("FAIL: no window\n"); return 20; }
    ctx = glACreateContextTags(GLA_Window, (IPTR)win, GLA_DoubleBuf, TRUE,
        GLA_RGBMode, TRUE, TAG_DONE);
    if (!ctx) { printf("FAIL: no GL context\n"); CloseWindow(win); return 20; }
    glAMakeCurrent(ctx);
    printf("GL_RENDERER: %s\n", (const char *)glGetString(GL_RENDERER));

    DateStamp(&t0);
    glGenTextures(count, names);
    for (i = 0; i < count; i++) {
        memset(pixels, i * 37, sizeof(pixels));
        glBindTexture(GL_TEXTURE_2D, names[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SIDE, SIDE, 0, GL_RGBA,
            GL_UNSIGNED_BYTE, pixels);
    }
    glFinish();
    DateStamp(&t1);
    ticks = (t1.ds_Minute - t0.ds_Minute) * 3000 + (t1.ds_Tick - t0.ds_Tick);
    printf("textures: %d in %ld ms\n", count, (long)ticks * 20);

    glViewport(0, 0, side, side);
    glEnable(GL_TEXTURE_2D);
    DateStamp(&t0);
    for (f = 0; f < frames; f++) {
        glClear(GL_COLOR_BUFFER_BIT);
        for (i = 0; i < 16; i++) {
            float x = -1.0f + (i % 4) * 0.5f, y = -1.0f + (i / 4) * 0.5f;
            glBindTexture(GL_TEXTURE_2D, names[(f + i) % count]);
            glBegin(GL_QUADS);
            glTexCoord2f(0, 0); glVertex2f(x, y);
            glTexCoord2f(1, 0); glVertex2f(x + 0.5f, y);
            glTexCoord2f(1, 1); glVertex2f(x + 0.5f, y + 0.5f);
            glTexCoord2f(0, 1); glVertex2f(x, y + 0.5f);
            glEnd();
        }
        glASwapBuffers(ctx);
    }
    DateStamp(&t1);
    ticks = (t1.ds_Minute - t0.ds_Minute) * 3000 + (t1.ds_Tick - t0.ds_Tick);
    if (frames > 0)
        printf("frames: %d in %ld ms (%ld ms per frame)\n", frames,
            (long)ticks * 20, (long)ticks * 20 / frames);
    printf("GL_ERROR: %u\n", (unsigned)glGetError());

    if (clean) {
        glDeleteTextures(count, names);
        glAMakeCurrent(NULL);
        glADestroyContext(ctx);
        printf("context destroyed\n");
    } else
        printf("context left alive\n");
    CloseWindow(win);
    free(names);
    return 0;
}
