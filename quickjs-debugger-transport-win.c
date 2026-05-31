#include "quickjs-debugger-transport.h"
#include "quickjs-debugger.h"

#include <assert.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <WS2tcpip.h>
#include <winsock2.h>

typedef intptr_t ssize_t;

static void __dump(const char *desc, const char *buffer, ssize_t length)
{

    printf("%s (%d)\n", desc, (int)length);

    for (ssize_t i = 0; i < length; i += 8) {

        size_t j,
            lim = i + 8;

        if (lim > length) {
            lim = length;
        }

        for (j = i; j < lim; j++) {
            printf("%02X ", buffer[j]);
        }

        while (j < i + 8) {
            printf("   ");
            j++;
        }

        printf(" | ");

        for (size_t j = i; j < lim; j++) {
            printf("%c", buffer[j]);
        }

        printf("\n");
    }
}

static int js_transport_read(void *udata, char *buffer, int length)
{
    struct js_transport_data *data = (struct js_transport_data *)udata;
    if (data->handle <= 0)
        return -1;

    if (length <= 0)
        return -2;

    if (buffer == NULL)
        return -3;

    int ret = recv(data->handle, (void *)buffer, length, 0);

    if (ret == SOCKET_ERROR)
        return -4;

    if (ret == 0)
        return -5;

    if (ret > length)
        return -6;

    return ret;
}

static int js_transport_write(void *udata, const char *buffer, int length)
{
    struct js_transport_data *data = (struct js_transport_data *)udata;
    if (data->handle <= 0)
        return -1;

    if (length <= 0)
        return -2;

    if (buffer == NULL) {
        return -3;
    }

    int ret = send(data->handle, (const void *)buffer, length, 0);
    if (ret <= 0 || ret > length)
        return -4;

    return ret;
}

static int js_transport_peek(void *udata)
{
    WSAPOLLFD fds[1];
    int poll_rc;

    struct js_transport_data *data = (struct js_transport_data *)udata;
    if (data->handle <= 0)
        return -1;

    fds[0].fd = data->handle;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    poll_rc = WSAPoll(fds, 1, 0);
    if (poll_rc < 0)
        return -2;
    if (poll_rc > 1)
        return -3;
    // no data
    if (poll_rc == 0)
        return 0;
    // has data
    return 1;
}

static void js_transport_close(JSRuntime *rt, void *udata)
{
    struct js_transport_data *data = (struct js_transport_data *)udata;
    if (data->handle <= 0)
        return;

    closesocket(data->handle);
    data->handle = 0;

    free(udata);

    WSACleanup();
}

void js_debugger_connect(JSContext *ctx, const char *address)
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    struct sockaddr_storage addr;
    int parse_addr_result = js_debugger_parse_sockaddr(address, &addr);
    assert(parse_addr_result == 0);

    int client = socket(addr.ss_family, SOCK_STREAM, 0);
    assert(client >= 0);

    int connect_result = connect(client, (const struct sockaddr *)&addr, js_debugger_sockaddr_length(&addr));
    assert(connect_result >= 0);

    struct js_transport_data *data = (struct js_transport_data *)malloc(sizeof(struct js_transport_data));
    data->handle = client;
    js_debugger_attach(ctx, js_transport_read, js_transport_write, js_transport_peek, js_transport_close, data);
}

void js_debugger_wait_connection(JSContext *ctx, const char *address)
{
    struct sockaddr_in addr;
    int parse_addr_result = js_debugger_parse_sockaddr(address, &addr);
    assert(parse_addr_result >= 0);

    int server = socket(AF_INET, SOCK_STREAM, 0);
    assert(server >= 0);

    int reuseAddress = 1;
    int setsock_result = setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuseAddress, sizeof(reuseAddress));
    assert(setsock_result >= 0);

    int bind_result = bind(server, (struct sockaddr *)&addr, sizeof(addr));
    assert(bind_result >= 0);

    listen(server, 1);

    struct sockaddr_in client_addr;
    socklen_t client_addr_size = (socklen_t)sizeof(addr);
    int client = accept(server, (struct sockaddr *)&client_addr, &client_addr_size);
    close(server);
    assert(client >= 0);

    struct js_transport_data *data = (struct js_transport_data *)malloc(sizeof(struct js_transport_data));
    memset(data, 0, sizeof(js_transport_data));
    data->handle = client;
    js_debugger_attach(ctx, js_transport_read, js_transport_write, js_transport_peek, js_transport_close, (void *)data);
}
