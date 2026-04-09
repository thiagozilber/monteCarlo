/*
 * N-Body Simulation - SDL2 Real-time Visualisation
 *
 * Uses dimensionless simulation units (G=1) to avoid the numerical
 * instability that arises from mixing SI constants with arbitrary scales.
 * All masses, distances, and times are in simulation units.
 *
 * Compile:
 *   gcc -O2 -Wall -o nbody_sdl nbody_sdl.c -lm -lSDL2
 *
 * Usage:
 *   ./nbody_sdl <num_bodies> <num_steps> [config]
 *
 * Configs:
 *   0 - Random        Chaotic, unbound - shows why initial conditions matter
 *   1 - Galaxy Disk   Heavy center + particles in stable circular orbits
 *   2 - Cold Collapse Uniform sphere, zero velocity - dramatic infall and bounce
 *   3 - Solar System  Dominant central star, others in spaced circular orbits
 *   4 - Two Galaxies  Two rotating disks on a collision course
 *   5 - Binary Stars  Two equal stars in mutual orbit, disk orbiting the pair
 *
 * Controls:
 *   Escape or close window -> quit
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <SDL2/SDL.h>

/* -- Window ----------------------------------------------------------------- */
#define WINDOW_WIDTH    900
#define WINDOW_HEIGHT   900
#define PARTICLE_RADIUS 3

/* -- Simulation units (G=1) ------------------------------------------------- */
/*
 * Everything is dimensionless. Choosing G=1 means we can freely set
 * mass and distance scales without worrying about unit conversions.
 * The timestep DT=0.001 gives ~1000 steps per orbital period for a
 * body at r=0.5 around a mass of 100 -- comfortable stability margin.
 */
#define G           1.0
#define DT          0.001
#define SOFTENING   0.02    /* prevents singularities; ~2% of disk radius */
#define DISK_RADIUS 1.0

/* Masses */
#define CENTRAL_MASS  100.0  /* heavy body anchoring orbital configs       */
#define STAR_MASS      50.0  /* each star in the binary config             */
#define BODY_MASS       0.1  /* typical particle                           */

/* Default run */
#define DEFAULT_NUM_BODIES  300
#define DEFAULT_NUM_STEPS   5000

/* -- Body ------------------------------------------------------------------- */
typedef struct {
    double pos_x, pos_y, pos_z;
    double vel_x, vel_y, vel_z;
    double force_x, force_y, force_z;
    double mass;
} Body;

/* -- Helpers ---------------------------------------------------------------- */
static void zero_forces(Body *bodies, int n) {
    for (int i = 0; i < n; i++)
        bodies[i].force_x = bodies[i].force_y = bodies[i].force_z = 0.0;
}

/* Circular orbital speed around a point mass M at distance r */
static double orbital_speed(double M, double r) {
    return sqrt(G * M / (r + SOFTENING));
}

/* ============================================================
 * CONFIGURATIONS
 * ============================================================ */

/* Config 0 - Random -------------------------------------------------------- */
void config_random(Body *bodies, int n) {
    srand(42);
    for (int i = 0; i < n; i++) {
        bodies[i].pos_x = ((double)rand()/RAND_MAX - 0.5) * 2.0 * DISK_RADIUS;
        bodies[i].pos_y = ((double)rand()/RAND_MAX - 0.5) * 2.0 * DISK_RADIUS;
        bodies[i].pos_z = 0.0;
        bodies[i].vel_x = ((double)rand()/RAND_MAX - 0.5) * 0.5;
        bodies[i].vel_y = ((double)rand()/RAND_MAX - 0.5) * 0.5;
        bodies[i].vel_z = 0.0;
        bodies[i].mass  = BODY_MASS * (0.5 + (double)rand()/RAND_MAX);
    }
    zero_forces(bodies, n);
}

/* Config 1 - Galaxy Disk --------------------------------------------------- */
/* Heavy central body; lighter particles assigned exact circular orbit speeds.
 * The simulation stays bound and produces beautiful rosette/spiral motion. */
void config_galaxy_disk(Body *bodies, int n) {
    srand(42);

    /* Central body */
    bodies[0].pos_x = bodies[0].pos_y = bodies[0].pos_z = 0.0;
    bodies[0].vel_x = bodies[0].vel_y = bodies[0].vel_z = 0.0;
    bodies[0].mass  = CENTRAL_MASS;
    bodies[0].force_x = bodies[0].force_y = bodies[0].force_z = 0.0;

    for (int i = 1; i < n; i++) {
        double r     = 0.05 + ((double)rand()/RAND_MAX) * (DISK_RADIUS - 0.05);
        double angle = ((double)rand()/RAND_MAX) * 2.0 * M_PI;

        bodies[i].pos_x = r * cos(angle);
        bodies[i].pos_y = r * sin(angle);
        bodies[i].pos_z = ((double)rand()/RAND_MAX - 0.5) * 0.02;

        double speed    = orbital_speed(CENTRAL_MASS, r);
        bodies[i].vel_x = -speed * sin(angle);
        bodies[i].vel_y =  speed * cos(angle);
        bodies[i].vel_z = 0.0;

        bodies[i].mass = BODY_MASS * (0.5 + (double)rand()/RAND_MAX);
        bodies[i].force_x = bodies[i].force_y = bodies[i].force_z = 0.0;
    }
}

/* Config 2 - Cold Collapse ------------------------------------------------- */
/* Bodies uniformly distributed in a sphere with zero velocity.
 * They free-fall inward, crunch at the center, then bounce outward.
 * The collapse time is ~sqrt(R^3 / (G*M_total)). */
void config_cold_collapse(Body *bodies, int n) {
    srand(42);
    for (int i = 0; i < n; i++) {
        double x, y, z;
        do {
            x = ((double)rand()/RAND_MAX - 0.5) * 2.0;
            y = ((double)rand()/RAND_MAX - 0.5) * 2.0;
            z = ((double)rand()/RAND_MAX - 0.5) * 2.0;
        } while (x*x + y*y + z*z > 1.0);

        bodies[i].pos_x = x * DISK_RADIUS;
        bodies[i].pos_y = y * DISK_RADIUS;
        bodies[i].pos_z = z * DISK_RADIUS;
        bodies[i].vel_x = bodies[i].vel_y = bodies[i].vel_z = 0.0;
        bodies[i].mass  = BODY_MASS * (0.5 + (double)rand()/RAND_MAX);
        bodies[i].force_x = bodies[i].force_y = bodies[i].force_z = 0.0;
    }
}

/* Config 3 - Solar System -------------------------------------------------- */
/* One dominant star, planets distributed logarithmically in radius so both
 * tight inner and wide outer orbits are visible at the same time. */
void config_solar_system(Body *bodies, int n) {
    srand(42);

    bodies[0].pos_x = bodies[0].pos_y = bodies[0].pos_z = 0.0;
    bodies[0].vel_x = bodies[0].vel_y = bodies[0].vel_z = 0.0;
    bodies[0].mass  = CENTRAL_MASS * 10.0;
    bodies[0].force_x = bodies[0].force_y = bodies[0].force_z = 0.0;

    double star_mass = bodies[0].mass;
    double r_min = 0.05, r_max = DISK_RADIUS;

    for (int i = 1; i < n; i++) {
        double t     = (double)(i - 1) / (double)(n > 2 ? n - 2 : 1);
        double r     = r_min * pow(r_max / r_min, t);
        r += ((double)rand()/RAND_MAX - 0.5) * r * 0.05;

        double angle = ((double)rand()/RAND_MAX) * 2.0 * M_PI;

        bodies[i].pos_x = r * cos(angle);
        bodies[i].pos_y = r * sin(angle);
        bodies[i].pos_z = 0.0;

        double speed    = orbital_speed(star_mass, r);
        bodies[i].vel_x = -speed * sin(angle);
        bodies[i].vel_y =  speed * cos(angle);
        bodies[i].vel_z = 0.0;

        bodies[i].mass = BODY_MASS * (0.1 + (double)rand()/RAND_MAX * 0.5);
        bodies[i].force_x = bodies[i].force_y = bodies[i].force_z = 0.0;
    }
}

/* Config 4 - Two Colliding Galaxies ---------------------------------------- */
/* Two rotating disks approaching each other. The galaxies pass through,
 * exchanging stars and forming tidal streams before eventually merging. */
void config_two_galaxies(Body *bodies, int n) {
    srand(42);

    int half         = n / 2;
    double cx        = DISK_RADIUS * 1.5;
    double approach  = orbital_speed(CENTRAL_MASS, cx) * 0.5;

    for (int galaxy = 0; galaxy < 2; galaxy++) {
        int   start = galaxy * half;
        int   count = (galaxy == 0) ? half : n - half;
        double gx   = (galaxy == 0) ? -cx : cx;
        double spin = (galaxy == 0) ? 1.0 : -1.0;
        double vx   = (galaxy == 0) ? approach : -approach;

        /* Galaxy center */
        bodies[start].pos_x = gx;
        bodies[start].pos_y = bodies[start].pos_z = 0.0;
        bodies[start].vel_x = vx;
        bodies[start].vel_y = bodies[start].vel_z = 0.0;
        bodies[start].mass  = CENTRAL_MASS;
        bodies[start].force_x = bodies[start].force_y = bodies[start].force_z = 0.0;

        for (int i = start + 1; i < start + count; i++) {
            double r     = 0.05 + ((double)rand()/RAND_MAX) * DISK_RADIUS * 0.7;
            double angle = ((double)rand()/RAND_MAX) * 2.0 * M_PI;

            bodies[i].pos_x = gx + r * cos(angle);
            bodies[i].pos_y =       r * sin(angle);
            bodies[i].pos_z = ((double)rand()/RAND_MAX - 0.5) * 0.02;

            double speed    = orbital_speed(CENTRAL_MASS, r) * spin;
            bodies[i].vel_x = vx - speed * sin(angle);
            bodies[i].vel_y =      speed * cos(angle);
            bodies[i].vel_z = 0.0;

            bodies[i].mass = BODY_MASS * (0.5 + (double)rand()/RAND_MAX);
            bodies[i].force_x = bodies[i].force_y = bodies[i].force_z = 0.0;
        }
    }
}

/* Config 5 - Binary Stars -------------------------------------------------- */
/* Two equal stars in a stable mutual orbit at the center.
 * Remaining particles form a disk orbiting the pair as a whole. */
void config_binary_stars(Body *bodies, int n) {
    srand(42);

    if (n < 2) { config_random(bodies, n); return; }

    double binary_r = 0.08;
    /* Binary orbital speed: F_grav = F_centripetal
     * G*M^2/(2r)^2 = M*v^2/r  =>  v = sqrt(G*M/4r) */
    double binary_v = sqrt(G * STAR_MASS / (4.0 * binary_r));

    bodies[0].pos_x = -binary_r; bodies[0].pos_y = 0.0; bodies[0].pos_z = 0.0;
    bodies[0].vel_x =  0.0; bodies[0].vel_y = -binary_v; bodies[0].vel_z = 0.0;
    bodies[0].mass  = STAR_MASS;
    bodies[0].force_x = bodies[0].force_y = bodies[0].force_z = 0.0;

    bodies[1].pos_x =  binary_r; bodies[1].pos_y = 0.0; bodies[1].pos_z = 0.0;
    bodies[1].vel_x =  0.0; bodies[1].vel_y =  binary_v; bodies[1].vel_z = 0.0;
    bodies[1].mass  = STAR_MASS;
    bodies[1].force_x = bodies[1].force_y = bodies[1].force_z = 0.0;

    double total_mass = 2.0 * STAR_MASS;

    for (int i = 2; i < n; i++) {
        /* Disk well outside the binary separation */
        double r     = binary_r * 4.0 + ((double)rand()/RAND_MAX) * (DISK_RADIUS - binary_r * 4.0);
        double angle = ((double)rand()/RAND_MAX) * 2.0 * M_PI;

        bodies[i].pos_x = r * cos(angle);
        bodies[i].pos_y = r * sin(angle);
        bodies[i].pos_z = ((double)rand()/RAND_MAX - 0.5) * 0.02;

        double speed    = orbital_speed(total_mass, r);
        bodies[i].vel_x = -speed * sin(angle);
        bodies[i].vel_y =  speed * cos(angle);
        bodies[i].vel_z = 0.0;

        bodies[i].mass = BODY_MASS * (0.1 + (double)rand()/RAND_MAX);
        bodies[i].force_x = bodies[i].force_y = bodies[i].force_z = 0.0;
    }
}

/* -- Dispatcher ------------------------------------------------------------- */
static const char *config_names[] = {
    "Random", "Galaxy Disk", "Cold Collapse",
    "Solar System", "Two Galaxies", "Binary Stars"
};

void initialise_bodies(Body *bodies, int n, int config) {
    switch (config) {
        case 1:  config_galaxy_disk(bodies, n);   break;
        case 2:  config_cold_collapse(bodies, n);  break;
        case 3:  config_solar_system(bodies, n);   break;
        case 4:  config_two_galaxies(bodies, n);   break;
        case 5:  config_binary_stars(bodies, n);   break;
        default: config_random(bodies, n);         break;
    }
}

/* ============================================================
 * PHYSICS
 * ============================================================ */

void compute_forces(Body *bodies, int n) {
    for (int i = 0; i < n; i++)
        bodies[i].force_x = bodies[i].force_y = bodies[i].force_z = 0.0;

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            double dx = bodies[j].pos_x - bodies[i].pos_x;
            double dy = bodies[j].pos_y - bodies[i].pos_y;
            double dz = bodies[j].pos_z - bodies[i].pos_z;

            double dist_sq    = dx*dx + dy*dy + dz*dz + SOFTENING*SOFTENING;
            double dist       = sqrt(dist_sq);
            double dist_cubed = dist_sq * dist;
            double mag        = G * bodies[i].mass * bodies[j].mass / dist_cubed;

            double fx = mag * dx, fy = mag * dy, fz = mag * dz;
            bodies[i].force_x += fx;  bodies[j].force_x -= fx;
            bodies[i].force_y += fy;  bodies[j].force_y -= fy;
            bodies[i].force_z += fz;  bodies[j].force_z -= fz;
        }
    }
}

void update_bodies(Body *bodies, int n) {
    for (int i = 0; i < n; i++) {
        double ax = bodies[i].force_x / bodies[i].mass;
        double ay = bodies[i].force_y / bodies[i].mass;
        double az = bodies[i].force_z / bodies[i].mass;
        bodies[i].vel_x += ax * DT;
        bodies[i].vel_y += ay * DT;
        bodies[i].vel_z += az * DT;
        bodies[i].pos_x += bodies[i].vel_x * DT;
        bodies[i].pos_y += bodies[i].vel_y * DT;
        bodies[i].pos_z += bodies[i].vel_z * DT;
    }
}

/* ============================================================
 * RENDERING
 * ============================================================ */

/* Color ramp by mass: Blue -> Cyan -> Yellow -> Red */
void mass_to_color(double mass, double max_mass, Uint8 *r, Uint8 *g, Uint8 *b) {
    double t = mass / max_mass;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    if (t < 0.33) {
        *r = 0; *g = (Uint8)(t/0.33*255); *b = 255;
    } else if (t < 0.66) {
        double s = (t-0.33)/0.33;
        *r = (Uint8)(s*255); *g = 255; *b = (Uint8)((1.0-s)*255);
    } else {
        double s = (t-0.66)/0.34;
        *r = 255; *g = (Uint8)((1.0-s)*255); *b = 0;
    }
}

void draw_filled_circle(SDL_Renderer *renderer, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)sqrt((double)(radius*radius - dy*dy));
        SDL_RenderDrawLine(renderer, cx-dx, cy+dy, cx+dx, cy+dy);
    }
}

/* Viewport: mean +/- 3 standard deviations, ignores escaped outliers */
void compute_viewport(const Body *bodies, int n,
                      double *min_x, double *max_x,
                      double *min_y, double *max_y) {
    double mx = 0.0, my = 0.0;
    for (int i = 0; i < n; i++) { mx += bodies[i].pos_x; my += bodies[i].pos_y; }
    mx /= n; my /= n;

    double vx = 0.0, vy = 0.0;
    for (int i = 0; i < n; i++) {
        vx += (bodies[i].pos_x - mx) * (bodies[i].pos_x - mx);
        vy += (bodies[i].pos_y - my) * (bodies[i].pos_y - my);
    }
    double sx = sqrt(vx/n) * 3.0 + SOFTENING;
    double sy = sqrt(vy/n) * 3.0 + SOFTENING;

    *min_x = mx - sx; *max_x = mx + sx;
    *min_y = my - sy; *max_y = my + sy;
}

void render_frame(SDL_Renderer *renderer, const Body *bodies, int n, double max_mass) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    double min_x, max_x, min_y, max_y;
    compute_viewport(bodies, n, &min_x, &max_x, &min_y, &max_y);
    double rx = max_x - min_x, ry = max_y - min_y;

    for (int i = 0; i < n; i++) {
        int px = (int)((bodies[i].pos_x - min_x) / rx * WINDOW_WIDTH);
        int py = (int)((bodies[i].pos_y - min_y) / ry * WINDOW_HEIGHT);
        Uint8 r, g, b;
        mass_to_color(bodies[i].mass, max_mass, &r, &g, &b);
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        draw_filled_circle(renderer, px, py, PARTICLE_RADIUS);
    }

    SDL_RenderPresent(renderer);
}

/* ============================================================
 * MAIN
 * ============================================================ */
int main(int argc, char *argv[]) {
    int num_bodies = DEFAULT_NUM_BODIES;
    int num_steps  = DEFAULT_NUM_STEPS;
    int config     = 1;   /* Galaxy Disk by default - most visually impressive */

    if (argc >= 2) num_bodies = atoi(argv[1]);
    if (argc >= 3) num_steps  = atoi(argv[2]);
    if (argc >= 4) config     = atoi(argv[3]);
    if (config < 0 || config > 5) config = 0;

    printf("Config : %d - %s\n", config, config_names[config]);
    printf("Bodies : %d\n", num_bodies);
    printf("Steps  : %d\n\n", num_steps);

    Body *bodies = malloc(num_bodies * sizeof(Body));
    if (!bodies) { fprintf(stderr, "Out of memory\n"); return 1; }
    initialise_bodies(bodies, num_bodies, config);

    double max_mass = 0.0;
    for (int i = 0; i < num_bodies; i++)
        if (bodies[i].mass > max_mass) max_mass = bodies[i].mass;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL error: %s\n", SDL_GetError());
        free(bodies); return 1;
    }

    char title[64];
    snprintf(title, sizeof(title), "%s  |  N=%d", config_names[config], num_bodies);
    SDL_Window *window = SDL_CreateWindow(title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    /* Target 60 fps regardless of body count.
     * If compute+render finishes early, sleep the remainder of the frame.
     * If it takes longer (many bodies), just run as fast as possible. */
    const int target_frame_ms = 1000 / 120;

    int running = 1;
    for (int step = 0; step < num_steps && running; step++) {
        Uint32 frame_start = SDL_GetTicks();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }
        compute_forces(bodies, num_bodies);
        update_bodies(bodies, num_bodies);
        render_frame(renderer, bodies, num_bodies, max_mass);

        int elapsed_ms = (int)(SDL_GetTicks() - frame_start);
        if (elapsed_ms < target_frame_ms)
            SDL_Delay(target_frame_ms - elapsed_ms);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(bodies);
    printf("Done.\n");
    return 0;
}