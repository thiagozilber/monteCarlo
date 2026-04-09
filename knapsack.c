#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Default problem parameters ─────────────────────────────────────────── */
#define DEFAULT_N    1000   /* number of items  */
#define DEFAULT_W    5000   /* knapsack capacity */
#define DEFAULT_SEED 42

/* ── Item definition ────────────────────────────────────────────────────── */
typedef struct {
    int weight;
    int value;
} Item;

/* ── Generate a random problem instance ────────────────────────────────── */
void generate_items(Item *items, int n, int max_weight, int max_value, unsigned int seed) {
    srand(seed);
    for (int i = 0; i < n; i++) {
        items[i].weight = rand() % max_weight + 1;  /* weight in [1, max_weight] */
        items[i].value  = rand() % max_value  + 1;  /* value  in [1, max_value]  */
    }
}

/* ── Solve using 2D DP table ────────────────────────────────────────────── */
/*
 * Returns the optimal value.
 * dp must be a pre-allocated (n+1) × (W+1) array (row-major).
 * Access: dp[i*(W+1) + j]
 */
int knapsack(const Item *items, int n, int W, int *dp) {
    /* Base case: 0 items or 0 capacity → value = 0 */
    for (int j = 0; j <= W; j++) dp[j] = 0;          /* row 0 */

    for (int i = 1; i <= n; i++) {
        int w = items[i-1].weight;
        int v = items[i-1].value;

        /* ── Inner loop: fill row i ──────────────────────────────────────── */
        for (int j = 0; j <= W; j++) {
            int skip = dp[(i-1)*(W+1) + j];            /* don't take item i */
            int take = (j >= w)                        /* if weight */
                       ? dp[(i-1)*(W+1) + (j-w)] + v   /* take item i       */
                       : -1;                           /* item doesn't fit  */
            dp[i*(W+1) + j] = (take > skip) ? take : skip;
        }
    }

    return dp[n*(W+1) + W];
}

/* ── Traceback: recover which items were selected ───────────────────────── */
/*
 * Walks the DP table backwards to find the chosen items.
 * selected[i] = 1 if item i was taken, 0 otherwise.
 */
void traceback(const Item *items, int n, int W, const int *dp, int *selected) {
    int j = W;
    for (int i = n; i >= 1; i--) {
        if (dp[i*(W+1) + j] != dp[(i-1)*(W+1) + j]) {
            selected[i-1] = 1;
            j -= items[i-1].weight;
        } else {
            selected[i-1] = 0;
        }
    }
}

/* ── Pretty-print results ───────────────────────────────────────────────── */
void print_summary(const Item *items, int n, int W,
                   int optimal_value, const int *selected,
                   double elapsed_sec) {
    int total_weight = 0, total_value = 0, count = 0;
    for (int i = 0; i < n; i++) {
        if (selected[i]) {
            total_weight += items[i].weight;
            total_value  += items[i].value;
            count++;
        }
    }

    printf("─────────────────────────────────\n");
    printf("  Items available  : %d\n", n);
    printf("  Capacity (W)     : %d\n", W);
    printf("─────────────────────────────────\n");
    printf("  Items selected   : %d\n", count);
    printf("  Total weight     : %d / %d\n", total_weight, W);
    printf("  Optimal value    : %d\n", optimal_value);
    printf("  Verify value     : %d  %s\n", total_value,
           total_value == optimal_value ? "✓" : "✗ MISMATCH");
    printf("─────────────────────────────────\n");
    printf("  Time (serial)    : %.4f s\n", elapsed_sec);
    printf("─────────────────────────────────\n");
}

/* ── Entry point ────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    int          N    = DEFAULT_N;
    int          W    = DEFAULT_W;
    unsigned int seed = DEFAULT_SEED;

    if (argc >= 2) N    = atoi(argv[1]);
    if (argc >= 3) W    = atoi(argv[2]);
    if (argc >= 4) seed = (unsigned int)atoi(argv[3]);

    /* Allocate items */
    Item *items = malloc(N * sizeof(Item));
    if (!items) { fprintf(stderr, "Out of memory\n"); return 1; }

    generate_items(items, N, W / 10, 500, seed);  /* weights up to W/10, values up to 500 */

    /* Allocate DP table: (N+1) rows × (W+1) columns */
    long long table_size = (long long)(N + 1) * (W + 1);
    int *dp = malloc(table_size * sizeof(int));
    if (!dp) { fprintf(stderr, "Out of memory (table too large)\n"); free(items); return 1; }

    printf("Problem: N=%d items, W=%d capacity (table: %.1f MB)\n\n",
           N, W, table_size * sizeof(int) / 1e6);

    /* Solve and time */
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int optimal = knapsack(items, N, W, dp);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;

    /* Traceback */
    int *selected = calloc(N, sizeof(int));
    traceback(items, N, W, dp, selected);

    print_summary(items, N, W, optimal, selected, elapsed);

    free(items);
    free(dp);
    free(selected);
    return 0;
}