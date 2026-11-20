/* Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * Small OpenGL check: prints the renderer, draws and reads pixels back.
 * usage: glcheck [expected renderer substring] [frames] */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <dos/var.h>
#include <GL/gl.h>
#include <GL/gla.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pixel(int x, int y, int r, int g, int b, const char *what)
{
    unsigned char p[4] = {0, 0, 0, 0};
    int ok;
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
    ok = abs(p[0] - r) <= 2 && abs(p[1] - g) <= 2 && abs(p[2] - b) <= 2;
    printf("%s pixel(%d,%d): %u %u %u expected %d %d %d %s\n", what, x, y,
        p[0], p[1], p[2], r, g, b, ok ? "ok" : "WRONG");
    return ok;
}

/* virgl.hidd leaves the renderer it chose, and why, in ENV:VirGL-Status. */
static void status(void)
{
    char text[200];
    if (GetVar((CONST_STRPTR)"VirGL-Status", (STRPTR)text, sizeof(text), GVF_GLOBAL_ONLY) > 0)
        printf("VirGL-Status: %s\n", text);
}

int main(int argc, char **argv)
{
    static const unsigned char tex[16] = {
        255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 255, 255,   255, 255, 0, 255 };
    const char *expect = argc > 1 ? argv[1] : "";
    int frames = argc > 2 ? atoi(argv[2]) : 60;
    struct Window *win;
    GLAContext ctx;
    const char *renderer;
    GLuint texture;
    struct DateStamp t0, t1;
    LONG ticks;
    int ok = 1, i;

    win = OpenWindowTags(NULL, WA_Title, (IPTR)"glcheck", WA_Left, 40, WA_Top, 40,
        WA_InnerWidth, 256, WA_InnerHeight, 256, WA_DragBar, TRUE,
        WA_DepthGadget, TRUE, WA_Activate, FALSE, TAG_DONE);
    if (!win) { printf("FAIL: no window\n"); return 20; }
    ctx = glACreateContextTags(GLA_Window, (IPTR)win, GLA_DoubleBuf, TRUE,
        GLA_RGBMode, TRUE, TAG_DONE);
    if (!ctx) { status(); printf("FAIL: no GL context\n"); CloseWindow(win); return 20; }
    glAMakeCurrent(ctx);
    renderer = (const char *)glGetString(GL_RENDERER);
    printf("GL_RENDERER: %s\n", renderer ? renderer : "(null)");
    printf("GL_VERSION: %s\n", (const char *)glGetString(GL_VERSION));
    printf("GL_VENDOR: %s\n", (const char *)glGetString(GL_VENDOR));
    status();
    if (!renderer || !strstr(renderer, expect)) ok = 0;

    glViewport(0, 0, 256, 256);
    glClearColor(0.5f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ok = pixel(8, 8, 128, 0, 0, "clear") && ok;

    glBegin(GL_TRIANGLES);
    glColor3f(0, 1, 0);
    glVertex2f(-0.8f, -0.8f); glVertex2f(0.8f, -0.8f); glVertex2f(0.0f, 0.8f);
    glEnd();
    ok = pixel(128, 100, 0, 255, 0, "triangle") && ok;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex);
    glEnable(GL_TEXTURE_2D);
    glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1, -1);
    glTexCoord2f(1, 0); glVertex2f( 1, -1);
    glTexCoord2f(1, 1); glVertex2f( 1,  1);
    glTexCoord2f(0, 1); glVertex2f(-1,  1);
    glEnd();
    ok = pixel(64, 64, 255, 0, 0, "texture") && ok;
    ok = pixel(192, 64, 0, 255, 0, "texture") && ok;
    ok = pixel(64, 192, 0, 0, 255, "texture") && ok;
    ok = pixel(192, 192, 255, 255, 0, "texture") && ok;
    glDisable(GL_TEXTURE_2D);
    printf("GL_ERROR: %u\n", (unsigned)glGetError());
    glASwapBuffers(ctx);

    DateStamp(&t0);
    for (i = 0; i < frames; i++) {
        glClearColor(0.1f, 0.1f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        glRotatef(i * 6.0f, 0, 0, 1);
        glBegin(GL_TRIANGLES);
        glColor3f(1, 0, 0); glVertex2f(-0.8f, -0.8f);
        glColor3f(0, 1, 0); glVertex2f(0.8f, -0.8f);
        glColor3f(0, 0, 1); glVertex2f(0.0f, 0.8f);
        glEnd();
        glASwapBuffers(ctx);
    }
    DateStamp(&t1);
    ticks = (t1.ds_Minute - t0.ds_Minute) * 3000 + (t1.ds_Tick - t0.ds_Tick);
    if (frames > 0)
        printf("frames: %d in %ld ms (%ld ms per frame)\n", frames,
            (long)ticks * 20, (long)ticks * 20 / frames);

    glAMakeCurrent(NULL);
    glADestroyContext(ctx);
    CloseWindow(win);
    printf(ok ? "PASS\n" : "FAIL\n");
    return ok ? 0 : 10;
}
