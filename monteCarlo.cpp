double monte_carlo_serial(long long n) {
    long long inside = 0;
    unsigned int seed = 42;
 
    for (long long i = 0; i < n; i++) {
        double x = (double)rand_r(&seed) / RAND_MAX;
        double y = (double)rand_r(&seed) / RAND_MAX;
        if (x * x + y * y <= 1.0)
            inside++;
    }
 
    return 4.0 * (double)inside / (double)n;
}