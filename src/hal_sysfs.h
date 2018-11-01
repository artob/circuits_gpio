#ifndef HAL_SYSFS_H
#define HAL_SYSFS_H

#include <stdint.h>
#include "erl_nif.h"

struct sysfs_priv {
    ErlNifTid poller_tid;
    int pipe_fds[2];

    char *helper_path;
};

int sysfs_read_gpio(int fd);
void *gpio_poller_thread(void *arg);
int update_polling_thread(struct gpio_pin *pin);

#endif // HAL_SYSFS_H
