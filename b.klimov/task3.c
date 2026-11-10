#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

void print_uids(const char *stage_name)
{
    printf("\n=== %s ===\n", stage_name);
    printf("Реальный UID (ruid):      %ld\n", (long)getuid());
    printf("Эффективный UID (euid):  %ld\n", (long)geteuid());
}

void try_open_file(const char *filename)
{
    printf("Пытаюсь открыть файл '%s'...\n", filename);
    FILE *f = fopen(filename, "r");
    if (f == NULL)
    {
        perror("Ошибка при открытии файла (fopen)");
    }
    else
    {
        printf("Файл успешно открыт!\n");
        if (fclose(f) != 0)
        {
            perror("Ошибка при закрытии файла (fclose)");
        }
    }
}

int main(int argc, char *argv[])
{
    const char *filename = (argc > 1) ? argv[1] : "secret.txt";

    /* Шаг 1 и 2: Печатаем UID и пытаемся открыть файл */
    print_uids("Этап 1: До сброса привилегий");
    try_open_file(filename);

    /* Шаг 3: Делаем, чтобы реальный и эффективный UID совпадали */
    /* Вызов setuid(getuid()) сбрасывает эффективный UID к реальному UID того, кто запустил процесс */
    if (setuid(getuid()) == -1)
    {
        perror("Ошибка вызова setuid");
    }

    /* Шаг 4: Повторяем первые два шага */
    print_uids("Этап 2: После setuid(getuid())");
    try_open_file(filename);

    return 0;
}