#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

/* Счетчик звуковых сигналов и флаг завершения работы.
 * Переменные, изменяемые в обработчиках сигналов, обязаны быть
 * volatile sig_atomic_t для предотвращения гонок и оптимизаций компилятора. */
static volatile sig_atomic_t beep_count = 0;
static volatile sig_atomic_t should_exit = 0;

/* Обработчик SIGINT (Ctrl-C) */
void sigint_handler(int sig)
{
    (void)sig;

    /* В обработчиках сигналов безопасно использовать только async-signal-safe функции.
     * printf() использовать нельзя, поэтому пищим через прямой системный вызов write(). */
    const char bell = '\a';
    write(STDOUT_FILENO, &bell, 1);

    beep_count++;
}

/* Обработчик SIGQUIT (Ctrl-\) */
void sigquit_handler(int sig)
{
    (void)sig;
    should_exit = 1;
}

int main(void)
{
    struct sigaction sa_int;
    struct sigaction sa_quit;

    /* Настройка обработчика для SIGINT */
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;

    if (sigaction(SIGINT, &sa_int, NULL) == -1)
    {
        perror("sigaction(SIGINT)");
        return 1;
    }

    /* Настройка обработчика для SIGQUIT */
    sa_quit.sa_handler = sigquit_handler;
    sigemptyset(&sa_quit.sa_mask);
    sa_quit.sa_flags = 0;

    if (sigaction(SIGQUIT, &sa_quit, NULL) == -1)
    {
        perror("sigaction(SIGQUIT)");
        return 1;
    }

    printf("Программа запущена (PID: %ld).\n", (long)getpid());
    printf("Нажимайте Ctrl-C (SIGINT) для звукового сигнала.\n");
    printf("Нажмите Ctrl-\\ (SIGQUIT) для вывода статистики и выхода.\n");
    fflush(stdout);

    /* Бесконечный цикл ожидания сигналов */
    while (!should_exit)
    {
        /* pause() переводит процесс в сон до прихода любого сигнала,
         * не нагружая процессор пустыми итерациями. */
        pause();
    }

    /* Вывод итоговой статистики при получении SIGQUIT */
    printf("\nПолучен сигнал SIGQUIT.\n");
    printf("Звуковой сигнал прозвучал раз: %d\n", (int)beep_count);

    return 0;
}