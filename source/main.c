#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <malloc.h>
#include <stdlib.h>

#include "ui.h"

#define SOC_ALIGN      0x1000
#define SOC_BUFFERSIZE 0x100000

// Change this to whatever page you want the browser to open on launch.
// HTTPS is not supported - use an http:// address.
#define START_URL "http://example.com/"

static u32 *socBuffer = NULL;

int main(void) {
    gfxInitDefault();
    cfguInit();

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget *bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    socBuffer = (u32 *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    socInit(socBuffer, SOC_BUFFERSIZE);

    BrowserState state;
    browser_init(&state, START_URL);

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) break;

        circlePosition cpad;
        hidCircleRead(&cpad);

        if (!state.pending_fetch) {
            ui_update(&state, &cpad, kDown);
        }

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TargetClear(top, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF));
        C2D_SceneBegin(top);
        ui_draw_top(&state);

        C2D_TargetClear(bottom, C2D_Color32(0x20, 0x20, 0x28, 0xFF));
        C2D_SceneBegin(bottom);
        ui_draw_bottom(&state);

        C3D_FrameEnd(0);

        // Perform the (blocking) network fetch *after* the loading frame
        // above has actually been drawn to the screen.
        if (state.pending_fetch) {
            browser_do_fetch(&state);
            state.pending_fetch = 0;
        }
    }

    browser_cleanup(&state);

    socExit();
    free(socBuffer);

    C2D_Fini();
    C3D_Fini();
    cfguExit();
    gfxExit();
    return 0;
}
