#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

typedef struct {
    char scheme[8];
    char host[256];
    int port;
    char path[1024];
} UrlParts;

static int parse_url(const char *url, UrlParts *out) {
    out->port = 80;
    strcpy(out->path, "/");

    const char *scheme_end = strstr(url, "://");
    if (!scheme_end) return -1;

    size_t scheme_len = (size_t)(scheme_end - url);
    if (scheme_len >= sizeof(out->scheme)) return -1;
    memcpy(out->scheme, url, scheme_len);
    out->scheme[scheme_len] = 0;

    const char *p = scheme_end + 3;
    const char *path_start = strchr(p, '/');
    const char *host_end = path_start ? path_start : p + strlen(p);
    const char *colon = memchr(p, ':', (size_t)(host_end - p));

    size_t host_len;
    if (colon) {
        host_len = (size_t)(colon - p);
        out->port = atoi(colon + 1);
    } else {
        host_len = (size_t)(host_end - p);
    }
    if (host_len == 0 || host_len >= sizeof(out->host)) return -1;
    memcpy(out->host, p, host_len);
    out->host[host_len] = 0;

    if (path_start) {
        strncpy(out->path, path_start, sizeof(out->path) - 1);
        out->path[sizeof(out->path) - 1] = 0;
    }
    return 0;
}

int http_get(const char *url, char **out_data, size_t *out_len) {
    UrlParts u;
    if (parse_url(url, &u) != 0) return -1;
    if (strcasecmp(u.scheme, "http") != 0) return -2;

    struct hostent *he = gethostbyname(u.host);
    if (!he || !he->h_addr_list[0]) return -3;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -4;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)u.port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -5;
    }

    char req[1600];
    int req_len = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: 3DSBrowser/0.1 (Nintendo 3DS homebrew)\r\n"
        "Accept: text/html,*/*\r\n"
        "Connection: close\r\n"
        "\r\n",
        u.path, u.host);

    if (send(sock, req, (size_t)req_len, 0) < 0) {
        close(sock);
        return -6;
    }

    size_t cap = 16384, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) { close(sock); return -7; }

    char tmp[2048];
    int n;
    while ((n = recv(sock, tmp, sizeof(tmp), 0)) > 0) {
        if (len + (size_t)n + 1 > cap) {
            size_t newcap = cap * 2;
            while (newcap < len + (size_t)n + 1) newcap *= 2;
            char *nb = (char *)realloc(buf, newcap);
            if (!nb) { free(buf); close(sock); return -8; }
            buf = nb;
            cap = newcap;
        }
        memcpy(buf + len, tmp, (size_t)n);
        len += (size_t)n;
    }
    buf[len] = 0;
    close(sock);

    char *body = strstr(buf, "\r\n\r\n");
    int chunked = 0;
    char headers_copy[2048];
    if (body) {
        size_t hlen = (size_t)(body - buf);
        if (hlen >= sizeof(headers_copy)) hlen = sizeof(headers_copy) - 1;
        memcpy(headers_copy, buf, hlen);
        headers_copy[hlen] = 0;
        for (char *c = headers_copy; *c; c++) *c = (char)tolower((unsigned char)*c);
        if (strstr(headers_copy, "transfer-encoding: chunked")) chunked = 1;
        body += 4;
    } else {
        body = buf + len;
    }

    if (chunked) {
        size_t body_len = len - (size_t)(body - buf);
        char *decoded = (char *)malloc(body_len + 1);
        size_t dpos = 0;
        char *p = body;
        char *end = buf + len;
        while (p < end) {
            char *line_end = strstr(p, "\r\n");
            if (!line_end) break;
            *line_end = 0;
            long chunk_size = strtol(p, NULL, 16);
            *line_end = '\r';
            p = line_end + 2;
            if (chunk_size <= 0) break;
            if (p + chunk_size > end) chunk_size = end - p;
            memcpy(decoded + dpos, p, (size_t)chunk_size);
            dpos += (size_t)chunk_size;
            p += chunk_size + 2;
        }
        decoded[dpos] = 0;
        free(buf);
        *out_data = decoded;
        *out_len = dpos;
    } else {
        size_t body_len = len - (size_t)(body - buf);
        char *result = (char *)malloc(body_len + 1);
        memcpy(result, body, body_len);
        result[body_len] = 0;
        *out_data = result;
        *out_len = body_len;
        free(buf);
    }

    return 0;
}
