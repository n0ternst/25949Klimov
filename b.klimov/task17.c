#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>

#define MAX_LINE 40
#define CTRL_D 4
#define CTRL_P 16
#define CTRL_W 23
#define CTRL_G '\a'

static struct termios orig_termios;

/* Восстановление исходных настроек терминала при выходе */
void reset_terminal_mode(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

/* Звуковой сигнал */
void ring_bell(void) {
    char bell = CTRL_G;
    write(STDOUT_FILENO, &bell, 1);
}

/* Удаление n символов с экрана влево от курсора */
void erase_chars_on_screen(int n) {
    for (int i = 0; i < n; i++) {
        write(STDOUT_FILENO, "\b \b", 3);
    }
}

/* Перерисовка остатка строки от позиции cursor_pos до line_len */
void redraw_tail(const char *buf, int cursor_pos, int line_len) {
    int tail_len = line_len - cursor_pos;
    if (tail_len > 0) {
        write(STDOUT_FILENO, &buf[cursor_pos], tail_len);
    }
    /* Затираем возможный оставшийся символ на конце */
    write(STDOUT_FILENO, " ", 1);

    /* Возвращаем курсор обратно на позицию cursor_pos */
    for (int i = 0; i <= tail_len; i++) {
        write(STDOUT_FILENO, "\b", 1);
    }
}

int main(void) {
    struct termios raw;

    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "Ошибка: стандартный ввод не является терминалом.\n");
        return 1;
    }

    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(reset_terminal_mode);

    raw = orig_termios;
    /* Отключаем канонический режим и эхо */
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    cc_t erase_char = orig_termios.c_cc[VERASE];
    cc_t kill_char  = orig_termios.c_cc[VKILL];

    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    char line_buf[MAX_LINE + 1];
    int line_len = 0;
    int cursor_pos = 0;
    int edit_mode = 0; /* 0 — обычный ввод, 1 — режим редактирования (стрелки) */
    char ch;

    while (read(STDIN_FILENO, &ch, 1) == 1) {
        /* Обработка переключения режима по Ctrl+P */
        if (ch == CTRL_P) {
            edit_mode = !edit_mode;
            if (!edit_mode) {
                /* При выходе из режима редактирования возвращаем курсор в конец строки */
                while (cursor_pos < line_len) {
                    write(STDOUT_FILENO, &line_buf[cursor_pos], 1);
                    cursor_pos++;
                }
            }
            continue;
        }

        /* Обработка стрелок клавиатуры (Escape-последовательности: ESC [ C / D) */
        if (ch == '\033') {
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1) {
                if (seq[0] == '[') {
                    if (edit_mode) {
                        if (seq[1] == 'D') { /* Стрелка ВЛЕВО */
                            if (cursor_pos > 0) {
                                cursor_pos--;
                                write(STDOUT_FILENO, "\b", 1);
                            } else {
                                ring_bell();
                            }
                            continue;
                        } else if (seq[1] == 'C') { /* Стрелка ВПРАВО */
                            if (cursor_pos < line_len) {
                                write(STDOUT_FILENO, &line_buf[cursor_pos], 1);
                                cursor_pos++;
                            } else {
                                ring_bell();
                            }
                            continue;
                        }
                    }
                }
            }
            ring_bell();
            continue;
        }

        /* 5. CTRL-D в начале строки — завершение работы */
        if (ch == CTRL_D) {
            if (line_len == 0) {
                break;
            } else {
                ring_bell();
                continue;
            }
        }

        /* Обработка нажатия Enter */
        if (ch == '\n' || ch == '\r') {
            write(STDOUT_FILENO, "\r\n", 2);
            line_len = 0;
            cursor_pos = 0;
            edit_mode = 0;
            continue;
        }

        /* 2. Символ ERASE (обычно Backspace / DEL) */
        if ((unsigned char)ch == (unsigned char)erase_char || ch == '\b' || ch == 127) {
            if (cursor_pos > 0) {
                /* Сдвигаем символы в буфере влево */
                memmove(&line_buf[cursor_pos - 1], &line_buf[cursor_pos], line_len - cursor_pos);
                cursor_pos--;
                line_len--;

                /* Сдвигаем курсор назад и перерисовываем остаток */
                write(STDOUT_FILENO, "\b", 1);
                redraw_tail(line_buf, cursor_pos, line_len);
            } else {
                ring_bell();
            }
            continue;
        }

        /* 3. Символ KILL — удаление всей строки */
        if ((unsigned char)ch == (unsigned char)kill_char) {
            if (line_len > 0) {
                /* Возвращаем курсор в конец для чистого затирания */
                while (cursor_pos < line_len) {
                    write(STDOUT_FILENO, &line_buf[cursor_pos], 1);
                    cursor_pos++;
                }
                erase_chars_on_screen(line_len);
                line_len = 0;
                cursor_pos = 0;
            }
            continue;
        }

        /* 4. Символ CTRL-W — стирание слова перед курсором */
        if (ch == CTRL_W) {
            if (cursor_pos > 0) {
                int to_erase = 0;
                int pos = cursor_pos;

                while (pos > 0 && line_buf[pos - 1] == ' ') {
                    pos--;
                    to_erase++;
                }
                while (pos > 0 && line_buf[pos - 1] != ' ') {
                    pos--;
                    to_erase++;
                }

                memmove(&line_buf[pos], &line_buf[cursor_pos], line_len - cursor_pos);
                line_len -= to_erase;
                cursor_pos = pos;

                for (int i = 0; i < to_erase; i++) {
                    write(STDOUT_FILENO, "\b", 1);
                }
                redraw_tail(line_buf, cursor_pos, line_len);
            } else {
                ring_bell();
            }
            continue;
        }

        /* 6. Непечатаемые символы */
        if (!isprint((unsigned char)ch)) {
            ring_bell();
            continue;
        }

        /* 1 & 7. Вставка печатного символа */
        if (line_len == MAX_LINE) {
            /* Перенос слов на границе 40 символов */
            int word_start = line_len;
            while (word_start > 0 && line_buf[word_start - 1] != ' ') {
                word_start--;
            }

            if (word_start > 0 && word_start < line_len) {
                int word_len = line_len - word_start;
                char wrap_word[MAX_LINE + 1];
                memcpy(wrap_word, &line_buf[word_start], word_len);

                /* Стираем переносимое слово */
                while (cursor_pos < line_len) {
                    write(STDOUT_FILENO, &line_buf[cursor_pos], 1);
                    cursor_pos++;
                }
                erase_chars_on_screen(word_len);

                write(STDOUT_FILENO, "\r\n", 2);
                write(STDOUT_FILENO, wrap_word, word_len);

                memcpy(line_buf, wrap_word, word_len);
                line_len = word_len;
                cursor_pos = word_len;
            } else {
                write(STDOUT_FILENO, "\r\n", 2);
                line_len = 0;
                cursor_pos = 0;
            }
        }

        /* Вставка символа в позицию cursor_pos (в середину или в конец) */
        memmove(&line_buf[cursor_pos + 1], &line_buf[cursor_pos], line_len - cursor_pos);
        line_buf[cursor_pos] = ch;
        line_len++;

        write(STDOUT_FILENO, &ch, 1);
        cursor_pos++;
        redraw_tail(line_buf, cursor_pos, line_len);
    }

    return 0;
}
/*
# Компиляция
gcc -Wall -Wextra task17.c -o task17
./task17
# 1. Обычный ввод символов.
# 2. Backspace (ERASE) и Ctrl+U (KILL).
# 3. Ctrl+W — удаляет последнее слово.
# 4. Ввод строки длиннее 40 символов с пробелами (слово автоматически перенесется).
# 5. Ctrl+D в начале пустой строки — выход из программы.
*/

