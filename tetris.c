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

#define MAX_TETROMINO_ARR_LEN 16
#define NUM_TETROMINOES 7


typedef enum state {
    ACTIVE_STATE,
    LINE_CLEAR_STATE,
    LOSE_STATE,
} state_t;

typedef struct tetromino {
    const int id;

    uint8_t arr[MAX_TETROMINO_ARR_LEN];
    const unsigned int dim;

    int i, j;
} tetromino_t;

const tetromino_t I_TETR = {
    0,
    {
        0, 0, 0, 0,
        1, 1, 1, 1,
        0, 0, 0, 0,
        0, 0, 0, 0,
    }, 4,
    -1, 3,
};

const tetromino_t J_TETR = {
    1,
    {
        1, 0, 0,
        1, 1, 1,
        0, 0, 0,
    }, 3,
    0, 3,
};

const tetromino_t L_TETR = {
    2,
    {
        0, 0, 1,
        1, 1, 1,
        0, 0, 0,
    }, 3,
    0, 3,
};

const tetromino_t O_TETR = {
    3,
    {
        1, 1,
        1, 1,
    }, 2,
    0, 4,
};

const tetromino_t S_TETR = {
    4,
    {
        0, 1, 1,
        1, 1, 0,
        0, 0, 0,
    }, 3,
    0, 3,
};

const tetromino_t T_TETR = {
    5,
    {
        0, 1, 0,
        1, 1, 1,
        0, 0, 0,
    }, 3,
    0, 3,
};

const tetromino_t Z_TETR = {
    6,
    {
        1, 1, 0,
        0, 1, 1,
        0, 0, 0,
    }, 3,
    0, 3,
};

const tetromino_t * const TETR_LIST[] = {
    &I_TETR,
    &J_TETR,
    &L_TETR,
    &O_TETR,
    &S_TETR,
    &T_TETR,
    &Z_TETR,
};

const char * const COLOR_LUT[] = {
    NULL,

    SGR(106),
    SGR(104),
    SGR(43),
    SGR(103),
    SGR(102),
    SGR(105),
    SGR(101),
};


state_t g_state;

#define MATRIX_HEIGHT 20
#define MATRIX_WIDTH 10
#define MATRIX_SIZE (MATRIX_HEIGHT * MATRIX_WIDTH)
uint8_t g_matrix[MATRIX_SIZE];

tetromino_t g_active_tetr;

unsigned int g_bag[NUM_TETROMINOES];
unsigned int g_bag_i;

#define GRAVITY 48
int g_gravity_ctdown;

bool g_clears[MATRIX_HEIGHT];

#define LINE_CLEAR_FRAMES 25
int g_clear_ctdown;


bool collision()
{
    for (unsigned int ti = 0; ti < g_active_tetr.dim; ti++) {
        for (unsigned int tj = 0; tj < g_active_tetr.dim; tj++) {
            uint8_t tv = g_active_tetr.arr[ti * g_active_tetr.dim + tj];

            int mi = g_active_tetr.i + ti;
            int mj = g_active_tetr.j + tj;

            if (tv != 0) {
                if (mi < 0) {
                    continue;
                }
                if (
                    mi >= MATRIX_HEIGHT ||
                    mj < 0 || mj >= MATRIX_WIDTH
                ) {
                    return true;
                }

                uint mv = g_matrix[mi * MATRIX_WIDTH + mj];
                if (mv != 0) {
                    return true;
                }
            }
        }
    }

    return false;
}

void shuffle_bag()
{
    /* Reset bag. */

    for (unsigned int i = 0; i < NUM_TETROMINOES; i++) {
        g_bag[i] = i;
    }

    /* Fisher-Yates shuffle. */

    for (unsigned int i = 0; i < NUM_TETROMINOES; i++) {
        unsigned int j = rand() % (NUM_TETROMINOES - i);

        unsigned int tmp = g_bag[i];
        g_bag[i] = g_bag[i + j];
        g_bag[i + j] = tmp;
    }

    g_bag_i = 0;
}

void set_active_tetr()
{
    unsigned int tl_i = g_bag[g_bag_i++];
    memcpy(&g_active_tetr, TETR_LIST[tl_i], sizeof(tetromino_t));
    if (g_bag_i >= NUM_TETROMINOES) {
        shuffle_bag();
    }

    if (collision()) {
        g_state = LOSE_STATE;
    }
}

void try_rotate_cw()
{
    tetromino_t tmp;
    memcpy(&tmp, &g_active_tetr, sizeof(tmp));

    unsigned int k = 0;
    for (unsigned int j = 0; j < g_active_tetr.dim; j++) {
        for (int i = g_active_tetr.dim - 1; i >= 0; i--) {
            g_active_tetr.arr[k++] = tmp.arr[i * g_active_tetr.dim + j];
        }
    }

    if (collision()) {
        memcpy(&g_active_tetr, &tmp, sizeof(g_active_tetr));
    }
}

void try_move_down()
{
    g_active_tetr.i++;

    if (collision()) {
        g_active_tetr.i--;

        /* Lock active tetromino to the matrix. */

        for (unsigned int ti = 0; ti < g_active_tetr.dim; ti++) {
            for (unsigned int tj = 0; tj < g_active_tetr.dim; tj++) {
                int mi = g_active_tetr.i + ti;
                int mj = g_active_tetr.j + tj;
                if (
                    mi >= 0 && mi < MATRIX_HEIGHT &&
                    mj >= 0 && mj < MATRIX_WIDTH
                ) {
                    uint8_t tv = g_active_tetr.arr[ti * g_active_tetr.dim + tj];
                    if (tv != 0) {
                        g_matrix[mi * MATRIX_WIDTH + mj] = (
                            tv * (g_active_tetr.id + 1)
                        );
                    }
                }
            }
        }

        /* Check line clears. */

        bool any_clear = false;

        for (
            unsigned int mi = (g_active_tetr.i >= 0 ? g_active_tetr.i : 0);
            mi < g_active_tetr.i + g_active_tetr.dim;
            mi++
        ) {
            bool cleared = true;
            for (unsigned int mj = 0; mj < MATRIX_WIDTH; mj++) {
                if (g_matrix[mi * MATRIX_WIDTH + mj] == 0) {
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
    g_active_tetr.j--;

    if (collision()) {
        g_active_tetr.j++;
    }
}

void try_move_right()
{
    g_active_tetr.j++;

    if (collision()) {
        g_active_tetr.j--;
    }
}

void init()
{
    memset(g_matrix, 0, sizeof(g_matrix));

    srand(time(NULL));
    shuffle_bag();
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
                    memset(g_matrix, 0, MATRIX_WIDTH);
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

    for (unsigned int ti = 0; ti < g_active_tetr.dim; ti++) {
        for (unsigned int tj = 0; tj < g_active_tetr.dim; tj++) {
            int mi = g_active_tetr.i + ti;
            int mj = g_active_tetr.j + tj;
            if (
                mi >= 0 && mi < MATRIX_HEIGHT &&
                mj >= 0 && mj < MATRIX_WIDTH
            ) {
                uint8_t tv = g_active_tetr.arr[ti * g_active_tetr.dim + tj];
                if (tv != 0) {
                    draw_matrix[mi * MATRIX_WIDTH + mj] = (
                        tv * (g_active_tetr.id + 1)
                    );
                }
            }
        }
    }

    /* Reset cursor. */
    printf(CSI(,H));

    for (unsigned int i = 0; i < MATRIX_HEIGHT; i++) {
        if (g_state == LINE_CLEAR_STATE) {
            if (g_clears[i]) {
                printf(SGR(7));
            }
        }

        for (unsigned int j = 0; j < MATRIX_WIDTH; j++) {
            printf(SGR(40));

            uint8_t dm_v = draw_matrix[i * MATRIX_WIDTH + j];
            if (dm_v == 0) {
                printf(SGR(90)"+-");
            } else {
                printf(SGR(30)"%s[]", COLOR_LUT[dm_v]);
            }
        }

        printf(SGR(0)"\n");
    }
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

    fflush(stdout);

    /* Disable alternative buffer. */
    printf(CSI(?1049, l));

    /* Restore terminal. */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &og_term);

    return 0;
}