#include "htmlparse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

static char *xstrndup(const char *s, size_t n) {
    char *r = (char *)malloc(n + 1);
    memcpy(r, s, n);
    r[n] = 0;
    return r;
}

static void decode_entities(char *s) {
    char *src = s, *dst = s;
    while (*src) {
        if (*src == '&') {
            if (!strncmp(src, "&amp;", 5)) { *dst++ = '&'; src += 5; continue; }
            if (!strncmp(src, "&lt;", 4))  { *dst++ = '<'; src += 4; continue; }
            if (!strncmp(src, "&gt;", 4))  { *dst++ = '>'; src += 4; continue; }
            if (!strncmp(src, "&quot;", 6)) { *dst++ = '"'; src += 6; continue; }
            if (!strncmp(src, "&#39;", 5))  { *dst++ = '\''; src += 5; continue; }
            if (!strncmp(src, "&apos;", 6)) { *dst++ = '\''; src += 6; continue; }
            if (!strncmp(src, "&nbsp;", 6)) { *dst++ = ' '; src += 6; continue; }
        }
        *dst++ = *src++;
    }
    *dst = 0;
}

// Resolves an href against base_url. Caller frees the result.
static char *resolve_url(const char *base, const char *href) {
    if (strncasecmp(href, "http://", 7) == 0 || strncasecmp(href, "https://", 8) == 0) {
        return strdup(href);
    }
    if (href[0] == 0 || href[0] == '#') {
        return strdup(base);
    }

    const char *scheme_sep = strstr(base, "://");
    if (!scheme_sep) return strdup(href);

    const char *after_scheme = scheme_sep + 3;
    const char *path_start = strchr(after_scheme, '/');
    size_t origin_len = path_start ? (size_t)(path_start - base) : strlen(base);

    if (href[0] == '/') {
        char *out = (char *)malloc(origin_len + strlen(href) + 1);
        memcpy(out, base, origin_len);
        strcpy(out + origin_len, href);
        return out;
    }

    const char *last_slash = path_start ? strrchr(path_start, '/') : NULL;
    size_t dir_len = last_slash ? (size_t)(last_slash - base + 1) : (origin_len + 1);
    char *out = (char *)malloc(dir_len + strlen(href) + 1);
    memcpy(out, base, dir_len);
    strcpy(out + dir_len, href);
    return out;
}

int html_parse(const char *html, const char *base_url, ParsedPage *out) {
    memset(out, 0, sizeof(*out));
    size_t in_len = strlen(html);

    char *text = (char *)malloc(in_len * 2 + 1024);
    size_t tlen = 0;

    int link_cap = 16;
    out->links = (Link *)malloc(sizeof(Link) * link_cap);
    out->link_count = 0;

    const char *p = html;

    while (*p) {
        if (*p == '<') {
            const char *q = p + 1;
            int closing = 0;
            if (*q == '/') { closing = 1; q++; }

            char name[16];
            int ni = 0;
            while (*q && (isalnum((unsigned char)*q)) && ni < 15) name[ni++] = (char)tolower((unsigned char)*q++);
            name[ni] = 0;

            if (!closing && (!strcmp(name, "script") || !strcmp(name, "style"))) {
                const char *close_tag = !strcmp(name, "script") ? "</script" : "</style";
                const char *end = strcasestr(p, close_tag);
                if (end) {
                    const char *gt = strchr(end, '>');
                    p = gt ? gt + 1 : p + strlen(p);
                } else {
                    p += strlen(p);
                }
                continue;
            }

            if (!closing && !strcmp(name, "title")) {
                const char *gt = strchr(p, '>');
                if (gt) {
                    const char *tstart = gt + 1;
                    const char *tend = strcasestr(tstart, "</title");
                    if (tend && !out->title) {
                        out->title = xstrndup(tstart, (size_t)(tend - tstart));
                        decode_entities(out->title);
                    }
                }
            }

            if (!closing && !strcmp(name, "a")) {
                const char *gt = strchr(p, '>');
                if (gt) {
                    const char *href_attr = strcasestr(p, "href=");
                    if (href_attr && href_attr < gt) {
                        const char *v = href_attr + 5;
                        char quote = 0;
                        if (*v == '"' || *v == '\'') { quote = *v; v++; }
                        const char *vend = quote ? strchr(v, quote) : v;
                        if (quote && vend && vend < gt) {
                            char *href = xstrndup(v, (size_t)(vend - v));
                            const char *tstart = gt + 1;
                            const char *tend = strcasestr(tstart, "</a");
                            char *ltext = tend ? xstrndup(tstart, (size_t)(tend - tstart)) : strdup(href);

                            // strip nested tags from the link text
                            char *s = ltext, *d = ltext;
                            int intag = 0;
                            while (*s) {
                                if (*s == '<') { intag = 1; s++; continue; }
                                if (*s == '>') { intag = 0; s++; continue; }
                                if (!intag) *d++ = *s;
                                s++;
                            }
                            *d = 0;
                            decode_entities(ltext);
                            if (ltext[0] == 0) { free(ltext); ltext = strdup(href); }

                            if (out->link_count >= link_cap) {
                                link_cap *= 2;
                                out->links = (Link *)realloc(out->links, sizeof(Link) * link_cap);
                            }
                            out->links[out->link_count].url = resolve_url(base_url, href);
                            out->links[out->link_count].text = ltext;
                            out->link_count++;
                            free(href);

                            text[tlen++] = '[';
                            size_t llen = strlen(ltext);
                            memcpy(text + tlen, ltext, llen);
                            tlen += llen;
                            text[tlen++] = ']';
                        }
                    }
                }
            }

            if ((closing && (!strcmp(name, "p") || !strcmp(name, "div") || !strcmp(name, "li") ||
                              !strcmp(name, "h1") || !strcmp(name, "h2") || !strcmp(name, "h3") ||
                              !strcmp(name, "h4") || !strcmp(name, "tr"))) ||
                (!closing && !strcmp(name, "br"))) {
                text[tlen++] = '\n';
            }

            const char *gt = strchr(p, '>');
            p = gt ? gt + 1 : p + strlen(p);
            continue;
        }
        text[tlen++] = *p++;
    }
    text[tlen] = 0;
    decode_entities(text);

    // collapse runs of whitespace, keep single newlines as paragraph breaks
    char *s = text, *d = text;
    int last_space = 1, last_nl = 0;
    while (*s) {
        if (*s == '\n') {
            if (!last_nl) { *d++ = '\n'; last_nl = 1; last_space = 1; }
        } else if (isspace((unsigned char)*s)) {
            if (!last_space) { *d++ = ' '; last_space = 1; }
        } else {
            *d++ = *s;
            last_space = 0;
            last_nl = 0;
        }
        s++;
    }
    *d = 0;

    out->text = text;
    return 0;
}

void html_free(ParsedPage *page) {
    free(page->title);
    free(page->text);
    for (int i = 0; i < page->link_count; i++) {
        free(page->links[i].url);
        free(page->links[i].text);
    }
    free(page->links);
    memset(page, 0, sizeof(*page));
}
