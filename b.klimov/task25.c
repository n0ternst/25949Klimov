#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUF_SIZE 256

int main(void) {
    int pipefd[2];
    pid_t pid;

    if (pipe(pipefd) == -1) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    pid = fork();
    if (pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        /*дочерний процесс: читает из канала переводит в верхний регистр и выводит */
        char buf[BUF_SIZE];
        ssize_t bytes_read;
        ssize_t i;

        /* закрываем неиспользуемый конец канала для записи */
        close(pipefd[1]);

        while ((bytes_read = read(pipefd[0], buf, sizeof(buf))) > 0) {
            for (i = 0; i < bytes_read; i++) {
                buf[i] = (char)toupper((unsigned char)buf[i]);
            }
            if (write(STDOUT_FILENO, buf, (size_t)bytes_read) == -1) {
                perror("write stdout");
                close(pipefd[0]);
                return EXIT_FAILURE;
            }
        }

        if (bytes_read == -1) {
            perror("read pipe");
            close(pipefd[0]);
            return EXIT_FAILURE;
        }

        close(pipefd[0]);
        return EXIT_SUCCESS;
    } else {
        /* родительский процесс отправляет текст смешанного регистра в канал */
        const char *text = "Hello, Solaris! Pipe IPC test.\n"
                           "LowerCase and UPPERCASE letters: aBcDeFgHiJkLmNoPqRsTuVwXyZ 123!\n";
        size_t len = strlen(text);
        size_t total_written = 0;

        /* закрываем неиспользуемый конец канала для чтения */
        close(pipefd[0]);

        while (total_written < len) {
            ssize_t written = write(pipefd[1], text + total_written, len - total_written);
            if (written == -1) {
                perror("write pipe");
                close(pipefd[1]);
                wait(NULL);
                return EXIT_FAILURE;
            }
            total_written += (size_t)written;
        }

        /* закрываем конец записи чтобы дочерний процесс получил EOF и вышел из цикла read() */
        close(pipefd[1]);

        /* ожидаем завершения дочернего процесса */
        if (wait(NULL) == -1) {
            perror("wait");
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }
}

/*
gcc -std=gnu99 -Wall -Wextra task25.c -o task25
./task25
*/