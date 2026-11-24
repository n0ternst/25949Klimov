#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

/* Счетчик звуковых сигналов, флаг паузы и флаг завершения работы.
 * Переменные, изменяемые в обработчиках сигналов, обязаны быть
 * volatile sig_atomic_t для предотвращения гонок и оптимизаций компилятора. */
static volatile sig_atomic_t beep_count = 0;
static volatile sig_atomic_t is_paused = 0;
static volatile sig_atomic_t should_exit = 0;

/* Обработчик SIGINT (Ctrl-C) */
void sigint_handler(int sig)
{
    (void)sig;

    if (!is_paused)
    {
        /* В обработчиках сигналов безопасно использовать только async-signal-safe функции*/
        const char bell = '\a';
        write(STDOUT_FILENO, &bell, 1);

        beep_count++;
    }
}

/*обработчик SIGTSTP (Ctrl-Z) */
void sigtstp_handler(int sig)
{
    (void)sig;

    is_paused = !is_paused;

    if (is_paused)
    {
        const char msg[] = "\n[Пауза: звуковой сигнал и счетчик отключены]\n";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    }
    else
    {
        const char msg[] = "\n[Штатный режим: звуковой сигнал и счетчик включены]\n";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    }
}

/* обработчик SIGQUIT (Ctrl-\) */
void sigquit_handler(int sig)
{
    (void)sig;
    should_exit = 1;
}

int main(void)
{
    struct sigaction sa_int;
    struct sigaction sa_tstp;
    struct sigaction sa_quit;

    /* настройка обработчика для SIGINT */
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;

    if (sigaction(SIGINT, &sa_int, NULL) == -1)
    {
        perror("sigaction(SIGINT)");
        return 1;
    }

    /* настройка обработчика для SIGTSTP */
    sa_tstp.sa_handler = sigtstp_handler;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = 0;

    if (sigaction(SIGTSTP, &sa_tstp, NULL) == -1)
    {
        perror("sigaction(SIGTSTP)");
        return 1;
    }

    /* настройка обработчика для SIGQUIT */
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
    printf("Нажимайте Ctrl-Z (SIGTSTP) для включения/выключения паузы.\n");
    printf("Нажмите Ctrl-\\ (SIGQUIT) для вывода статистики и выхода.\n");
    fflush(stdout);

    /* бесконечный цикл ожидания сигналов */
    while (!should_exit)
    {
        /* pause() переводит процесс в сон до прихода любого сигнала*/
        pause();
    }

    /* вывод при получении SIGQUIT */
    printf("\nПолучен сигнал SIGQUIT.\n");
    printf("Звуковой сигнал прозвучал раз: %d\n", (int)beep_count);

    return 0;
}

/*
gcc -std=gnu99 -Wall -Wextra task21.c -o task21
./task21
*/