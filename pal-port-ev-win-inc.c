#include <assert.h>

#include <windows.h>

#include "pal-port.h"

extern pal_ev_globals_t pal_ev;

pal_ev_loop_t *pal_ev_create_loop(int max_timeout)
{
    pal_ev_loop_t *loop;

    assert(PAL_EV_IS_INITED(pal_ev));
    if ((loop = (pal_ev_loop_t *)pal_malloc(sizeof(pal_ev_loop_t))) == NULL) {
        return NULL;
    }
    if (pal_ev_init_loop_internal(loop, max_timeout) != 0) {
        pal_free(loop);
        return NULL;
    }

    return loop;
}

int pal_ev_destroy_loop(pal_ev_loop_t *loop)
{
    pal_ev_deinit_loop_internal(loop);
    free(loop);
    return 0;
}

int pal_ev_update_events_internal(pal_ev_loop_t *loop, int fd, int events)
{
    pal_ev.fds[fd].events = events & PAL_EV_READWRITE;
    return 0;
}

int pal_ev_poll_once_internal(pal_ev_loop_t *loop, int max_wait)
{
    fd_set readfds, writefds, errorfds;
    struct timeval tv;
    int i, r, maxfd = 0;

    /* setup */
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&errorfds);
    for (i = 0; i < pal_ev.max_fd; ++i) {
        pal_ev_fd_t *fd = pal_ev.fds + i;
        if (fd->loop_id == loop->loop_id) {
            if ((fd->events & PAL_EV_READ) != 0) {
                FD_SET(i, &readfds);
                if (maxfd < i) {
                    maxfd = i;
                }
            }
            if ((fd->events & PAL_EV_WRITE) != 0) {
                FD_SET(i, &writefds);
                if (maxfd < i) {
                    maxfd = i;
                }
            }
        }
    }

    /* select and handle if any */
    tv.tv_sec = max_wait;
    tv.tv_usec = 0;
    r = select(maxfd + 1, &readfds, &writefds, &errorfds, &tv);
    if (r == -1) {
        return -1;
    } else if (r > 0) {
        for (i = 0; i < pal_ev.max_fd; ++i) {
            pal_ev_fd_t *target = pal_ev.fds + i;
            if (target->loop_id == loop->loop_id) {
                int revents = (FD_ISSET(i, &readfds) ? PAL_EV_READ : 0) | (FD_ISSET(i, &writefds) ? PAL_EV_WRITE : 0);
                if (revents != 0) {
                    (*target->callback)(loop, i, revents, target->cb_arg);
                }
            }
        }
    }

    return 0;
}
