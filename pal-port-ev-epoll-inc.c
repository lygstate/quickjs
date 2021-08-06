#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "pal-port.h"

#ifndef PAL_EV_EPOLL_DEFER_DELETES
#define PAL_EV_EPOLL_DEFER_DELETES 1
#endif

typedef struct pal_ev_loop_epoll_t {
    pal_ev_loop_t loop;
    int epfd;
    struct epoll_event events[1024];
} pal_ev_loop_epoll_t;

pal_ev_globals_t pal_ev;

pal_ev_loop_t *pal_ev_create_loop(int max_timeout)
{
    pal_ev_loop_epoll_t *loop;

    /* init parent */
    assert(PAL_EV_IS_INITED);
    if ((loop = (pal_ev_loop_epoll_t *)malloc(sizeof(pal_ev_loop_epoll_t))) == NULL) {
        return NULL;
    }
    if (pal_ev_init_loop_internal(&loop->loop, max_timeout) != 0) {
        free(loop);
        return NULL;
    }

    /* init myself */
    if ((loop->epfd = epoll_create(pal_ev.max_fd)) == -1) {
        pal_ev_deinit_loop_internal(&loop->loop);
        free(loop);
        return NULL;
    }
    return &loop->loop;
}

int pal_ev_destroy_loop(pal_ev_loop_t *_loop)
{
    pal_ev_loop_epoll_t *loop = (pal_ev_loop_epoll_t *)_loop;

    if (close(loop->epfd) != 0) {
        return -1;
    }
    pal_ev_deinit_loop_internal(&loop->loop);
    free(loop);
    return 0;
}

int pal_ev_update_events_internal(pal_ev_loop_t *_loop, int fd, int events)
{
    pal_ev_loop_epoll_t *loop = (pal_ev_loop_epoll_t *)_loop;
    pal_ev_fd_t *target = pal_ev.fds + fd;
    struct epoll_event ev;
    int epoll_ret;

    memset(&ev, 0, sizeof(ev));
    assert(PAL_EV_FD_BELONGS_TO_LOOP(&loop->loop, fd));

    if ((events & PAL_EV_READWRITE) == target->events) {
        return 0;
    }

    ev.events = ((events & PAL_EV_READ) != 0 ? EPOLLIN : 0) | ((events & PAL_EV_WRITE) != 0 ? EPOLLOUT : 0);
    ev.data.fd = fd;

#define SET(op, check_error)                            \
    do {                                                \
        epoll_ret = epoll_ctl(loop->epfd, op, fd, &ev); \
        assert(!check_error || epoll_ret == 0);         \
    } while (0)

#if PAL_EV_EPOLL_DEFER_DELETES

    if ((events & PAL_EV_DEL) != 0) {
        /* nothing to do */
    } else if ((events & PAL_EV_READWRITE) == 0) {
        SET(EPOLL_CTL_DEL, 1);
    } else {
        SET(EPOLL_CTL_MOD, 0);
        if (epoll_ret != 0) {
            assert(errno == ENOENT);
            SET(EPOLL_CTL_ADD, 1);
        }
    }

#else

    if ((events & PAL_EV_READWRITE) == 0) {
        SET(EPOLL_CTL_DEL, 1);
    } else {
        SET(target->events == 0 ? EPOLL_CTL_ADD : EPOLL_CTL_MOD, 1);
    }

#endif

#undef SET

    target->events = events;

    return 0;
}

int pal_ev_poll_once_internal(pal_ev_loop_t *_loop, int max_wait)
{
    pal_ev_loop_epoll_t *loop = (pal_ev_loop_epoll_t *)_loop;
    int i, nevents;

    nevents = epoll_wait(loop->epfd, loop->events,
                         sizeof(loop->events) / sizeof(loop->events[0]),
                         max_wait * 1000);
    if (nevents == -1) {
        return -1;
    }
    for (i = 0; i < nevents; ++i) {
        struct epoll_event *event = loop->events + i;
        pal_ev_fd_t *target = pal_ev.fds + event->data.fd;
        if (loop->loop.loop_id == target->loop_id && (target->events & PAL_EV_READWRITE) != 0) {
            int revents = ((event->events & EPOLLIN) != 0 ? PAL_EV_READ : 0) | ((event->events & EPOLLOUT) != 0 ? PAL_EV_WRITE : 0);
            if (revents != 0) {
                (*target->callback)(&loop->loop, event->data.fd, revents,
                                    target->cb_arg);
            }
        } else {
#if PAL_EV_EPOLL_DEFER_DELETES
            event->events = 0;
            epoll_ctl(loop->epfd, EPOLL_CTL_DEL, event->data.fd, event);
#endif
        }
    }
    return 0;
}
