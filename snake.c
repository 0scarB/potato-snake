#ifndef WASM

#define _DEFAULT_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#else

typedef unsigned long size_t;

__attribute__((import_name("rand")))
int rand(void);

__attribute__((import_name("sbrk")))
void* sbrk(size_t size);

__attribute__((import_name("brk")))
int brk(void* ptr);

__attribute__((import_name("wasm_capture_input")))
int wasm_capture_input(void);

__attribute__((import_name("wasm_get_terminal_dims")))
size_t wasm_get_terminal_dims(void);

__attribute__((import_name("wasm_terminal_write")))
void wasm_terminal_write(char* str, int str_len);

__attribute__((import_name("wasm_terminal_write_int")))
void wasm_terminal_write_int(size_t x);

__attribute__((import_name("wasm_terminal_move_cursor")))
void wasm_terminal_move_cursor(size_t x, size_t y);

__attribute__((import_name("wasm_terminal_flush_out")))
void wasm_terminal_flush_out(void);

#endif

typedef struct vec {
    size_t x;
    size_t y;
} Vec;

typedef enum direction {
    UP    = 0,
    RIGHT = 1,
    DOWN  = 2,
    LEFT  = 3,
} Direction;

const int QUIT = 4;

typedef struct state {
    Vec terminal_dims;

    unsigned char* grid;
    Vec            grid_offset;
    Vec            grid_dims;

    Vec snake_head;
    Vec snake_tail;
    Direction snake_head_prev_direction;
    Direction snake_head_direction;
    Direction snake_tail_direction;

    Vec food;

    size_t score;
    Vec    score_pos;

    size_t snake_grow_increment;
    size_t snake_grow_countdown;

    float update_interval;

    size_t do_main_loop_iteration;
    size_t do_teardown;

#ifndef WASM
    char* terminal_out;
    char* terminal_out_write_ptr;

    struct termios orig_terminal_config;
#endif
} State;

int capture_input(void) {
#ifndef WASM
    char input_buf[1024];
    int n = read(1, input_buf, 1024);
    if (n != -1) {
        for (size_t i = 0; i < n; ++i) {
            char c = input_buf[i];
            switch (c) {
                case 'w': return UP;
                case 'a': return LEFT;
                case 's': return DOWN;
                case 'd': return RIGHT;
                case 'q': return QUIT;
            }
        }
    }

    return -1;
#else
    return wasm_capture_input();
#endif
}

// See man ioctl_tty(2)
typedef struct ioctl_terminal_winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
} IOCtlTerminalWinsize;

Vec get_terminal_dims(void) {
#ifndef WASM
    Vec dims;

    IOCtlTerminalWinsize ioctl_winsize;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ioctl_winsize) == -1) {
        // Fallback values
        dims.x = 80;
        dims.y = 24;
    } else {
        // We need reduce the grid width by 1 to prevent the Gnome terminal
        // from scrolling when we write to the bottom right corner of the
        // terminal
        dims.x = ioctl_winsize.ws_col - 1;
        dims.y = ioctl_winsize.ws_row;
    }

    return dims;
#else
    size_t packed_dims = wasm_get_terminal_dims();
    return (Vec) {.x = packed_dims>>16, .y = packed_dims & ((1<<16) - 1)};
#endif
}

void terminal_write(State* state, char* str) {
#ifndef WASM
    while (*str != '\0')
        *state->terminal_out_write_ptr++ = *str++;
#else
    size_t str_len = 0;
    while (str[str_len] != '\0')
        ++str_len;
    wasm_terminal_write(str, str_len);
#endif
}

void terminal_write_int(State* state, size_t x) {
#ifndef WASM
    char* ptr_bottom = state->terminal_out_write_ptr;

    // Push digits on the string in reverse order
    do {
        size_t prev_x = x;
        x /= 10;
        *state->terminal_out_write_ptr++ = (prev_x - x*10) + '0';
    } while (x);

    char* ptr_top = state->terminal_out_write_ptr;

    // Swap the digits on the string
    while (ptr_bottom < ptr_top) {
        --ptr_top;

        char tmp    = *ptr_bottom;
        *ptr_bottom = *ptr_top;
        *ptr_top    = tmp;

        ++ptr_bottom;
    }
#else
    wasm_terminal_write_int(x);
#endif
}

void terminal_move_cursor(State* state, size_t x, size_t y) {
#ifndef WASM
    *state->terminal_out_write_ptr++ = '\033';
    *state->terminal_out_write_ptr++ = '[';
    terminal_write_int(state, y+1);
    *state->terminal_out_write_ptr++ = ';';
    terminal_write_int(state, x+1);
    *state->terminal_out_write_ptr++ = 'H';
#else
    wasm_terminal_move_cursor(x, y);
#endif
}

void terminal_move_cursor_to_grid_pos(State* state, Vec pos) {
    terminal_move_cursor(state, pos.x*2 + state->grid_offset.x,
                                pos.y   + state->grid_offset.y);
}

void terminal_flush_out(State* state) {
#ifndef WASM
    write(STDOUT_FILENO,
          state->terminal_out,
          state->terminal_out_write_ptr - state->terminal_out);
    state->terminal_out_write_ptr = state->terminal_out;
#else
    wasm_terminal_flush_out();
#endif
}

#ifndef WASM
void terminal_clear(State* state) {
    *state->terminal_out_write_ptr++ = '\033';
    *state->terminal_out_write_ptr++ = '[';
    *state->terminal_out_write_ptr++ = '2';
    *state->terminal_out_write_ptr++ = 'J';
}

void terminal_hide_cursor(State* state) {
    *state->terminal_out_write_ptr++ = '\033';
    *state->terminal_out_write_ptr++ = '[';
    *state->terminal_out_write_ptr++ = '?';
    *state->terminal_out_write_ptr++ = '2';
    *state->terminal_out_write_ptr++ = '5';
    *state->terminal_out_write_ptr++ = 'l';
}

void terminal_restore_cursor(State* state) {
    *state->terminal_out_write_ptr++ = '\033';
    *state->terminal_out_write_ptr++ = '[';
    *state->terminal_out_write_ptr++ = '?';
    *state->terminal_out_write_ptr++ = '2';
    *state->terminal_out_write_ptr++ = '5';
    *state->terminal_out_write_ptr++ = 'h';
}
#endif // not WASM

size_t encode_direction_change(Direction d1, Direction d2) {
    return (d2 - d1 + 2) & 3;
}

size_t decode_direction_change(Direction dir, size_t delta) {
    return (dir + delta + 2) & 3;
}

unsigned char snake_at(
    State* state,
    Vec pos
) {
    size_t idx = pos.y*state->grid_dims.x + pos.x;
    return (state->grid[idx>>2] >> ((idx & 3)<<1)) & 3;
}

void snake_start(
    State* state,
    Vec pos
) {
    size_t idx = pos.y*state->grid_dims.x + pos.x;
    state->grid[idx>>2] |= 2<<((idx & 3)<<1);
}

size_t snake_extend_head(State* state) {
    unsigned char* grid = state->grid;
    Vec grid_dims = state->grid_dims;
    Vec* head = &state->snake_head;
    Direction      direction = state->snake_head_direction;
    Direction prev_direction = state->snake_head_prev_direction;

    size_t idx = head->y*grid_dims.x + head->x;
    size_t direction_change_encoding = encode_direction_change(prev_direction, direction);

    grid[idx>>2] = direction_change_encoding<<((idx & 3)<<1)
                       | (grid[idx>>2] & ~(3<<((idx & 3)<<1)));

    if (direction & 2) {
        if (direction & 1) {
            head->x = (head->x - 1 + grid_dims.x) % grid_dims.x;
        } else {
            head->y = (head->y + 1) % grid_dims.y;
        }
    } else {
        if (direction & 1) {
            head->x = (head->x + 1) % grid_dims.x;
        } else {
            head->y = (head->y - 1 + grid_dims.y) % grid_dims.y;
        }
    }

    if (snake_at(state, *head)) {
        return 0;
    }

    idx = head->y*grid_dims.x + head->x;
    grid[idx>>2] |= 2<<((idx & 3)<<1);

    return 1;
}

void snake_retract_tail(State* state) {
    unsigned char* grid = state->grid;
    Vec grid_dims = state->grid_dims;
    Vec* tail = &state->snake_tail;
    Direction prev_direction = state->snake_tail_direction;

    size_t idx = tail->y*grid_dims.x + tail->x;
    size_t direction_change_encoding = grid[idx>>2]>>((idx & 3)<<1) & 3;
    Direction direction = decode_direction_change(prev_direction, direction_change_encoding);

    grid[idx>>2] &= ~(3<<((idx & 3)<<1));

    if (direction & 2) {
        if (direction & 1) {
            tail->x = (tail->x - 1 + grid_dims.x) % grid_dims.x;
        } else {
            tail->y = (tail->y + 1) % grid_dims.y;
        }
    } else {
        if (direction & 1) {
            tail->x = (tail->x + 1) % grid_dims.x;
        } else {
            tail->y = (tail->y - 1 + grid_dims.y) % grid_dims.y;
        }
    }

    state->snake_tail_direction = direction;
}

State state;

#ifdef WASM
__attribute__((export_name("update")))
#endif
float update(void) {
    if (state.do_main_loop_iteration) {
        state.snake_head_prev_direction = state.snake_head_direction;
        int input = capture_input();
        if (input == QUIT) {
            state.do_main_loop_iteration = 0;
            state.do_teardown = 1;
            return state.update_interval;
        } else if (input != -1) {
            state.snake_head_direction = input;
        }

        if (!snake_extend_head(&state)) {
            state.do_main_loop_iteration = 0;
            state.do_teardown = 1;
            return state.update_interval;
        }

        terminal_move_cursor_to_grid_pos(&state, state.snake_head);
        terminal_write(&state, "██");

        if (state.snake_head.x == state.food.x && state.snake_head.y == state.food.y) {
            state.snake_grow_countdown += state.snake_grow_increment;

            ++state.score;

            do {
                state.food.x = rand()%state.grid_dims.x;
                state.food.y = rand()%state.grid_dims.y;
            } while (snake_at(&state, state.food));
            terminal_move_cursor_to_grid_pos(&state, state.food);
            terminal_write(&state, "▓▓");

            terminal_move_cursor(&state, state.score_pos.x, state.score_pos.y);
            terminal_write_int(&state, state.score);
        }

        if (state.snake_grow_countdown == 0) {
            snake_retract_tail(&state);

            terminal_move_cursor_to_grid_pos(&state, state.snake_tail);
            terminal_write(&state, "  ");
        } else {
            --state.snake_grow_countdown;
        }

        terminal_flush_out(&state);
    } else if (state.do_teardown) {
        terminal_flush_out(&state);

#ifndef WASM
        terminal_restore_cursor(&state);
        terminal_flush_out(&state);

        tcsetattr(STDIN_FILENO, TCSANOW, &state.orig_terminal_config);

        brk(state.grid);
#endif
    } else /* do setup */ {
        state.terminal_dims = get_terminal_dims();

        state.grid_offset.x = 0;
        state.grid_offset.y = 1;
        state.grid_dims.x = (state.terminal_dims.x - state.grid_offset.x)>>1;
        state.grid_dims.y = (state.terminal_dims.y - state.grid_offset.y);
        state.grid = sbrk(state.grid_dims.x*state.grid_dims.y<<2);

        state.snake_head.x = state.grid_dims.x>>1;
        state.snake_head.y = state.grid_dims.y>>1;
        state.snake_tail = state.snake_head;
        state.snake_head_prev_direction = RIGHT;
        state.snake_head_direction      = RIGHT;
        state.snake_tail_direction      = RIGHT;

        snake_start(&state, state.snake_head);

        size_t half_circumference = state.terminal_dims.x + state.terminal_dims.y;

        state.snake_grow_increment = half_circumference/30;
        if (state.snake_grow_increment == 0)
            state.snake_grow_increment = 1;
        state.snake_grow_countdown = state.snake_grow_increment;

        state.update_interval = ((float) 10)/((float) half_circumference);

#ifndef WASM
        char  terminal_out[1024];
        state.terminal_out = terminal_out;
        state.terminal_out_write_ptr = state.terminal_out;
        terminal_clear(&state);
        terminal_hide_cursor(&state);
#endif

        do {
            state.food.x = rand()%state.grid_dims.x;
            state.food.y = rand()%state.grid_dims.y;
        } while (snake_at(&state, state.food));
        terminal_move_cursor_to_grid_pos(&state, state.food);
        terminal_write(&state, "▓▓");

        state.score = 0;

        terminal_move_cursor(&state, 0, 0);
        terminal_write(&state, "Score: ");
        terminal_write_int(&state, state.score);

        state.score_pos.x = 7;
        state.score_pos.y = 0;

#ifndef WASM
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

        struct termios new_terminal_config;
        tcgetattr(STDIN_FILENO, &state.orig_terminal_config);
        new_terminal_config = state.orig_terminal_config;
        {
            // Don't ignore carriage return or do mapping betwen carriage return
            // and newline
            new_terminal_config.c_iflag &= ~(IGNCR | ICRNL | INLCR);
            // Don't strip input characters to 7 bits
            new_terminal_config.c_iflag &= ~ISTRIP;
            // Disable parity checking and errors
            new_terminal_config.c_iflag &= ~(INPCK | PARMRK);
            // Don't do output processing
            new_terminal_config.c_oflag &= ~OPOST;
            // Disable echoing of input character back to output
            new_terminal_config.c_lflag &= ~(ECHO | ECHOE | ECHONL);
            // Enable "canonical" mode, disabling input buffering
            new_terminal_config.c_lflag &= ~ICANON;
            // See General Terminal Interface, POSIX, IEEE Std 1003.1-2024,
            // https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/V1_chap11.html
            // for more info.
        }
        tcsetattr(STDIN_FILENO, TCSANOW, &new_terminal_config);
#endif

        terminal_flush_out(&state);

        state.do_main_loop_iteration = 1;
        state.do_teardown = 0;
    }

    return state.update_interval;
}

#ifndef TEST
//#if 0
#ifndef WASM
int main(void) {
    float update_interval = update();

    struct timespec update_deadline;
    clock_gettime(CLOCK_MONOTONIC, &update_deadline);

    while (state.do_main_loop_iteration) {

        update_deadline.tv_nsec += update_interval*1e9;
        while (update_deadline.tv_nsec >= 1e9) {
            update_deadline.tv_sec  += 1;
            update_deadline.tv_nsec -= 1e9;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &update_deadline, NULL);

        update_interval = update();
    }

    update();
    return 0;
}
#endif

#else // if TEST

#include "test_framework.h"

int main(void) {
    test_begin("encode_direction_change"); {
        test_assert(encode_direction_change(UP, UP   ) == 2,
                   "encode_direction_change(UP, UP   ) != 2");
        test_assert(encode_direction_change(UP, RIGHT) == 3,
                   "encode_direction_change(UP, RIGHT) != 3");
        test_assert(encode_direction_change(UP, DOWN ) == 0,
                   "encode_direction_change(UP, DOWN ) != 0");
        test_assert(encode_direction_change(UP, LEFT ) == 1,
                   "encode_direction_change(UP, LEFT ) != 1");

        test_assert(encode_direction_change(RIGHT, UP   ) == 1,
                   "encode_direction_change(RIGHT, UP   ) != 1");
        test_assert(encode_direction_change(RIGHT, RIGHT) == 2,
                   "encode_direction_change(RIGHT, RIGHT) != 2");
        test_assert(encode_direction_change(RIGHT, DOWN ) == 3,
                   "encode_direction_change(RIGHT, DOWN ) != 3");
        test_assert(encode_direction_change(RIGHT, LEFT ) == 0,
                   "encode_direction_change(RIGHT, LEFT ) != 0");

        test_assert(encode_direction_change(DOWN, UP   ) == 0,
                   "encode_direction_change(DOWN, UP   ) != 0");
        test_assert(encode_direction_change(DOWN, RIGHT) == 1,
                   "encode_direction_change(DOWN, RIGHT) != 1");
        test_assert(encode_direction_change(DOWN, DOWN ) == 2,
                   "encode_direction_change(DOWN, DOWN ) != 2");
        test_assert(encode_direction_change(DOWN, LEFT ) == 3,
                   "encode_direction_change(DOWN, LEFT ) != 3");

        test_assert(encode_direction_change(LEFT, UP   ) == 3,
                   "encode_direction_change(LEFT, UP   ) != 3");
        test_assert(encode_direction_change(LEFT, RIGHT) == 0,
                   "encode_direction_change(LEFT, RIGHT) != 0");
        test_assert(encode_direction_change(LEFT, DOWN ) == 1,
                   "encode_direction_change(LEFT, DOWN ) != 1");
        test_assert(encode_direction_change(LEFT, LEFT ) == 2,
                   "encode_direction_change(LEFT, LEFT ) != 2");
    }; test_end();
    test_begin("decode_direction_change"); {
        test_assert(decode_direction_change(UP, UP   ) == 2,
                   "decode_direction_change(UP, UP   ) != 2");
        test_assert(decode_direction_change(UP, RIGHT) == 3,
                   "decode_direction_change(UP, RIGHT) != 3");
        test_assert(decode_direction_change(UP, DOWN ) == 0,
                   "decode_direction_change(UP, DOWN ) != 0");
        test_assert(decode_direction_change(UP, LEFT ) == 1,
                   "decode_direction_change(UP, LEFT ) != 1");

        test_assert(decode_direction_change(RIGHT, UP   ) == 3,
                   "decode_direction_change(RIGHT, UP   ) != 3");
        test_assert(decode_direction_change(RIGHT, RIGHT) == 0,
                   "decode_direction_change(RIGHT, RIGHT) != 0");
        test_assert(decode_direction_change(RIGHT, DOWN ) == 1,
                   "decode_direction_change(RIGHT, DOWN ) != 1");
        test_assert(decode_direction_change(RIGHT, LEFT ) == 2,
                   "decode_direction_change(RIGHT, LEFT ) != 2");

        test_assert(decode_direction_change(DOWN, UP   ) == 0,
                   "decode_direction_change(DOWN, UP   ) != 0");
        test_assert(decode_direction_change(DOWN, RIGHT) == 1,
                   "decode_direction_change(DOWN, RIGHT) != 1");
        test_assert(decode_direction_change(DOWN, DOWN ) == 2,
                   "decode_direction_change(DOWN, DOWN ) != 2");
        test_assert(decode_direction_change(DOWN, LEFT ) == 3,
                   "decode_direction_change(DOWN, LEFT ) != 3");

        test_assert(decode_direction_change(LEFT, UP   ) == 1,
                   "decode_direction_change(LEFT, UP   ) != 1");
        test_assert(decode_direction_change(LEFT, RIGHT) == 2,
                   "decode_direction_change(LEFT, RIGHT) != 2");
        test_assert(decode_direction_change(LEFT, DOWN ) == 3,
                   "decode_direction_change(LEFT, DOWN ) != 3");
        test_assert(decode_direction_change(LEFT, LEFT ) == 0,
                   "decode_direction_change(LEFT, LEFT ) != 0");
    }; test_end();
    test_begin("snake movement"); {
        State state;

        state.grid_dims   = (Vec) {.x = 7, .y = 7};
        state.grid_offset = (Vec) {.x = 3, .y = 4};
        unsigned char grid[1024] = {0};
        state.grid = grid;

        state.snake_head = (Vec) {.x = 3, .y = 3};
        state.snake_tail = state.snake_head;

        test_begin("start"); {
            test_assert(!snake_at(&state, state.snake_head),
                        "snake_at <%ld,%ld> != false", state.snake_head.x, state.snake_head.y);

            snake_start(&state, state.snake_head);

            test_assert(snake_at(&state, state.snake_head),
                       "snake_at <%ld,%ld> != true", state.snake_head.x, state.snake_head.y);
        }; test_end();

        test_begin("extend left"); {
            state.snake_head_direction = LEFT;
            state.snake_head_prev_direction = LEFT;

            Vec expected_pos = state.snake_head;

            // Extend left x 3
            for (size_t i = 0; i < 3; ++i) {
                --expected_pos.x;

                test_assert(!snake_at(&state, expected_pos),
                            "snake_at <%ld,%ld> != false",
                            expected_pos.x, expected_pos.y);

                snake_extend_head(&state);

                test_assert(state.snake_head.x == expected_pos.x,
                           "snake_head.x == %ld, not %ld",
                           state.snake_head.x, expected_pos.x);
                test_assert(state.snake_head.y == expected_pos.y,
                           "snake_head.y == %ld, not %ld",
                           state.snake_head.y, expected_pos.y);

                test_assert(snake_at(&state, state.snake_head),
                           "snake_at <%ld,%ld> != true",
                           state.snake_head.x, state.snake_head.y);

            }

            // Move left again, which should wrap around to right
            expected_pos.x = 6;
            {
                test_assert(!snake_at(&state, expected_pos),
                            "snake_at <%ld,%ld> != false",
                            expected_pos.x, expected_pos.y);

                snake_extend_head(&state);

                test_assert(state.snake_head.x == expected_pos.x,
                           "snake_head.x == %ld, not %ld",
                           state.snake_head.x, expected_pos.x);
                test_assert(state.snake_head.y == expected_pos.y,
                           "snake_head.y == %ld, not %ld",
                           state.snake_head.y, expected_pos.y);

                test_assert(snake_at(&state, state.snake_head),
                           "snake_at <%ld,%ld> != true",
                           state.snake_head.x, state.snake_head.y);
            }
        }; test_end();

        test_begin("retract left"); {
            state.snake_tail_direction = LEFT;

            Vec expected_pos = state.snake_tail;

            // Retract left x 3
            for (size_t i = 0; i < 3; ++i) {
                test_assert(snake_at(&state, expected_pos),
                           "snake_at <%ld,%ld> != true",
                           expected_pos.x, expected_pos.y);

                test_assert(state.snake_tail.x == expected_pos.x,
                           "snake_tail.x == %ld, not %ld",
                           state.snake_tail.x, expected_pos.x);
                test_assert(state.snake_tail.y == expected_pos.y,
                           "snake_tail.y == %ld, not %ld",
                           state.snake_tail.y, expected_pos.y);
                snake_retract_tail(&state);

                test_assert(!snake_at(&state, expected_pos),
                            "snake_at <%ld,%ld> != false", expected_pos.x, expected_pos.y);

                --expected_pos.x;
            }
        }; test_end();
    }; test_end();

    return test_report_returning_exit_status();
}

#endif // TEST
