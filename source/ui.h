#ifndef UI_H
#define UI_H

#include <3ds.h>
#include <citro2d.h>
#include "htmlparse.h"

#define MAX_HISTORY   32
#define MAX_LINES     800
#define MAX_LINK_ROWS 128
#define MAX_ELEMENTS  16

typedef enum { MODE_BROWSE, MODE_LOADING, MODE_ERROR } BrowserMode;

typedef enum { EL_NONE, EL_ADDR, EL_BACK, EL_FORWARD, EL_RELOAD, EL_LINK } ElType;

typedef struct {
    ElType type;
    float x, y, w, h;
    int data; // link index, for EL_LINK
} UIElement;

typedef struct {
    char url[512];
    char status[256];
    ParsedPage page;
    BrowserMode mode;

    // pending navigation - actually fetched in the main loop, after one
    // "Loading..." frame has been drawn, so the UI doesn't freeze silently.
    int pending_fetch;
    char fetch_url[512];
    int fetch_push;

    char *history[MAX_HISTORY];
    int history_count;
    int history_pos;

    float cursor_x, cursor_y;

    int scroll_line;
    int link_scroll;

    C2D_TextBuf page_buf;
    C2D_Text page_lines[MAX_LINES];
    char *page_line_text[MAX_LINES];
    int page_line_count;

    C2D_TextBuf link_buf;
    C2D_Text link_texts[MAX_LINK_ROWS];
    int link_text_count;

    C2D_TextBuf scratch_buf;

    UIElement elements[MAX_ELEMENTS];
    int element_count;
    int hover_index;
} BrowserState;

void browser_init(BrowserState *st, const char *start_url);
void browser_navigate(BrowserState *st, const char *url, int push_history);
void browser_do_fetch(BrowserState *st);
void browser_cleanup(BrowserState *st);

void ui_update(BrowserState *st, circlePosition *cpad, u32 kDown);
void ui_draw_top(BrowserState *st);
void ui_draw_bottom(BrowserState *st);

#endif
