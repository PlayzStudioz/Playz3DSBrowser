#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

// Performs a blocking HTTP GET request. HTTP only (no HTTPS/TLS).
// On success, *out_data is a malloc'd, NUL-terminated buffer containing the
// response body, and *out_len is its length. Caller must free(*out_data).
//
// Return codes:
//   0   success
//  -1   could not parse URL
//  -2   unsupported scheme (e.g. https)
//  -3   DNS lookup failed
//  -4   could not create socket
//  -5   could not connect
//  -6   send() failed
//  -7   out of memory
//  -8   out of memory (growing buffer)
int http_get(const char *url, char **out_data, size_t *out_len);

#endif
