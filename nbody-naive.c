/*
 * N-Body Gravitational Simulation — Serial
 *
 * Simulates N particles attracting each other under gravity.
 * Uses the brute-force O(N²) all-pairs algorithm: every particle
 * exerts a gravitational force on every other particle each timestep.
 *
 * Physics:
 *   Newton's law of gravitation between two bodies i and j:
 *     F = G * mass_i * mass_j / (distance² + softening²)
 *
 *   The softening factor (epsilon) prevents force from blowing up
 *   when two particles get very close together (avoids division by ~0).
 *
 *   Integration: Leapfrog (Störmer-Verlet)
 *     — More accurate than Euler for orbital mechanics
 *     — Conserves energy much better over long simulations
 *     — velocity updated at half-steps, position at full steps
 *
 * Complexity: O(num_bodies² × num_steps)
 *   With num_bodies=8000 and num_steps=100 → ~6.4B force evaluations
 *   This comfortably exceeds 30 seconds on a typical CPU.
 *
 * Compile:
 *   gcc -O2 -Wall -o nbody nbody.c -lm
 *
 * Usage:
 *   ./nbody                              → defaults
 *   ./nbody <num_bodies> <num_steps>
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

/* ── Simulation parameters ──────────────────────────────────────────────── */
#define DEFAULT_NUM_BODIES  8000
#define DEFAULT_NUM_STEPS   100
#define DEFAULT_THREADS     2

#define GRAVITATIONAL_CONST 6.674e-11   /* m³ kg⁻¹ s⁻²              */
#define SOFTENING_FACTOR    1e-9        /* prevents singularities     */
#define TIMESTEP_SECONDS    0.01        /* seconds per simulation step */
#define MAX_INITIAL_POS     1.0e6       /* metres, initial spread     */
#define MAX_INITIAL_VEL     1.0e2       /* m/s, initial velocity      */
#define MAX_BODY_MASS       1.0e26      /* kg (~10% of Saturn's mass) */

/* ── Body definition ────────────────────────────────────────────────────── */
typedef struct {
    double pos_x, pos_y, pos_z;   /* position     (metres)    */
    double vel_x, vel_y, vel_z;   /* velocity     (m/s)       */
    double force_x, force_y, force_z; /* accumulated force (N) */
    double mass;                   /* mass         (kg)        */
} Body;

/* ── Initialise bodies with random positions, velocities, masses ────────── */
void initialise_bodies(Body *bodies, int num_bodies, unsigned int seed) {
    srand(seed);
    for (int body_idx = 0; body_idx < num_bodies; body_idx++) {
        bodies[body_idx].pos_x = ((double)rand() / RAND_MAX - 0.5) * 2.0 * MAX_INITIAL_POS;
        bodies[body_idx].pos_y = ((double)rand() / RAND_MAX - 0.5) * 2.0 * MAX_INITIAL_POS;
        bodies[body_idx].pos_z = ((double)rand() / RAND_MAX - 0.5) * 2.0 * MAX_INITIAL_POS;

        bodies[body_idx].vel_x = ((double)rand() / RAND_MAX - 0.5) * 2.0 * MAX_INITIAL_VEL;
        bodies[body_idx].vel_y = ((double)rand() / RAND_MAX - 0.5) * 2.0 * MAX_INITIAL_VEL;
        bodies[body_idx].vel_z = ((double)rand() / RAND_MAX - 0.5) * 2.0 * MAX_INITIAL_VEL;

        bodies[body_idx].mass = ((double)rand() / RAND_MAX) * MAX_BODY_MASS + 1.0;

        bodies[body_idx].force_x = 0.0;
        bodies[body_idx].force_y = 0.0;
        bodies[body_idx].force_z = 0.0;
    }
}

/* ── Step 1: Compute gravitational forces between all pairs ─────────────── */
/*
 * This is the O(N²) bottleneck — the inner loop is the parallelisation
 * target for OpenMP. For each body i, we sum the forces from all other
 * bodies j. We use Newton's third law (F_ij = -F_ji) to halve the work:
 * compute the force once per pair and apply it to both bodies.
 *
 * NOTE on parallelising: the Newton's-third-law optimisation introduces
 * a write conflict (both body_i and body_j are updated per pair), which
 * requires careful handling with OpenMP (e.g. thread-local force arrays
 * or atomic operations). The parallel version will address this.
 */
void compute_forces(Body *bodies, int num_bodies, int threads) {
    /* Reset force accumulators */
    for (int body_idx = 0; body_idx < num_bodies; body_idx++) {
        bodies[body_idx].force_x = 0.0;
        bodies[body_idx].force_y = 0.0;
        bodies[body_idx].force_z = 0.0;
    }

    /* All-pairs force calculation */
    #ifdef CHUNK_SIZE
        #pragma omp parallel for schedule(SCHED_TYPE, CHUNK_SIZE) num_threads(threads)
    #else
        #pragma omp parallel for schedule(SCHED_TYPE) num_threads(threads)
    #endif
    for (int body_i = 0; body_i < num_bodies; body_i++) {
        for (int body_j = 0; body_j < num_bodies; body_j++) {
            if (body_j == body_i) continue;
            double delta_x = bodies[body_j].pos_x - bodies[body_i].pos_x;
            double delta_y = bodies[body_j].pos_y - bodies[body_i].pos_y;
            double delta_z = bodies[body_j].pos_z - bodies[body_i].pos_z;

            double distance_squared = delta_x * delta_x
                                    + delta_y * delta_y
                                    + delta_z * delta_z
                                    + SOFTENING_FACTOR * SOFTENING_FACTOR;

            double distance         = sqrt(distance_squared);
            double distance_cubed   = distance_squared * distance;

            double force_magnitude  = GRAVITATIONAL_CONST
                                    * bodies[body_i].mass
                                    * bodies[body_j].mass
                                    / distance_cubed;

            double force_x = force_magnitude * delta_x;
            double force_y = force_magnitude * delta_y;
            double force_z = force_magnitude * delta_z;

            /* Apply equal and opposite forces (Newton's third law) */
            bodies[body_i].force_x += force_x;
            bodies[body_i].force_y += force_y;
            bodies[body_i].force_z += force_z;
        }
    }
}

/* ── Step 2: Update velocities and positions (Leapfrog integration) ─────── */
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

/* ── Compute total kinetic energy (sanity check) ────────────────────────── */
double compute_total_kinetic_energy(const Body *bodies, int num_bodies) {
    double total_kinetic_energy = 0.0;
    for (int body_idx = 0; body_idx < num_bodies; body_idx++) {
        double speed_squared = bodies[body_idx].vel_x * bodies[body_idx].vel_x
                             + bodies[body_idx].vel_y * bodies[body_idx].vel_y
                             + bodies[body_idx].vel_z * bodies[body_idx].vel_z;
        total_kinetic_energy += 0.5 * bodies[body_idx].mass * speed_squared;
    }
    return total_kinetic_energy;
}

/* ── Entry point ────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    int num_bodies = DEFAULT_NUM_BODIES;
    int num_steps  = DEFAULT_NUM_STEPS;
    int threads    = DEFAULT_THREADS;

    if (argc >= 2) num_bodies = atoi(argv[1]);
    if (argc >= 3) num_steps  = atoi(argv[2]);
    if (argc >= 4) threads    = atoi(argv[3]);

    printf("N-Body Simulation (serial)\n");
    printf("  Bodies    : %d\n", num_bodies);
    printf("  Steps     : %d\n", num_steps);
    printf("  Pairs/step: %lld\n", (long long)num_bodies * (num_bodies - 1) / 2);
    printf("  Total ops : %.2e\n\n",
           (double)num_bodies * (num_bodies - 1) / 2.0 * num_steps);

    Body *bodies = malloc(num_bodies * sizeof(Body));
    if (!bodies) { fprintf(stderr, "Out of memory\n"); return 1; }

    initialise_bodies(bodies, num_bodies, 42);

    printf("  Initial KE: %.6e J\n\n", compute_total_kinetic_energy(bodies, num_bodies));

    /* ── Main simulation loop ─────────────────────────────────────────── */
    struct timespec time_start, time_end;
    clock_gettime(CLOCK_MONOTONIC, &time_start);

    for (int step = 0; step < num_steps; step++) {
        compute_forces(bodies, num_bodies, threads);
        update_bodies(bodies, num_bodies);

        if ((step + 1) % 10 == 0) {
            printf("  Step %4d / %d complete\n", step + 1, num_steps);
            fflush(stdout);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &time_end);

    double elapsed_seconds = (time_end.tv_sec  - time_start.tv_sec) +
                             (time_end.tv_nsec - time_start.tv_nsec) / 1e9;

    printf("\n  Final KE  : %.6e J\n", compute_total_kinetic_energy(bodies, num_bodies));
    printf("─────────────────────────────────\n");
    printf("  Time (serial) : %.4f s\n", elapsed_seconds);
    printf("  Avg per step  : %.4f s\n", elapsed_seconds / num_steps);
    printf("─────────────────────────────────\n");

    free(bodies);
    return 0;
}