#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <stdlib.h>
#include <string.h>

int js_debugger_sockaddr_length(void *sock_addr)
{
    struct sockaddr_storage *addr_storage = sock_addr;
    if (addr_storage->ss_family == AF_INET) {
        return sizeof(struct sockaddr_in);
    } else if (addr_storage->ss_family == AF_INET6) {
        return sizeof(struct sockaddr_in6);
    }
    return -1;
}

int js_debugger_parse_sockaddr(const char *address, void *sock_addr)
{
    char *port_string = strstr(address, ":");
    if (!port_string) {
        return -1;
    }

    int port = atoi(port_string + 1);
    if (port == 0) {
        return -1;
    }
    int port_net = htons(port);

    char host_string[256];
    strcpy(host_string, address);
    host_string[port_string - address] = 0;

    struct sockaddr_storage peer_addr;
    struct sockaddr_in *addr_in4 = (struct sockaddr_in *)&peer_addr;
    struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)&peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));

    struct hostent *host = gethostbyname(host_string);
    if (host != NULL) {
        if (host->h_addrtype == AF_INET) {
            addr_in4->sin_family = host->h_addrtype;
            addr_in4->sin_port = port_net;
            memcpy((char *)&(addr_in4->sin_addr), (char *)host->h_addr, host->h_length);
        } else if (host->h_addrtype == AF_INET6) {
            addr_in6->sin6_family = host->h_addrtype;
            addr_in6->sin6_port = port_net;
            memcpy((char *)&(addr_in6->sin6_addr), (char *)host->h_addr, host->h_length);
        }
    } else if (inet_pton(AF_INET, host_string, &(addr_in4->sin_addr)) == 1) {
        addr_in4->sin_family = AF_INET;
        addr_in4->sin_port = port_net;
    } else if (inet_pton(AF_INET6, host_string, &(addr_in6->sin6_addr)) == 1) {
        addr_in6->sin6_family = AF_INET6;
        addr_in6->sin6_port = port_net;
    }

    if (peer_addr.ss_family == AF_UNSPEC) {
        return -1;
    }
    memcpy(sock_addr, &peer_addr, sizeof(peer_addr));
    return 0;
}
