#include "job_catalog.h"
#include <string.h>

int job_add(const int *args, size_t argc) {
    int res = 0;
    for (size_t i = 0; i < argc; i++) {
        res += args[i];
    }
    return res;
}

int job_mul(const int *args, size_t argc) {
    if (argc == 0) return 0;
    int res = 1;
    for (size_t i = 0; i < argc; i++) {
        res *= args[i];
    }
    return res;
}

int job_factorial(const int *args, size_t argc) {
    if (argc < 1) return 0;
    int n = args[0];
    if (n < 0) return 0;
    if (n == 0 || n == 1) return 1;

    int res = 1;
    for (int i = 2; i <= n; i++) {
        res *= i;
    }
    return res;
}

int job_fibonacci(const int *args, size_t argc) {
    if (argc < 1) return 0;
    
    int n = args[0];
    if (n <= 0) return 0;
    if (n == 1) return 1;

    int a = 0, b = 1, temp;
    for (int i = 2; i <= n; i++) {
        temp = a + b;
        a = b;
        b = temp;
    }
    return b;
}

static job_entry_t catalog[] = {
    { "math", "add", job_add },
    { "math", "multiply", job_mul },
    {"math", "fib", job_fibonacci},
    { "math", "factorial", job_factorial },
    {NULL, NULL, NULL} // Sentinela para o fim da listas
};

job_fn_t job_catalog_lookup(const char *service, const char *function) {
    if (!service || !function) return NULL;

    for (int i = 0; catalog[i].fn != NULL; i++) {
        if (strcmp(catalog[i].service, service) == 0 && 
            strcmp(catalog[i].function, function) == 0) {
            return catalog[i].fn;
        }
    }
    return NULL;
}
