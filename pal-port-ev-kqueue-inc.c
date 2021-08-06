#include <errno.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

#include "pal-port.h"

#define EV_QUEUE_SZ 128

#define BACKEND_BUILD(next_fd, events)	\
  ((unsigned)((next_fd << 8) | (events & 0xff)))
#define BACKEND_GET_NEXT_FD(backend) ((int)(backend) >> 8)
#define BACKEND_GET_OLD_EVENTS(backend) ((int)(backend) & 0xff)

typedef struct pal_ev_loop_kqueue_t {
  pal_ev_loop_t loop;
  int kq;
  int changed_fds; /* link list using pal_ev_fd_t::_backend, -1 if not changed */
  struct kevent events[1024];
  struct kevent changelist[256];
} pal_ev_loop_kqueue_t;

pal_ev_globals_t pal_ev;

static int apply_pending_changes(pal_ev_loop_kqueue_t* loop, int apply_all)
{
#define SET(op, events)						\
  EV_SET(loop->changelist + cl_off++, loop->changed_fds,	\
	 (((events) & PAL_EV_READ) != 0 ? EVFILT_READ : 0)	\
	 | (((events) & PAL_EV_WRITE) != 0 ? EVFILT_WRITE : 0), \
	 (op), 0, 0, NULL)

  int cl_off = 0, nevents;

  while (loop->changed_fds != -1) {
    pal_ev_fd_t* changed = pal_ev.fds + loop->changed_fds;
    int old_events = BACKEND_GET_OLD_EVENTS(changed->_backend);
    if (changed->events != old_events) {
      if (old_events != 0) {
	SET(EV_DISABLE, old_events);
      }
      if (changed->events != 0) {
	SET(EV_ADD | EV_ENABLE, changed->events);
      }
      if ((size_t)cl_off + 1
	  >= sizeof(loop->changelist) / sizeof(loop->changelist[0])) {
	nevents = kevent(loop->kq, loop->changelist, cl_off, NULL, 0, NULL);
	assert(nevents == 0);
	cl_off = 0;
      }
    }
    loop->changed_fds = BACKEND_GET_NEXT_FD(changed->_backend);
    changed->_backend = -1;
  }

  if (apply_all && cl_off != 0) {
    nevents = kevent(loop->kq, loop->changelist, cl_off, NULL, 0, NULL);
    assert(nevents == 0);
    cl_off = 0;
  }

  return cl_off;

#undef SET
}

pal_ev_loop_t* pal_ev_create_loop(int max_timeout)
{
  pal_ev_loop_kqueue_t* loop;

  /* init parent */
  assert(PAL_EV_IS_INITED);
  if ((loop = (pal_ev_loop_kqueue_t*)malloc(sizeof(pal_ev_loop_kqueue_t)))
      == NULL) {
    return NULL;
  }
  if (pal_ev_init_loop_internal(&loop->loop, max_timeout) != 0) {
    free(loop);
    return NULL;
  }

  /* init kqueue */
  if ((loop->kq = kqueue()) == -1) {
    pal_ev_deinit_loop_internal(&loop->loop);
    free(loop);
    return NULL;
  }
  loop->changed_fds = -1;
  return &loop->loop;
}

int pal_ev_destroy_loop(pal_ev_loop_t* _loop)
{
  pal_ev_loop_kqueue_t* loop = (pal_ev_loop_kqueue_t*)_loop;

  if (close(loop->kq) != 0) {
    return -1;
  }
  pal_ev_deinit_loop_internal(&loop->loop);
  free(loop);
  return 0;
}

int pal_ev_update_events_internal(pal_ev_loop_t* _loop, int fd, int events)
{
  pal_ev_loop_kqueue_t* loop = (pal_ev_loop_kqueue_t*)_loop;
  pal_ev_fd_t* target = pal_ev.fds + fd;

  assert(PAL_EV_FD_BELONGS_TO_LOOP(&loop->loop, fd));

  /* initialize if adding the fd */
  if ((events & PAL_EV_ADD) != 0) {
    target->_backend = -1;
  }
  /* return if nothing to do */
  if (events == PAL_EV_DEL
      ? target->_backend == -1
      : (events & PAL_EV_READWRITE) == target->events) {
    return 0;
  }
  /* add to changed list if not yet being done */
  if (target->_backend == -1) {
    target->_backend = BACKEND_BUILD(loop->changed_fds, target->events);
    loop->changed_fds = fd;
  }
  /* update events */
  target->events = events & PAL_EV_READWRITE;
  /* apply immediately if is a DELETE */
  if ((events & PAL_EV_DEL) != 0) {
    apply_pending_changes(loop, 1);
  }

  return 0;
}

int pal_ev_poll_once_internal(pal_ev_loop_t* _loop, int max_wait)
{
  pal_ev_loop_kqueue_t* loop = (pal_ev_loop_kqueue_t*)_loop;
  struct timespec ts;
  int cl_off = 0, nevents, i;

  /* apply pending changes, with last changes stored to loop->changelist */
  cl_off = apply_pending_changes(loop, 0);

  ts.tv_sec = max_wait;
  ts.tv_nsec = 0;
  nevents = kevent(loop->kq, loop->changelist, cl_off, loop->events,
		   sizeof(loop->events) / sizeof(loop->events[0]), &ts);
  if (nevents == -1) {
    /* the errors we can only rescue */
    assert(errno == EACCES || errno == EFAULT || errno == EINTR);
    return -1;
  }
  for (i = 0; i < nevents; ++i) {
    struct kevent* event = loop->events + i;
    pal_ev_fd_t* target = pal_ev.fds + event->ident;
    assert((event->flags & EV_ERROR) == 0); /* changelist errors are fatal */
    if (loop->loop.loop_id == target->loop_id
	&& (event->filter & (EVFILT_READ | EVFILT_WRITE)) != 0) {
      int revents;
      switch (event->filter) {
      case EVFILT_READ:
	revents = PAL_EV_READ;
	break;
      case EVFILT_WRITE:
	revents = PAL_EV_WRITE;
	break;
      default:
	assert(0);
	revents = 0; // suppress compiler warning
	break;
      }
      (*target->callback)(&loop->loop, event->ident, revents, target->cb_arg);
    }
  }

  return 0;
}
