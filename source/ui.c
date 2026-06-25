#include "ui.h"
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LINE_WRAP_CHARS 58
#define LINE_HEIGHT     14

#define BTN_ROW_Y   8
#define BTN_ROW_H   24
#define ADDR_Y      (BTN_ROW_Y + BTN_ROW_H + 4)
#define ADDR_H      22
#define LIST_TOP    (ADDR_Y + ADDR_H + 4)
#define LIST_BOTTOM 224
#define LINK_ROW_H  20
#define LIST_VISIBLE ((LIST_BOTTOM - LIST_TOP) / LINK_ROW_H)

// ---------------------------------------------------------------- history --

static void history_push(BrowserState *st, const char *url) {
    for (int i = st->history_pos + 1; i < st->history_count; i++) {
        free(st->history[i]);
        st->history[i] = NULL;
    }
    st->history_count = st->history_pos + 1;

    if (st->history_count >= MAX_HISTORY) {
        free(st->history[0]);
        memmove(&st->history[0], &st->history[1], sizeof(char *) * (MAX_HISTORY - 1));
        st->history_count--;
        st->history_pos--;
    }

    st->history[st->history_count] = strdup(url);
    st->history_count++;
    st->history_pos = st->history_count - 1;
}

// ------------------------------------------------------------- text layout --

static void wrap_and_load_text(BrowserState *st) {
    for (int i = 0; i < st->page_line_count; i++) free(st->page_line_text[i]);
    st->page_line_count = 0;
    C2D_TextBufClear(st->page_buf);

    const char *text = st->page.text ? st->page.text : "(empty page)";
    char linebuf[256];
    int lpos = 0;
    char word[160];
    int wpos = 0;

    const char *p = text;
    while (st->page_line_count < MAX_LINES) {
        int is_space = (*p == ' ');
        int is_nl = (*p == '\n');
        int is_end = (*p == 0);

        if (is_space || is_nl || is_end) {
            if (wpos > 0) {
                word[wpos] = 0;
                if (lpos > 0 && lpos + 1 + wpos > LINE_WRAP_CHARS) {
                    linebuf[lpos] = 0;
                    st->page_line_text[st->page_line_count] = strdup(linebuf);
                    C2D_TextParse(&st->page_lines[st->page_line_count], st->page_buf, st->page_line_text[st->page_line_count]);
                    C2D_TextOptimize(&st->page_lines[st->page_line_count]);
                    st->page_line_count++;
                    lpos = 0;
                    if (st->page_line_count >= MAX_LINES) break;
                }
                if (lpos > 0) linebuf[lpos++] = ' ';
                memcpy(linebuf + lpos, word, (size_t)wpos);
                lpos += wpos;
                wpos = 0;
            }
            if (is_nl || (lpos > 0 && is_end)) {
                linebuf[lpos] = 0;
                st->page_line_text[st->page_line_count] = strdup(linebuf);
                C2D_TextParse(&st->page_lines[st->page_line_count], st->page_buf, st->page_line_text[st->page_line_count]);
                C2D_TextOptimize(&st->page_lines[st->page_line_count]);
                st->page_line_count++;
                lpos = 0;
            }
            if (is_end) break;
            p++;
        } else {
            if (wpos < (int)sizeof(word) - 1) word[wpos++] = *p;
            p++;
        }
    }

    st->scroll_line = 0;
}

static void build_link_texts(BrowserState *st) {
    C2D_TextBufClear(st->link_buf);
    st->link_text_count = 0;

    int n = st->page.link_count;
    if (n > MAX_LINK_ROWS) n = MAX_LINK_ROWS;

    for (int i = 0; i < n; i++) {
        char buf[80];
        const char *t = (st->page.links[i].text && st->page.links[i].text[0]) ? st->page.links[i].text : st->page.links[i].url;
        snprintf(buf, sizeof(buf), "> %.74s", t);
        C2D_TextParse(&st->link_texts[st->link_text_count], st->link_buf, buf);
        C2D_TextOptimize(&st->link_texts[st->link_text_count]);
        st->link_text_count++;
    }
}

// ------------------------------------------------------------- navigation --

void browser_navigate(BrowserState *st, const char *url, int push_history) {
    strncpy(st->fetch_url, url, sizeof(st->fetch_url) - 1);
    st->fetch_url[sizeof(st->fetch_url) - 1] = 0;
    st->fetch_push = push_history;
    st->pending_fetch = 1;
    st->mode = MODE_LOADING;
    snprintf(st->status, sizeof(st->status), "Loading %s", st->fetch_url);
}

void browser_do_fetch(BrowserState *st) {
    char *data = NULL;
    size_t len = 0;
    int rc = http_get(st->fetch_url, &data, &len);

    if (rc == -2) {
        st->mode = MODE_ERROR;
        snprintf(st->status, sizeof(st->status), "HTTPS isn't supported yet - try an http:// page.");
        return;
    }
    if (rc != 0) {
        st->mode = MODE_ERROR;
        snprintf(st->status, sizeof(st->status), "Could not load page (error %d). Check the URL and your Wi-Fi.", rc);
        return;
    }

    html_free(&st->page);
    html_parse(data, st->fetch_url, &st->page);
    free(data);

    strncpy(st->url, st->fetch_url, sizeof(st->url) - 1);
    st->url[sizeof(st->url) - 1] = 0;

    if (st->fetch_push) history_push(st, st->url);

    wrap_and_load_text(st);
    build_link_texts(st);

    st->link_scroll = 0;
    st->mode = MODE_BROWSE;
    st->status[0] = 0;
}

void browser_init(BrowserState *st, const char *start_url) {
    memset(st, 0, sizeof(*st));
    st->cursor_x = 160.0f;
    st->cursor_y = 120.0f;
    st->hover_index = -1;

    st->page_buf = C2D_TextBufNew(32768);
    st->link_buf = C2D_TextBufNew(8192);
    st->scratch_buf = C2D_TextBufNew(2048);

    browser_navigate(st, start_url, 1);
}

void browser_cleanup(BrowserState *st) {
    html_free(&st->page);
    for (int i = 0; i < st->page_line_count; i++) free(st->page_line_text[i]);
    for (int i = 0; i < st->history_count; i++) free(st->history[i]);
    C2D_TextBufDelete(st->scratch_buf);
    C2D_TextBufDelete(st->link_buf);
    C2D_TextBufDelete(st->page_buf);
}

// ------------------------------------------------------------------ layout --

static void ui_layout(BrowserState *st) {
    st->element_count = 0;
    UIElement *e;

    e = &st->elements[st->element_count++];
    *e = (UIElement){ EL_BACK, 8, BTN_ROW_Y, 56, BTN_ROW_H, 0 };

    e = &st->elements[st->element_count++];
    *e = (UIElement){ EL_FORWARD, 68, BTN_ROW_Y, 56, BTN_ROW_H, 0 };

    e = &st->elements[st->element_count++];
    *e = (UIElement){ EL_RELOAD, 128, BTN_ROW_Y, 64, BTN_ROW_H, 0 };

    e = &st->elements[st->element_count++];
    *e = (UIElement){ EL_ADDR, 8, ADDR_Y, 304, ADDR_H, 0 };

    int total_links = st->link_text_count;
    int visible = LIST_VISIBLE;
    int max_scroll = total_links - visible;
    if (max_scroll < 0) max_scroll = 0;
    if (st->link_scroll > max_scroll) st->link_scroll = max_scroll;
    if (st->link_scroll < 0) st->link_scroll = 0;

    for (int i = 0; i < visible && (st->link_scroll + i) < total_links; i++) {
        e = &st->elements[st->element_count++];
        *e = (UIElement){ EL_LINK, 8, (float)(LIST_TOP + i * LINK_ROW_H), 304, (float)(LINK_ROW_H - 2), st->link_scroll + i };
    }
}

// ------------------------------------------------------------------ update --

void ui_update(BrowserState *st, circlePosition *cpad, u32 kDown) {
    const float dead = 18.0f;
    const float speed = 6.5f;

    if (fabsf((float)cpad->dx) > dead) st->cursor_x += ((float)cpad->dx / 156.0f) * speed;
    if (fabsf((float)cpad->dy) > dead) st->cursor_y -= ((float)cpad->dy / 156.0f) * speed;

    if (st->cursor_x < 0) st->cursor_x = 0;
    if (st->cursor_x > 320) st->cursor_x = 320;
    if (st->cursor_y < 0) st->cursor_y = 0;
    if (st->cursor_y > 240) st->cursor_y = 240;

    ui_layout(st);

    st->hover_index = -1;
    for (int i = 0; i < st->element_count; i++) {
        UIElement *e = &st->elements[i];
        if (st->cursor_x >= e->x && st->cursor_x <= e->x + e->w &&
            st->cursor_y >= e->y && st->cursor_y <= e->y + e->h) {
            st->hover_index = i;
            break;
        }
    }

    if (kDown & KEY_DUP)   st->scroll_line -= 3;
    if (kDown & KEY_DDOWN) st->scroll_line += 3;
    if (st->scroll_line < 0) st->scroll_line = 0;
    int max_scroll = st->page_line_count - 1;
    if (max_scroll < 0) max_scroll = 0;
    if (st->scroll_line > max_scroll) st->scroll_line = max_scroll;

    if (kDown & KEY_L) st->link_scroll -= 4;
    if (kDown & KEY_R) st->link_scroll += 4;

    if (kDown & KEY_B) {
        if (st->history_pos > 0) {
            st->history_pos--;
            browser_navigate(st, st->history[st->history_pos], 0);
            return;
        }
    }

    if ((kDown & KEY_A) && st->hover_index >= 0 && st->mode == MODE_BROWSE) {
        UIElement *e = &st->elements[st->hover_index];
        switch (e->type) {
            case EL_BACK:
                if (st->history_pos > 0) {
                    st->history_pos--;
                    browser_navigate(st, st->history[st->history_pos], 0);
                }
                break;
            case EL_FORWARD:
                if (st->history_pos < st->history_count - 1) {
                    st->history_pos++;
                    browser_navigate(st, st->history[st->history_pos], 0);
                }
                break;
            case EL_RELOAD:
                browser_navigate(st, st->url, 0);
                break;
            case EL_ADDR: {
                SwkbdState swkbd;
                static char buf[512];
                strncpy(buf, st->url, sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = 0;
                swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
                swkbdSetHintText(&swkbd, "Enter a URL, e.g. http://example.com");
                swkbdSetInitialText(&swkbd, buf);
                SwkbdButton button = swkbdInputText(&swkbd, buf, sizeof(buf));
                if (button == SWKBD_BUTTON_CONFIRM && buf[0]) {
                    char full[512];
                    if (!strstr(buf, "://")) {
                        snprintf(full, sizeof(full), "http://%s", buf);
                    } else {
                        strncpy(full, buf, sizeof(full) - 1);
                        full[sizeof(full) - 1] = 0;
                    }
                    browser_navigate(st, full, 1);
                }
                break;
            }
            case EL_LINK:
                if (e->data >= 0 && e->data < st->page.link_count) {
                    browser_navigate(st, st->page.links[e->data].url, 1);
                }
                break;
            default:
                break;
        }
    }
}

// ------------------------------------------------------------------- draw --

void ui_draw_top(BrowserState *st) {
    if (st->mode == MODE_LOADING || st->mode == MODE_ERROR) {
        C2D_TextBufClear(st->scratch_buf);
        C2D_Text t;
        C2D_TextParse(&t, st->scratch_buf, st->status[0] ? st->status : "...");
        C2D_TextOptimize(&t);
        u32 color = (st->mode == MODE_ERROR) ? C2D_Color32(0xAA, 0x10, 0x10, 0xFF) : C2D_Color32(0x20, 0x20, 0x20, 0xFF);
        C2D_DrawText(&t, C2D_WithColor, 16.0f, 100.0f, 0.5f, 0.6f, 0.6f, color);
        return;
    }

    C2D_DrawRectSolid(0, 0, 0.4f, 400, 18, C2D_Color32(0xE0, 0x40, 0x80, 0xFF));

    C2D_TextBufClear(st->scratch_buf);
    C2D_Text titleText;
    const char *title = (st->page.title && st->page.title[0]) ? st->page.title : st->url;
    C2D_TextParse(&titleText, st->scratch_buf, title);
    C2D_TextOptimize(&titleText);
    C2D_DrawText(&titleText, C2D_WithColor, 6.0f, 3.0f, 0.5f, 0.45f, 0.45f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF));

    float y = 24.0f;
    int max_visible = (240 - 24) / LINE_HEIGHT;
    for (int i = 0; i < max_visible; i++) {
        int idx = st->scroll_line + i;
        if (idx >= st->page_line_count) break;
        C2D_DrawText(&st->page_lines[idx], C2D_WithColor, 6.0f, y, 0.5f, 0.5f, 0.5f, C2D_Color32(0x10, 0x10, 0x10, 0xFF));
        y += LINE_HEIGHT;
    }

    if (st->page_line_count > max_visible) {
        float track_h = 240.0f - 24.0f;
        float thumb_h = track_h * ((float)max_visible / (float)st->page_line_count);
        if (thumb_h < 10) thumb_h = 10;
        float thumb_y = 24.0f + (track_h - thumb_h) * ((float)st->scroll_line / (float)(st->page_line_count - max_visible));
        C2D_DrawRectSolid(396, thumb_y, 0.45f, 3, thumb_h, C2D_Color32(0xC0, 0x40, 0x80, 0xFF));
    }
}

void ui_draw_bottom(BrowserState *st) {
    ui_layout(st);
    C2D_TextBufClear(st->scratch_buf);

    for (int i = 0; i < st->element_count; i++) {
        UIElement *e = &st->elements[i];
        int hovered = (i == st->hover_index);
        u32 bg;
        const char *label = NULL;

        switch (e->type) {
            case EL_BACK:
                label = "< Back";
                bg = (st->history_pos > 0) ? C2D_Color32(0x70, 0x20, 0x45, 0xFF) : C2D_Color32(0x28, 0x18, 0x22, 0xFF);
                break;
            case EL_FORWARD:
                label = "Fwd >";
                bg = (st->history_pos < st->history_count - 1) ? C2D_Color32(0x70, 0x20, 0x45, 0xFF) : C2D_Color32(0x28, 0x18, 0x22, 0xFF);
                break;
            case EL_RELOAD:
                label = "Reload";
                bg = C2D_Color32(0x70, 0x20, 0x45, 0xFF);
                break;
            case EL_ADDR:
                label = st->url[0] ? st->url : "(no page loaded)";
                bg = C2D_Color32(0x0C, 0x18, 0x22, 0xFF);
                break;
            default:
                label = NULL;
                bg = C2D_Color32(0x10, 0x28, 0x30, 0xFF);
                break;
        }
        if (hovered) bg = C2D_Color32(0xFF, 0xE0, 0x40, 0xFF);

        C2D_DrawRectSolid(e->x, e->y, 0.3f, e->w, e->h, bg);

        if (e->type == EL_LINK) {
            if (e->data >= 0 && e->data < st->link_text_count) {
                u32 col = hovered ? C2D_Color32(0, 0, 0, 255) : C2D_Color32(0x40, 0xFF, 0xEE, 0xFF);
                C2D_DrawText(&st->link_texts[e->data], C2D_WithColor, e->x + 4, e->y + 3, 0.4f, 0.42f, 0.42f, col);
            }
        } else if (label) {
            char buf[300];
            strncpy(buf, label, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = 0;
            C2D_Text t;
            C2D_TextParse(&t, st->scratch_buf, buf);
            C2D_TextOptimize(&t);
            u32 col = hovered ? C2D_Color32(0, 0, 0, 255) : C2D_Color32(0xE0, 0xE0, 0xE0, 0xFF);
            C2D_DrawText(&t, C2D_WithColor, e->x + 4, e->y + 4, 0.4f, 0.42f, 0.42f, col);
        }
    }

    C2D_Text hint;
    char hintbuf[100];
    snprintf(hintbuf, sizeof(hintbuf), "Circle Pad: move   A: select   B: back   D-Pad/L/R: scroll   START: quit");
    C2D_TextParse(&hint, st->scratch_buf, hintbuf);
    C2D_TextOptimize(&hint);
    C2D_DrawText(&hint, C2D_WithColor, 4.0f, 228.0f, 0.4f, 0.32f, 0.32f, C2D_Color32(0x40, 0xC0, 0xC0, 0xFF));

    C2D_DrawCircleSolid(st->cursor_x, st->cursor_y, 0.5f, 4.0f, C2D_Color32(0xFF, 0xE0, 0x00, 0xFF));
}
