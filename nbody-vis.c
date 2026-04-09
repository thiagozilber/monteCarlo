/*
 * N-Body Simulation — Real-time ncurses Visualisation
 *
 * A visual companion to nbody.c. Uses a smaller body count (default 300)
 * so the simulation runs fast enough for smooth real-time rendering.
 * The physics and force calculation are identical to the benchmark version.
 *
 * Compile:
 *   gcc -O2 -Wall -o nbody_vis nbody_vis.c -lm -lncurses
 *
 * Usage:
 *   ./nbody_vis                          → defaults (300 bodies)
 *   ./nbody_vis <num_bodies> <num_steps> → custom (keep bodies ≤ 500 for smooth rendering)
 *
 * Controls:
 *   q  → quit early
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <ncurses.h>

/* ── Simulation parameters ──────────────────────────────────────────────── */
#define DEFAULT_NUM_BODIES    300
#define DEFAULT_NUM_STEPS     2000

#define GRAVITATIONAL_CONST   6.674e-11
#define SOFTENING_FACTOR      1e-9
#define TIMESTEP_SECONDS      0.01
#define MAX_INITIAL_POS       1.0e6
#define MAX_INITIAL_VEL       1.0e2
#define MAX_BODY_MASS         1.0e26

/* How many simulation steps to run between each screen redraw.
 * Increase this if the animation is too slow. */
#define STEPS_PER_FRAME       1

/* ── Body definition ────────────────────────────────────────────────────── */
typedef struct {
    double pos_x, pos_y, pos_z;
    double vel_x, vel_y, vel_z;
    double force_x, force_y, force_z;
    double mass;
} Body;

/* ── Initialise bodies ──────────────────────────────────────────────────── */
void initialise_bodies(Body *bodies, int num_bodies, unsigned int seed) {
    srand(seed);
    for (int body_idx = 0; body_idx < num_bodies; body_idx++) {
        bodies[body_idx].pos_x = ((double)rand() / RAND_MAX - 0.5) * 2.0 * MAX_INITIAL_POS;
        bodies[body_idx].pos_y = ((double)rand() / RAND_MAX - 0.5) * 2.0 * MAX_INITIAL_POS;
        bodies[body_idx].pos_z = ((double)rand() / RAND_MAX - 0.5) * 2.0 * MAX_INITIAL_POS;
        bodies[body_idx].vel_x = ((double)rand() / RAND_MAX - 0.5) * 2.0 * MAX_INITIAL_VEL;
        bodies[body_idx].vel_y = ((double)rand() / RAND_MAX - 0.5) * 2.0 * MAX_INITIAL_VEL;
        bodies[body_idx].vel_z = ((double)rand() / RAND_MAX - 0.5) * 2.0 * MAX_INITIAL_VEL;
        bodies[body_idx].mass  = ((double)rand() / RAND_MAX) * MAX_BODY_MASS + 1.0;
        bodies[body_idx].force_x = bodies[body_idx].force_y = bodies[body_idx].force_z = 0.0;
    }
}

/* ── Compute forces (O(N²)) ─────────────────────────────────────────────── */
void compute_forces(Body *bodies, int num_bodies) {
    for (int body_idx = 0; body_idx < num_bodies; body_idx++) {
        bodies[body_idx].force_x = 0.0;
        bodies[body_idx].force_y = 0.0;
        bodies[body_idx].force_z = 0.0;
    }

    for (int body_i = 0; body_i < num_bodies; body_i++) {
        for (int body_j = body_i + 1; body_j < num_bodies; body_j++) {
            double delta_x = bodies[body_j].pos_x - bodies[body_i].pos_x;
            double delta_y = bodies[body_j].pos_y - bodies[body_i].pos_y;
            double delta_z = bodies[body_j].pos_z - bodies[body_i].pos_z;

            double distance_squared = delta_x * delta_x
                                    + delta_y * delta_y
                                    + delta_z * delta_z
                                    + SOFTENING_FACTOR * SOFTENING_FACTOR;
            double distance       = sqrt(distance_squared);
            double distance_cubed = distance_squared * distance;

            double force_magnitude = GRAVITATIONAL_CONST
                                   * bodies[body_i].mass
                                   * bodies[body_j].mass
                                   / distance_cubed;

            double force_x = force_magnitude * delta_x;
            double force_y = force_magnitude * delta_y;
            double force_z = force_magnitude * delta_z;

            bodies[body_i].force_x += force_x;
            bodies[body_i].force_y += force_y;
            bodies[body_i].force_z += force_z;
            bodies[body_j].force_x -= force_x;
            bodies[body_j].force_y -= force_y;
            bodies[body_j].force_z -= force_z;
        }
    }
}

/* ── Update positions ───────────────────────────────────────────────────── */
void update_bodies(Body *bodies, int num_bodies) {
    for (int body_idx = 0; body_idx < num_bodies; body_idx++) {
        double acceleration_x = bodies[body_idx].force_x / bodies[body_idx].mass;
        double acceleration_y = bodies[body_idx].force_y / bodies[body_idx].mass;
        double acceleration_z = bodies[body_idx].force_z / bodies[body_idx].mass;

        bodies[body_idx].vel_x += acceleration_x * TIMESTEP_SECONDS;
        bodies[body_idx].vel_y += acceleration_y * TIMESTEP_SECONDS;
        bodies[body_idx].vel_z += acceleration_z * TIMESTEP_SECONDS;

        bodies[body_idx].pos_x += bodies[body_idx].vel_x * TIMESTEP_SECONDS;
        bodies[body_idx].pos_y += bodies[body_idx].vel_y * TIMESTEP_SECONDS;
        bodies[body_idx].pos_z += bodies[body_idx].vel_z * TIMESTEP_SECONDS;
    }
}

/* ── Find bounding box of all bodies (for dynamic scaling) ─────────────── */
void compute_bounding_box(const Body *bodies, int num_bodies,
                          double *min_x, double *max_x,
                          double *min_y, double *max_y) {
    *min_x = *max_x = bodies[0].pos_x;
    *min_y = *max_y = bodies[0].pos_y;
    for (int body_idx = 1; body_idx < num_bodies; body_idx++) {
        if (bodies[body_idx].pos_x < *min_x) *min_x = bodies[body_idx].pos_x;
        if (bodies[body_idx].pos_x > *max_x) *max_x = bodies[body_idx].pos_x;
        if (bodies[body_idx].pos_y < *min_y) *min_y = bodies[body_idx].pos_y;
        if (bodies[body_idx].pos_y > *max_y) *max_y = bodies[body_idx].pos_y;
    }
    /* Add 5% padding so particles don't touch the edges */
    double pad_x = (*max_x - *min_x) * 0.05 + 1.0;
    double pad_y = (*max_y - *min_y) * 0.05 + 1.0;
    *min_x -= pad_x; *max_x += pad_x;
    *min_y -= pad_y; *max_y += pad_y;
}

/* ── Choose character based on mass ────────────────────────────────────────
 * Heavier bodies appear as larger/brighter ASCII characters.          */
static char mass_to_char(double mass) {
    double mass_fraction = mass / MAX_BODY_MASS;
    if (mass_fraction > 0.75) return '@';
    if (mass_fraction > 0.50) return 'O';
    if (mass_fraction > 0.25) return 'o';
    return '.';
}

/* ── Render one frame ───────────────────────────────────────────────────── */
void render_frame(const Body *bodies, int num_bodies,
                  int step, int num_steps, double elapsed_seconds) {
    int term_rows, term_cols;
    getmaxyx(stdscr, term_rows, term_cols);

    /* Reserve bottom 3 rows for the status bar */
    int render_rows = term_rows - 3;
    int render_cols = term_cols;

    /* Dynamic bounding box — view follows the particles */
    double min_x, max_x, min_y, max_y;
    compute_bounding_box(bodies, num_bodies, &min_x, &max_x, &min_y, &max_y);

    double range_x = max_x - min_x;
    double range_y = max_y - min_y;

    erase();

    /* Draw border */
    for (int col = 0; col < render_cols; col++) {
        mvaddch(0,            col, '-');
        mvaddch(render_rows,  col, '-');
    }
    for (int row = 1; row < render_rows; row++) {
        mvaddch(row, 0,              '|');
        mvaddch(row, render_cols - 1,'|');
    }

    /* Draw each body */
    for (int body_idx = 0; body_idx < num_bodies; body_idx++) {
        /* Map simulation coordinates → terminal cell */
        int screen_col = (int)((bodies[body_idx].pos_x - min_x) / range_x
                               * (render_cols - 2)) + 1;
        int screen_row = (int)((bodies[body_idx].pos_y - min_y) / range_y
                               * (render_rows - 2)) + 1;

        /* Clamp to render area */
        if (screen_col < 1)              screen_col = 1;
        if (screen_col > render_cols - 2) screen_col = render_cols - 2;
        if (screen_row < 1)              screen_row = 1;
        if (screen_row > render_rows - 2) screen_row = render_rows - 2;

        mvaddch(screen_row, screen_col, mass_to_char(bodies[body_idx].mass));
    }

    /* Status bar */
    double progress = (double)(step + 1) / num_steps * 100.0;
    double step_time = (step > 0) ? elapsed_seconds / step : 0.0;

    mvprintw(render_rows + 1, 1,
             "Step: %5d / %d  |  Progress: %5.1f%%  |  Avg/step: %.4fs  |  Bodies: %d  |  [q] quit",
             step + 1, num_steps, progress, step_time, num_bodies);

    /* Legend */
    mvprintw(render_rows + 2, 1, "Mass scale:  . low    o medium    O high    @ very high");

    refresh();
}

/* ── Entry point ────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    int num_bodies = DEFAULT_NUM_BODIES;
    int num_steps  = DEFAULT_NUM_STEPS;

    if (argc >= 2) num_bodies = atoi(argv[1]);
    if (argc >= 3) num_steps  = atoi(argv[2]);

    Body *bodies = malloc(num_bodies * sizeof(Body));
    if (!bodies) { fprintf(stderr, "Out of memory\n"); return 1; }

    initialise_bodies(bodies, num_bodies, 42);

    /* ── ncurses setup ────────────────────────────────────────────────── */
    initscr();
    cbreak();
    noecho();
    curs_set(0);          /* hide cursor                  */
    nodelay(stdscr, TRUE); /* non-blocking getch()         */
    keypad(stdscr, TRUE);

    /* ── Main loop ────────────────────────────────────────────────────── */
    struct timespec time_start, time_now;
    clock_gettime(CLOCK_MONOTONIC, &time_start);

    for (int step = 0; step < num_steps; step++) {
        /* Check for quit key */
        int key = getch();
        if (key == 'q' || key == 'Q') break;

        compute_forces(bodies, num_bodies);
        update_bodies(bodies, num_bodies);

        if (step % STEPS_PER_FRAME == 0) {
            clock_gettime(CLOCK_MONOTONIC, &time_now);
            double elapsed_seconds = (time_now.tv_sec  - time_start.tv_sec) +
                                     (time_now.tv_nsec - time_start.tv_nsec) / 1e9;
            render_frame(bodies, num_bodies, step, num_steps, elapsed_seconds);
        }
    }

    /* ── Cleanup ──────────────────────────────────────────────────────── */
    endwin();

    free(bodies);

    printf("Simulation complete.\n");
    return 0;
}