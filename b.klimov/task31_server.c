#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "./unix_sock_31"
#define MAX_CLIENTS 64
#define BUF_SIZE 256

static volatile sig_atomic_t keep_running = 1;

void handle_sigint(int sig)
{
    (void)sig;
    keep_running = 0;
}

int main(void)
{
    int server_fd;
    struct sockaddr_un addr;
    /* Массив структур для poll: индекс 0 — слушающий сокет, 1..MAX_CLIENTS — клиенты */
    struct pollfd fds[MAX_CLIENTS + 1];
    struct sigaction sa;

    /* Настраиваем корректный выход по Ctrl+C, чтобы удалить файл сокета */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

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

    /* Инициализируем таблицу дескрипторов для poll() */
    fds[0].fd = server_fd;
    fds[0].events = POLLIN; /* Ждем входящих подключений */

    for (int i = 1; i <= MAX_CLIENTS; i++)
    {
        fds[i].fd = -1; /* Ядро игнорирует элементы с отрицательным fd */
        fds[i].events = POLLIN;
    }

    printf("Сервер запущен (сокет '%s'). Ожидание клиентов (Ctrl+C для выхода)...\n", SOCKET_PATH);
    fflush(stdout);

    while (keep_running)
    {
        /* Ждем событий на любом из сокетов (-1 = ждать бесконечно) */
        int ready = poll(fds, MAX_CLIENTS + 1, -1);
        if (ready == -1)
        {
            if (errno == EINTR)
            {
                continue; /* Прервано сигналом (например, Ctrl+C) */
            }
            perror("poll");
            break;
        }

        /* 1. Проверяем слушающий сокет: не пришел ли новый клиент? */
        if (fds[0].revents & POLLIN)
        {
            int client_fd = accept(server_fd, NULL, NULL);
            if (client_fd == -1)
            {
                perror("accept");
            }
            else
            {
                int added = 0;
                for (int i = 1; i <= MAX_CLIENTS; i++)
                {
                    if (fds[i].fd == -1)
                    {
                        fds[i].fd = client_fd;
                        fds[i].events = POLLIN;
                        added = 1;
                        break;
                    }
                }
                if (!added)
                {
                    fprintf(stderr, "Достигнут лимит клиентов, соединение отклонено.\n");
                    close(client_fd);
                }
            }
        }

        /* 2. Проверяем всех активных клиентов на наличие данных или разрыв связи */
        for (int i = 1; i <= MAX_CLIENTS; i++)
        {
            if (fds[i].fd == -1)
            {
                continue;
            }

            if (fds[i].revents & (POLLIN | POLLHUP | POLLERR))
            {
                char buf[BUF_SIZE];
                ssize_t bytes_read = read(fds[i].fd, buf, sizeof(buf));

                if (bytes_read <= 0)
                {
                    if (bytes_read == -1)
                    {
                        perror("read");
                    }
                    /* Клиент закрыл соединение (0) или ошибка — освобождаем слот */
                    close(fds[i].fd);
                    fds[i].fd = -1;
                }
                else
                {
                    /* Переводим полученную порцию в верхний регистр и сразу выводим */
                    for (ssize_t j = 0; j < bytes_read; j++)
                    {
                        buf[j] = (char)toupper((unsigned char)buf[j]);
                    }
                    if (write(STDOUT_FILENO, buf, (size_t)bytes_read) == -1)
                    {
                        perror("write stdout");
                    }
                }
            }
        }
    }

    /* Закрываем все оставшиеся сокеты и удаляем файл сокета при завершении */
    for (int i = 0; i <= MAX_CLIENTS; i++)
    {
        if (fds[i].fd != -1)
        {
            close(fds[i].fd);
        }
    }
    unlink(SOCKET_PATH);
    printf("\nСервер остановлен.\n");

    return EXIT_SUCCESS;
}

/*
gcc -std=gnu99 -Wall -Wextra task31_server.c -o server31 -lsocket -lnsl
gcc -std=gnu99 -Wall -Wextra task31_client.c -o client31 -lsocket -lnsl

./server31 &
./client31 "first client text" 5 & ./client31 "second client text" 5 &
*/