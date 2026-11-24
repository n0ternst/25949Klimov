#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "./unix_sock_32"

int main(int argc, char *argv[])
{
    int sock_fd;
    struct sockaddr_un addr;
    const char *msg = (argc > 1) ? argv[1] : "default client message";
    int repeats = (argc > 2) ? atoi(argv[2]) : 5;

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("connect");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    char line[512];
    for (int i = 1; i <= repeats; i++)
    {
        int len = snprintf(line, sizeof(line), "[pid %ld | msg %d] %s\n",
                           (long)getpid(), i, msg);
        if (len > 0)
        {
            if (write(sock_fd, line, (size_t)len) == -1)
            {
                perror("write");
                close(sock_fd);
                return EXIT_FAILURE;
            }
        }
        /* Пауза 0.3 сек между отправками, чтобы параллельные клиенты перемешались */
        usleep(300000);
    }

    close(sock_fd);
    return EXIT_SUCCESS;
}