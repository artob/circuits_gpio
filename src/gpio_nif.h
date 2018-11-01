#ifndef GPIO_NIF_H
#define GPIO_NIF_H

#include "erl_nif.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

//#define DEBUG

#ifdef DEBUG
#define log_location stderr
//#define LOG_PATH "/tmp/circuits_gpio.log"
#define debug(...) do { enif_fprintf(log_location, __VA_ARGS__); enif_fprintf(log_location, "\r\n"); fflush(log_location); } while(0)
#define error(...) do { debug(__VA_ARGS__); } while (0)
#define start_timing() ErlNifTime __start = enif_monotonic_time(ERL_NIF_USEC)
#define elapsed_microseconds() (enif_monotonic_time(ERL_NIF_USEC) - __start)
#else
#define debug(...)
#define error(...) do { enif_fprintf(stderr, __VA_ARGS__); enif_fprintf(stderr, "\n"); } while(0)
#define start_timing()
#define elapsed_microseconds() 0
#endif

#define MAX_GPIO_LISTENERS 32

enum edge_mode {
    EDGE_NONE,
    EDGE_RISING,
    EDGE_FALLING,
    EDGE_BOTH
};

enum pull_mode {
    PULL_NOT_SET,
    PULL_NONE,
    PULL_UP,
    PULL_DOWN
};

struct gpio_priv {
    ERL_NIF_TERM atom_ok;

    ErlNifResourceType *gpio_pin_rt;

    uint32_t hal_priv[1];
};

struct gpio_config {
    bool is_output;
    enum edge_mode edge;
    enum pull_mode pull;
    bool suppress_glitches;
    ErlNifPid pid;
};

struct gpio_pin {
    int pin_number;
    int fd;
    void *hal_priv;
    struct gpio_config config;
};

// HAL

/**
 * Return information about the HAL.
 *
 * This should return a map with the name of the HAL and any info that
 * would help debug issues with it.
 */
ERL_NIF_TERM hal_info(ErlNifEnv *env);

size_t hal_priv_size(void);
int hal_load(void *hal_priv);
void hal_set_helper_path(void *hal_priv, const char *path);
void hal_unload(void *hal_priv);
int hal_open_gpio(struct gpio_pin *pin,
                  char *error_str);
void hal_close_gpio(struct gpio_pin *pin);
int hal_read_gpio(struct gpio_pin *pin);
int hal_write_gpio(struct gpio_pin *pin, int value);
int hal_apply_edge_mode(struct gpio_pin *pin);
int hal_apply_pull_mode(struct gpio_pin *pin);
int hal_apply_direction(struct gpio_pin *pin);

// nif_utils.c
ERL_NIF_TERM make_ok_tuple(ErlNifEnv *env, ERL_NIF_TERM value);
ERL_NIF_TERM make_error_tuple(ErlNifEnv *env, const char *reason);
int enif_get_boolean(ErlNifEnv *env, ERL_NIF_TERM term, bool *v);

int send_gpio_message(ErlNifEnv *env,
                      ERL_NIF_TERM atom_gpio,
                      int pin_number,
                      ErlNifPid *pid,
                      int64_t timestamp,
                      int value);

#endif // GPIO_NIF_H
