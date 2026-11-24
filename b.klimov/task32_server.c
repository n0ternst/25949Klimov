#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "./unix_sock_32"
#define MAX_CLIENTS 64
#define BUF_SIZE 256

static volatile sig_atomic_t io_ready = 0;
static volatile sig_atomic_t keep_running = 1;

/* Обработчик сигнала ввода-вывода (SIGIO / SIGPOLL) */
void handle_sigio(int sig)
{
    (void)sig;
    io_ready = 1;
}

/* Обработчик Ctrl+C для корректного удаления файла сокета */
void handle_sigint(int sig)
{
    (void)sig;
    keep_running = 0;
}

/* Перевод сокета в неблокирующий асинхронный режим (генерация SIGIO при событиях) */
int enable_async_io(int fd)
{
    /* Указываем ядру, какому процессу (PID) слать сигнал SIGIO */
    if (fcntl(fd, F_SETOWN, getpid()) == -1)
    {
        perror("fcntl(F_SETOWN)");
        return -1;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl(F_GETFL)");
        return -1;
    }

    /* O_NONBLOCK — чтобы read/accept не зависали, когда данные кончились;
     * O_ASYNC    — чтобы ядро присылало SIGIO при появлении новых данных/подключений */
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK | O_ASYNC) == -1)
    {
        perror("fcntl(F_SETFL)");
        return -1;
    }

    return 0;
}

int main(void)
{
    int server_fd;
    int client_fds[MAX_CLIENTS];
    struct sockaddr_un addr;
    struct sigaction sa_io, sa_int;
    sigset_t block_mask, old_mask;

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_fds[i] = -1;
    }

    /* 1. Настраиваем обработчики SIGIO и SIGINT */
    memset(&sa_io, 0, sizeof(sa_io));
    sa_io.sa_handler = handle_sigio;
    sigemptyset(&sa_io.sa_mask);
    if (sigaction(SIGIO, &sa_io, NULL) == -1)
    {
        perror("sigaction(SIGIO)");
        return EXIT_FAILURE;
    }

    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    if (sigaction(SIGINT, &sa_int, NULL) == -1)
    {
        perror("sigaction(SIGINT)");
        return EXIT_FAILURE;
    }

    /* 2. Блокируем SIGIO на время проверки флагов, чтобы не потерять сигнал до sigsuspend() */
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGIO);
    if (sigprocmask(SIG_BLOCK, &block_mask, &old_mask) == -1)
    {
        perror("sigprocmask");
        return EXIT_FAILURE;
    }

    /* 3. Создаем, привязываем и начинаем слушать Unix domain socket */
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    unlink(SOCKET_PATH);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 16) == -1)
    {
        perror("listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    /* Включаем генерацию SIGIO для слушающего сокета */
    if (enable_async_io(server_fd) == -1)
    {
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    printf("Сервер (async I/O) запущен на '%s'. Ожидание клиентов (Ctrl+C для выхода)...\n", SOCKET_PATH);
    fflush(stdout);

    while (keep_running)
    {
        /* Если событий пока нет — атомарно разблокируем SIGIO и засыпаем до прихода сигнала */
        while (!io_ready && keep_running)
        {
            sigsuspend(&old_mask);
        }

        if (!keep_running)
        {
            break;
        }

        /* Сбрасываем флаг перед вычитыванием всех накопившихся событий */
        io_ready = 0;

        /* 4. Принимаем ВСЕ накопившиеся входящие подключения */
        while (1)
        {
            int cfd = accept(server_fd, NULL, NULL);
            if (cfd == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break; /* Очередь новых подключений пуста */
                }
                perror("accept");
                break;
            }

            if (enable_async_io(cfd) == -1)
            {
                close(cfd);
                continue;
            }

            int added = 0;
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (client_fds[i] == -1)
                {
                    client_fds[i] = cfd;
                    added = 1;
                    break;
                }
            }

            if (!added)
            {
                fprintf(stderr, "Достигнут лимит клиентов.\n");
                close(cfd);
            }
        }

        /* 5. Вычитываем доступные данные от ВСЕХ активных клиентов */
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (client_fds[i] == -1)
            {
                continue;
            }

            char buf[BUF_SIZE];
            while (1)
            {
                ssize_t bytes_read = read(client_fds[i], buf, sizeof(buf));
                if (bytes_read > 0)
                {
                    for (ssize_t j = 0; j < bytes_read; j++)
                    {
                        buf[j] = (char)toupper((unsigned char)buf[j]);
                    }
                    if (write(STDOUT_FILENO, buf, (size_t)bytes_read) == -1)
                    {
                        perror("write stdout");
                    }
                }
                else if (bytes_read == 0)
                {
                    /* Клиент закрыл соединение */
                    close(client_fds[i]);
                    client_fds[i] = -1;
                    break;
                }
                else
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        /* Данных от этого клиента больше нет — переходим к следующему */
                        break;
                    }
                    perror("read");
                    close(client_fds[i]);
                    client_fds[i] = -1;
                    break;
                }
            }
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_fds[i] != -1)
        {
            close(client_fds[i]);
        }
    }
    close(server_fd);
    unlink(SOCKET_PATH);
    printf("\nСервер остановлен.\n");

    return EXIT_SUCCESS;
}

/*
gcc -std=gnu99 -Wall -Wextra task32_server.c -o server32 -lsocket -lnsl
gcc -std=gnu99 -Wall -Wextra task32_client.c -o client32 -lsocket -lnsl

./server32 &
./client32 "first async client" 5 & ./client32 "second async client" 5 &
*/