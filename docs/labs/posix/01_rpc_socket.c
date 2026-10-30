/*
 * A tiny RPC lab using a Unix socket pair.
 *
 * This is a portable model of a request/reply boundary. It is not Mach IPC
 * and it is not seL4 IPC; use it to make the control flow visible first.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static void die(const char *what) {
    perror(what);
    exit(EXIT_FAILURE);
}

static void write_all(int fd, const char *buf, size_t len) {
    while (len > 0) {
        ssize_t n = write(fd, buf, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            die("write");
        }
        buf += (size_t)n;
        len -= (size_t)n;
    }
}

static ssize_t read_line(int fd, char *buf, size_t cap) {
    size_t used = 0;
    while (used + 1 < cap) {
        char ch;
        ssize_t n = read(fd, &ch, 1);
        if (n == 0) break;
        if (n < 0) {
            if (errno == EINTR) continue;
            die("read");
        }
        buf[used++] = ch;
        if (ch == '\n') break;
    }
    buf[used] = '\0';
    return (ssize_t)used;
}

int main(void) {
    int channel[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, channel) < 0) die("socketpair");

    pid_t child = fork();
    if (child < 0) die("fork");

    if (child == 0) {
        char request[256];
        char reply[320];
        close(channel[0]);

        if (read_line(channel[1], request, sizeof request) == 0) {
            fprintf(stderr, "server: client closed without a request\n");
            _exit(EXIT_FAILURE);
        }
        printf("server: received request: %s", request);
        fflush(stdout);
        int written = snprintf(reply, sizeof reply,
                               "reply: server handled [%s", request);
        if (written < 0 || (size_t)written >= sizeof reply) {
            fprintf(stderr, "server: reply too long\n");
            _exit(EXIT_FAILURE);
        }
        write_all(channel[1], reply, (size_t)written);
        close(channel[1]);
        _exit(EXIT_SUCCESS);
    }

    close(channel[1]);
    const char request[] = "read /hello.txt\n";
    char reply[320];
    printf("client: sending request: %s", request);
    write_all(channel[0], request, sizeof request - 1);
    if (read_line(channel[0], reply, sizeof reply) == 0) die("read reply");
    printf("client: received %s", reply);
    close(channel[0]);

    int status;
    if (waitpid(child, &status, 0) < 0) die("waitpid");
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "client: server failed\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
