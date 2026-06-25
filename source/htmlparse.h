#ifndef HTMLPARSE_H
#define HTMLPARSE_H

typedef struct {
    char *url;
    char *text;
} Link;

typedef struct {
    char *title;
    char *text;       // plain-text rendering of the page, links shown as [Link Text]
    Link *links;
    int link_count;
} ParsedPage;

// Parses raw HTML into plain text + a list of links (hrefs resolved against base_url).
// Always returns 0. Fills *out; caller must call html_free(out) when done.
int html_parse(const char *html, const char *base_url, ParsedPage *out);
void html_free(ParsedPage *page);

#endif
