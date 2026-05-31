#ifndef QUICKJS_DEBUGGER_TRANSPORT_H
#define QUICKJS_DEBUGGER_TRANSPORT_H

struct js_transport_data {
    int handle;
} js_transport_data;

int js_debugger_sockaddr_length(void *sock_addr);
int js_debugger_parse_sockaddr(const char *address, void *sock_addr);

#endif
