#include "term.h"
#include "common.h"
#include <termios.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int tcsetattr_ttou(int fd, int optional_actions, const struct termios *p);

class canonical_termios_t
{
 public:
  canonical_termios_t()
   : restore_tios(false)
  {
    if (tcgetattr(0, &old_tios) == 0)
    {
      struct termios new_tios = old_tios;
      new_tios.c_lflag &= ~(ICANON | ECHO);
      if (tcsetattr_ttou(0, TCSANOW, &new_tios) == 0)
        restore_tios = true;
    }
  }

  ~canonical_termios_t()
  {
    if (restore_tios)
      tcsetattr_ttou(0, TCSANOW, &old_tios);
  }
 private:
  struct termios old_tios;
  bool restore_tios;
};

static canonical_termios_t tios; // exit() will clean up for us

// The console device polls for input on every HTIF tick, i.e. every few
// thousand target instructions, and each poll is a system call.  Between
// bursts of input the host is only asked once per POLL_INTERVAL_NS of host
// time, which is invisible to a user typing, and never again once standard
// input has ended.
static const long POLL_INTERVAL_NS = 1000000;
static struct timespec last_poll;
static bool poll_pending = true;
static bool input_ended = false;

int canonical_terminal_t::read()
{
  if (input_ended)
    return -1;

  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  if (!poll_pending) {
    long elapsed_ns = (now.tv_sec - last_poll.tv_sec) * 1000000000L + (now.tv_nsec - last_poll.tv_nsec);
    if (elapsed_ns >= 0 && elapsed_ns < POLL_INTERVAL_NS)
      return -1;
  }
  last_poll = now;
  poll_pending = false;

  struct pollfd pfd;
  pfd.fd = 0;
  pfd.events = POLLIN;
  int ret = poll(&pfd, 1, 0);
  if (ret <= 0 || !(pfd.revents & POLLIN))
    return -1;

  unsigned char ch;
  ret = ::read(0, &ch, 1);
  if (ret == 0)
    input_ended = true;
  else if (ret > 0)
    poll_pending = true; // drain a burst of input promptly
  return ret <= 0 ? -1 : ch;
}

void canonical_terminal_t::write(char ch)
{
  if (::write(1, &ch, 1) != 1)
    abort();
}

static volatile sig_atomic_t sigttou_caught;

static void sigttou_handler(int UNUSED signum) {
  sigttou_caught = 1;
}

static int tcsetattr_ttou(int fd, int optional_actions, const struct termios *p)
{
  struct sigaction sa, old_sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sigttou_handler;
  sigemptyset(&sa.sa_mask);

  if (sigaction(SIGTTOU, &sa, &old_sa))
    abort();

  sigttou_caught = 0;

  int result = tcsetattr(fd, optional_actions, p);

  if (sigttou_caught) {
    sigaction(SIGTTOU, &old_sa, NULL);
    return -1;
  }

  if (sigaction(SIGTTOU, &old_sa, NULL))
    abort();

  return result;
}
