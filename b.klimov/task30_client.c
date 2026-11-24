#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "./unix_sock_30"

int main(int argc, char *argv[])
{
    int sock_fd;
    struct sockaddr_un addr;

    /* Можно передать свой текст аргументом, иначе отправится строка по умолчанию */
    const char *text = (argc > 1)
                           ? argv[1]
                           : "Hello, Solaris! Unix Domain Socket test.\n"
                             "Mixed Case Text: aBcDeFgHiJkLmNoPqRsTuVwXyZ 123!\n";

    /* 1. Создаем клиентский Unix domain socket */
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    /* 2. Указываем тот же путь к файлу сокета, что и у сервера */
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    /* 3. Устанавливаем соединение с сервером */
    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("connect");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    /* 4. Отправляем текст серверу */
    size_t len = strlen(text);
    size_t total_written = 0;

    while (total_written < len)
    {
        ssize_t written = write(sock_fd, text + total_written, len - total_written);
        if (written == -1)
        {
            perror("write");
            close(sock_fd);
            return EXIT_FAILURE;
        }
        total_written += (size_t)written;
    }

    /* Если текст передан через argv[1] без переноса строки, добавим \n для красоты */
    if (argc > 1)
    {
        write(sock_fd, "\n", 1);
    }

    /* 5. Разрываем соединение и завершаемся */
    close(sock_fd);
    printf("Клиент передал данные и разорвал соединение.\n");

    return EXIT_SUCCESS;
}