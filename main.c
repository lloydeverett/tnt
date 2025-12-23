#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <termios.h>
#include "tmt.h"
#include "pty_popen.h"

#define TERMINAL_HEIGHT ((size_t)10)

/* Forward declaration of a callback.
 * libtmt will call this function when the terminal's state changes.
 */
void callback(tmt_msg_t m, TMT *vt, const void *a, void *p);

/* Check if we're rendering for the first time.
 */
bool first_render = true;

/* Holds pty master file descriptor.
 */
int pty_fd;

void* write_stdin_thread(void* arg) {
    int tty_fd = open("/dev/tty", O_RDWR);
    char buf[1024];
    ssize_t nread;
    // Continuously read from stdin and write to the PTY master
    while ((nread = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        write(pty_fd, buf, nread);
    }
    if (nread == -1) {
        perror("read stdin error");
    }
    return NULL;
}

struct termios original_mode;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_mode);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &original_mode);

    atexit(disable_raw_mode);

    struct termios raw = original_mode;

    cfmakeraw(&raw);

    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

int
main(void)
{
    enable_raw_mode();
    atexit(disable_raw_mode);

    /* Retrieve terminal dimensions.
     */
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    /* Start child process.
     */
    const char* command[] = { "/bin/sh", 0 };
    struct winsize w_child;
    w_child.ws_row = TERMINAL_HEIGHT;
    w_child.ws_col = w.ws_col;
    w_child.ws_xpixel = 0;
    w_child.ws_ypixel = 0;
    int pid = pty_popen(command, &pty_fd, &original_mode, &w_child);

    /* Open a virtual terminal.
     *
     * The first NULL is just a pointer that will be provided to the
     * callback; it can be anything. The second NULL specifies that
     * we want to use the default Alternate Character Set; this
     * could be a pointer to a wide string that has the desired
     * characters to be displayed when in ACS mode.
     */
    TMT *vt = tmt_open(TERMINAL_HEIGHT, w.ws_col, callback, NULL, NULL);
    if (!vt)
        return perror("could not allocate terminal"), EXIT_FAILURE;

    /* Create a thread to continously write parent stdin to child process.
     */
    pthread_t thread;
    if (pthread_create(&thread, NULL, write_stdin_thread, NULL) != 0) {
        perror("pthread_create failed");
        exit(EXIT_FAILURE);
    }

    /* Continously write child stdout to virtual terminal.
     */
    char buffer[1024];
    int bytes_read;
    while ((bytes_read = read(pty_fd, buffer, sizeof(buffer))) > 0) {
        tmt_write(vt, buffer, bytes_read);
    }

    /* Writing input to the virtual terminal can (and in this case, did)
     * call the callback letting us know the screen was updated. See the
     * callback below to see how that works.
     */
    tmt_close(vt);

    /* Wait for the child process to close.
     */
    waitpid(pid, NULL, 0);

    return EXIT_SUCCESS;
}

void
callback(tmt_msg_t m, TMT *vt, const void *a, void *p)
{
    /* grab a pointer to the virtual screen */
    const TMTSCREEN *s = tmt_screen(vt);
    const TMTPOINT *c = tmt_cursor(vt);

    switch (m){
        case TMT_MSG_BELL:
            printf("\a");
            break;

        case TMT_MSG_UPDATE:
            /* the screen image changed; a is a pointer to the TMTSCREEN */

            /* move cursor up by prev_nline lines */
            putchar('\r');
            if (!first_render) {
                printf("\033[%zuA", TERMINAL_HEIGHT);
            }
            first_render = false;

            /* render new buffer */
            for (size_t r = 0; r < s->nline; r++){
                if (s->lines[r]->dirty) {
                    for (size_t c = 0; c < s->ncol; c++){
                        putchar(s->lines[r]->chars[c].c);
                    }
                }
                putchar('\r');
                putchar('\n');
            }

            /* let tmt know we've redrawn the screen */
            tmt_clean(vt);
            break;

        case TMT_MSG_ANSWER:
            /* ignore for now */
            break;

        case TMT_MSG_CURSOR:
            /* ignore for now */
            break;

        case TMT_MSG_MOVED:
            /* ignore for now */
            break;
    }
}

