#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "./unix_sock_30"
#define BUF_SIZE 256

int main(void)
{
    int server_fd, client_fd;
    struct sockaddr_un addr;
    char buf[BUF_SIZE];
    ssize_t bytes_read;

    /* 1. Создаем Unix domain socket (потоковый, как TCP) */
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    /* Удаляем старый файл сокета, если он остался от прошлого запуска */
    unlink(SOCKET_PATH);

    /* 2. Заполняем адресную структуру путем к файлу сокета */
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    /* 3. Привязываем сокет к файлу в файловой системе */
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    /* 4. Переводим сокет в режим прослушивания входящих соединений */
    if (listen(server_fd, 1) == -1)
    {
        perror("listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    printf("Сервер запущен и ждет подключения клиента на сокете '%s'...\n", SOCKET_PATH);
    fflush(stdout);

    /* 5. Принимаем одно подключение клиента (процесс спит, пока клиент не подключится) */
    client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1)
    {
        perror("accept");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    /* 6. Читаем данные от клиента, переводим в верхний регистр и выводим */
    while ((bytes_read = read(client_fd, buf, sizeof(buf))) > 0)
    {
        for (ssize_t i = 0; i < bytes_read; i++)
        {
            buf[i] = (char)toupper((unsigned char)buf[i]);
        }
        if (write(STDOUT_FILENO, buf, (size_t)bytes_read) == -1)
        {
            perror("write stdout");
            break;
        }
    }

    if (bytes_read == -1)
    {
        perror("read");
    }

    /* 7. Клиент разорвал соединение (read вернул 0) — закрываем сокеты и удаляем файл */
    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);

    printf("Клиент отключился. Сервер завершает работу.\n");
    return EXIT_SUCCESS;
}

/*
gcc -std=gnu99 -Wall -Wextra task30_server.c -o server -lsocket -lnsl
gcc -std=gnu99 -Wall -Wextra task30_client.c -o client -lsocket -lnsl

./server &
./client
*/

