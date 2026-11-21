#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>

#include <termios.h>
#include <unistd.h>
#include <stdbool.h>


#define CSI(x, t) "\x1B["#x#t
#define SGR(x) CSI(x, m)

#define MIN(x, y) (x < y ? x : y)
#define MAX(x, y) (x > y ? x : y)

#define MAX_TETROMINO_DATA_LEN 36

typedef struct tetromino_t {
    const uint8_t data[MAX_TETROMINO_DATA_LEN];
    const unsigned int dim, len;
    const int start_i, start_j;

    const unsigned int color_idx;
} tetromino_t;

const tetromino_t I_TETR = {
    .data = {
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF,

        0xFF, 0xFF, 0x00, 0xFF,
        0xFF, 0xFF, 0x00, 0xFF,
        0xFF, 0xFF, 0x00, 0xFF,
        0xFF, 0xFF, 0x00, 0xFF,
    },
    .dim = 4, .len = 2,
    .start_i = -2, .start_j = 3,

    .color_idx = 0,
};

const tetromino_t O_TETR = {
    .data = {
        0x00, 0x00,
        0x00, 0x00,
    },
    .dim = 2, .len = 1,
    .start_i = 0, .start_j = 4,

    .color_idx = 0,
};

const tetromino_t J_TETR = {
    .data = {
        0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00,
        0xFF, 0xFF, 0x00,

        0xFF, 0x00, 0xFF,
        0xFF, 0x00, 0xFF,
        0x00, 0x00, 0xFF,

        0x00, 0xFF, 0xFF,
        0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF,

        0xFF, 0x00, 0x00,
        0xFF, 0x00, 0xFF,
        0xFF, 0x00, 0xFF,
    },
    .dim = 3, .len = 4,
    .start_i = -1, .start_j = 3,

    .color_idx = 1,
};

const tetromino_t L_TETR = {
    .data = {
        0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00,
        0x00, 0xFF, 0xFF,

        0x00, 0x00, 0xFF,
        0xFF, 0x00, 0xFF,
        0xFF, 0x00, 0xFF,

        0xFF, 0xFF, 0x00,
        0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF,

        0xFF, 0x00, 0xFF,
        0xFF, 0x00, 0xFF,
        0xFF, 0x00, 0x00,
    },
    .dim = 3, .len = 4,
    .start_i = -1, .start_j = 3,

    .color_idx = 2,
};

const tetromino_t S_TETR = {
    .data = {
        0xFF, 0xFF, 0xFF,
        0xFF, 0x00, 0x00,
        0x00, 0x00, 0xFF,

        0xFF, 0x00, 0xFF,
        0xFF, 0x00, 0x00,
        0xFF, 0xFF, 0x00,
    },
    .dim = 3, .len = 2,
    .start_i = -1, .start_j = 3,

    .color_idx = 1,
};

const tetromino_t T_TETR = {
    .data = {
        0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00,
        0xFF, 0x00, 0xFF,

        0xFF, 0x00, 0xFF,
        0x00, 0x00, 0xFF,
        0xFF, 0x00, 0xFF,

        0xFF, 0x00, 0xFF,
        0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF,

        0xFF, 0x00, 0xFF,
        0xFF, 0x00, 0x00,
        0xFF, 0x00, 0xFF,
    },
    .dim = 3, .len = 4,
    .start_i = -1, .start_j = 3,

    .color_idx = 0,
};

const tetromino_t Z_TETR = {
    .data = {
        0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0xFF,
        0xFF, 0x00, 0x00,

        0xFF, 0xFF, 0x00,
        0xFF, 0x00, 0x00,
        0xFF, 0x00, 0xFF,
    },
    .dim = 3, .len = 2,
    .start_i = -1, .start_j = 3,

    .color_idx = 2,
};

#define NUM_TETROMINOES 7
#define IS_EMPTY(x) (x == 0xFF)

const tetromino_t * const TETR_LIST[] = {
    &I_TETR,
    &O_TETR,
    &J_TETR,
    &L_TETR,
    &S_TETR,
    &T_TETR,
    &Z_TETR,
};

const unsigned int COLOR_LUT[] = {
    107, 104, 106,
    107, 42, 102,
    107, 45, 105,
    107, 104, 102,
    107, 101, 102,
    107, 104, 102,
    107, 101, 100,
    107, 104, 41,
    107, 104, 101,
    107, 101, 43,
};

enum {
    ACTIVE_STATE,
    LINE_CLEAR_STATE,
    LOSE_STATE,
} g_state;

#define MATRIX_HEIGHT 20
#define MATRIX_WIDTH 10
#define MATRIX_SIZE (MATRIX_HEIGHT * MATRIX_WIDTH)
uint8_t g_matrix[MATRIX_SIZE];
#define G_MATRIX_VAL(i, j) *(g_matrix + (i) * MATRIX_WIDTH + (j))

const tetromino_t *g_tetr, *g_next_tetr;
int g_tetr_i, g_tetr_j, g_tetr_r;
#define TETR_ARR_VAL(t, i, j, r) *(t->data + (r) * t->dim * t->dim + (i) * t->dim + (j))

#define GRAVITY 48
int g_gravity_ctdown;

bool g_clears[MATRIX_HEIGHT];

#define LINE_CLEAR_FRAMES 25
int g_clear_ctdown;

bool collision(int di, int dj, int dr)
{
    int r = (g_tetr_r + dr) % g_tetr->len;

    for (unsigned int ti = 0; ti < g_tetr->dim; ti++) {
        for (unsigned int tj = 0; tj < g_tetr->dim; tj++) {
            if (!IS_EMPTY(TETR_ARR_VAL(g_tetr, ti, tj, r))) {
                int mi = g_tetr_i + ti + di;
                int mj = g_tetr_j + tj + dj;

                if (mi < 0) {
                    continue;
                }
                if (
                    mi >= MATRIX_HEIGHT ||
                    mj < 0 || mj >= MATRIX_WIDTH
                ) {
                    return true;
                }
                
                if (!IS_EMPTY(G_MATRIX_VAL(mi, mj))) {
                    return true;
                }
            }
        }
    }

    return false;
}

void set_active_tetr()
{
    if (g_next_tetr == NULL) {
        g_tetr = TETR_LIST[rand() % NUM_TETROMINOES];
    } else {
        g_tetr = g_next_tetr;
    }
    g_next_tetr = TETR_LIST[rand() % NUM_TETROMINOES];

    g_tetr_i = g_tetr->start_i;
    g_tetr_j = g_tetr->start_j;
    g_tetr_r = 0;

    if (collision(0, 0, 0)) {
        g_state = LOSE_STATE;
    }
}

void try_rotate_cw()
{
    if (!collision(0, 0, 1)) {
        g_tetr_r = (g_tetr_r + 1) % g_tetr->len;
    }
}

void try_move_down()
{
    if (!collision(1, 0, 0)) {
        g_tetr_i++;
    } else {
        /* Lock active tetromino to the matrix. */

        for (unsigned int ti = 0; ti < g_tetr->dim; ti++) {
            for (unsigned int tj = 0; tj < g_tetr->dim; tj++) {
                int mi = g_tetr_i + ti;
                int mj = g_tetr_j + tj;
                if (
                    mi >= 0 && mi < MATRIX_HEIGHT &&
                    mj >= 0 && mj < MATRIX_WIDTH
                ) {
                    if (!IS_EMPTY(TETR_ARR_VAL(g_tetr, ti, tj, g_tetr_r))) {
                        G_MATRIX_VAL(mi, mj) = g_tetr->color_idx;
                    }
                }
            }
        }

        /* Check line clears. */

        bool any_clear = false;

        for (
            unsigned int mi = MAX(g_tetr_i, 0);
            mi < MIN(g_tetr_i + g_tetr->dim, MATRIX_HEIGHT);
            mi++
        ) {
            bool cleared = true;
            for (unsigned int mj = 0; mj < MATRIX_WIDTH; mj++) {
                if (IS_EMPTY(G_MATRIX_VAL(mi, mj))) {
                    cleared = false;
                    break;
                }
            }

            any_clear = any_clear || cleared;

            if (cleared) {
                g_clears[mi] = true;
            }
        }

        if (any_clear) {
            g_state = LINE_CLEAR_STATE;
            g_clear_ctdown = LINE_CLEAR_FRAMES;
        } else {
            set_active_tetr();
        }
    }
}

void try_move_left()
{
    if (!collision(0, -1, 0)) {
        g_tetr_j--;
    }
}

void try_move_right()
{
    if (!collision(0, 1, 0)) {
        g_tetr_j++;
    }
}

void init()
{
    memset(g_matrix, 0xFF, sizeof(g_matrix));

    srand(time(NULL));
    g_next_tetr = NULL;
    set_active_tetr();

    g_gravity_ctdown = GRAVITY;

    memset(g_clears, 0, sizeof(g_clears));

    g_state = ACTIVE_STATE;
}

void update(unsigned char inpt)
{
    if (g_state == ACTIVE_STATE) {
        if (inpt == 'w') {
            try_rotate_cw();
        } else if (inpt == 'a') {
            try_move_left();
        } else if (inpt == 's') {
            try_move_down();
        } else if (inpt == 'd') {
            try_move_right();
        }
        
        if (--g_gravity_ctdown == 0) {
            try_move_down();
            g_gravity_ctdown = GRAVITY;
        }
    } else if (g_state == LINE_CLEAR_STATE) {
        if (--g_clear_ctdown == 0) {
            /* Process line clears. */

            for (unsigned int i = 0; i < MATRIX_HEIGHT; i++) {
                if (g_clears[i]) {
                    memmove(
                        g_matrix + MATRIX_WIDTH,
                        g_matrix,
                        i * MATRIX_WIDTH
                    );
                    memset(g_matrix, 0xFF, MATRIX_WIDTH);
                }
            }

            set_active_tetr();

            g_gravity_ctdown = GRAVITY;

            memset(g_clears, 0, sizeof(g_clears));

            g_state = ACTIVE_STATE;
        }
    }
}

void draw()
{
    uint8_t draw_matrix[MATRIX_SIZE];
    memcpy(&draw_matrix, g_matrix, sizeof(draw_matrix));

    /* Overlay active piece. */

    for (unsigned int ti = 0; ti < g_tetr->dim; ti++) {
        for (unsigned int tj = 0; tj < g_tetr->dim; tj++) {
            int mi = g_tetr_i + ti;
            int mj = g_tetr_j + tj;
            if (
                mi >= 0 && mi < MATRIX_HEIGHT &&
                mj >= 0 && mj < MATRIX_WIDTH
            ) {
                if (!IS_EMPTY(TETR_ARR_VAL(g_tetr, ti, tj, g_tetr_r))) {
                    draw_matrix[mi * MATRIX_WIDTH + mj] = g_tetr->color_idx;
                }
            }
        }
    }

    printf(CSI(,H)CSI(,J));

    /* Draw board. */

    for (unsigned int i = 0; i < MATRIX_HEIGHT; i++) {
        if (g_state == LINE_CLEAR_STATE && g_clears[i]) {
            printf(SGR(7));
        }

        for (unsigned int j = 0; j < MATRIX_WIDTH; j++) {
            uint8_t dm_v = draw_matrix[i * MATRIX_WIDTH + j];
            if (IS_EMPTY(dm_v)) {
                printf(SGR(37;40)"+ ");
            } else {
                printf(SGR(%u;30)"[]", COLOR_LUT[dm_v + 12]);
            }
        }

        printf(SGR(0)CSI(,E));
    }

    /* Draw sidebar. */

    printf(CSI(2;22,H));
    printf(CSI(2;22,H)"LINES"CSI(3;22,H)"000");
    printf(CSI(5;22,H)"SCORE"CSI(6;22,H)"000");

    printf(CSI(8;22,H)"NEXT");
    unsigned int di = abs(g_next_tetr->start_i);
    for (unsigned int i = 0; i < g_next_tetr->dim - di; i++) {
        printf(CSI(%u;22,H), 10 + i);
        for (unsigned int j = 0; j < g_next_tetr->dim; j++) {
            if (IS_EMPTY(TETR_ARR_VAL(g_next_tetr, i + di, j, 0))) {
                printf(SGR(37;40)"  ");
            } else {
                printf(SGR(%u;30)"[]", COLOR_LUT[g_next_tetr->color_idx + 12]);
            }
        }
        printf(SGR(0));
    }

    printf(CSI(13;22,H)"LEVEL"CSI(14;22,H)"000");

    fflush(stdout);
}


int main()
{
    struct termios og_term, raw_term;

    /* Initialize terminal. */

    tcgetattr(STDIN_FILENO, &og_term);

    raw_term = og_term;
    raw_term.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw_term.c_cc[VMIN] = 0;
    raw_term.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_term);

    /* Enable alternative buffer */
    printf(CSI(?1049, h));

    /* Hide terminal cursor. */
    printf(CSI(?25, l));

    /* Game loop. */

    init();

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    while (true) {
        unsigned char inpt = 0;
        if (read(STDIN_FILENO, &inpt, 1) == 1) {
            if (inpt == '\x03') {
                break;
            }
        }

        update(inpt);

        draw();
        
        /* 60 FPS. */
        next.tv_nsec += 16 * 1e6;
        if (next.tv_nsec >= 1e9) {
            next.tv_nsec -= 1e9;
            next.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }
    
    /* Restore terminal cursor. */
    printf(CSI(?25, h));

    /* Disable alternative buffer. */
    printf(CSI(?1049, l));

    /* Restore terminal. */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &og_term);

    return 0;
}