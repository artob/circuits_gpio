/*
 *  Copyright 2018 Frank Hunleth, Mark Sebald
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * GPIO NIF implementation.
 */

#include "gpio_nif.h"

#include <string.h>

#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "hal_sysfs.h"

size_t hal_priv_size()
{
    return sizeof(struct sysfs_priv);
}

static ssize_t sysfs_write_file(const char *pathname, const char *value)
{
    int fd = open(pathname, O_WRONLY);
    if (fd < 0) {
        error("Error opening %s", pathname);
        return -1;
    }

    size_t count = strlen(value);
    ssize_t written = write(fd, value, count);
    close(fd);

    if (written < 0 || (size_t) written != count) {
        error("Error writing '%s' to %s", value, pathname);
        return -1;
    }
    return written;
}

static int export_pin(int pin_number)
{
    char pinstr[16];
    sprintf(pinstr, "%d", pin_number);
    if (sysfs_write_file("/sys/class/gpio/export", pinstr) <= 0)
        return - 1;

    return 0;
}

static const char *edge_mode_string(enum edge_mode mode)
{
    switch (mode) {
    default:
    case EDGE_NONE:
        return "none";
    case EDGE_FALLING:
        return "falling";
    case EDGE_RISING:
        return "rising";
    case EDGE_BOTH:
        return "both";
    }
}

ERL_NIF_TERM hal_info(ErlNifEnv *env)
{
    ERL_NIF_TERM info = enif_make_new_map(env);
    enif_make_map_put(env, info, enif_make_atom(env, "name"), enif_make_atom(env, "sysfs"), &info);
    return info;
}

int hal_load(void *hal_priv)
{
    struct sysfs_priv *priv = hal_priv;
    memset(priv, 0, sizeof(struct sysfs_priv));

    if (pipe(priv->pipe_fds) < 0) {
        error("pipe failed");
        return 1;
    }

    if (enif_thread_create("gpio_poller", &priv->poller_tid, gpio_poller_thread, &priv->pipe_fds[0], NULL) != 0) {
        error("enif_thread_create failed");
        return 1;
    }

    return 0;
}

void hal_set_helper_path(void *hal_priv, const char *path)
{
    struct sysfs_priv *priv = hal_priv;
    if (priv->helper_path)
        free(priv->helper_path);
    priv->helper_path = strdup(path);
}

void hal_unload(void *hal_priv)
{
    struct sysfs_priv *priv = hal_priv;

    // Close everything related to the listening thread so that it exits
    close(priv->pipe_fds[0]);
    close(priv->pipe_fds[1]);

    // If the listener thread hasn't exited already, it should do so soon.
    enif_thread_join(priv->poller_tid, NULL);

    // TODO free everything else!
}

int hal_open_gpio(struct gpio_pin *pin,
                  char *error_str)
{
    *error_str = '\0';

    char value_path[64];
    sprintf(value_path, "/sys/class/gpio/gpio%d/value", pin->pin_number);
    pin->fd = open(value_path, O_RDWR);
    if (pin->fd < 0) {
        if (export_pin(pin->pin_number) < 0) {
            strcpy(error_str, "export_failed");
            return -1;
        }

        pin->fd = open(value_path, O_RDWR);
        if (pin->fd < 0) {
            strcpy(error_str, "access_denied");
            return -1;
        }
    }
    if (hal_apply_direction(pin) < 0) {
        strcpy(error_str, "error_setting_direction");
        goto error;
    }
    if (hal_apply_edge_mode(pin) < 0) {
        strcpy(error_str, "error_setting_edge_mode");
        goto error;
    }

    return 0;

error:
    close(pin->fd);
    pin->fd = -1;
    return -1;
}

void hal_close_gpio(struct gpio_pin *pin)
{
    if (pin->fd >= 0) {
        close(pin->fd);
        pin->fd = -1;
    }
}

int sysfs_read_gpio(int fd)
{
    char buf;
    ssize_t amount_read = pread(fd, &buf, sizeof(buf), 0);
    if (amount_read == sizeof(buf))
        return buf == '1';
    else
        return -1;
}

int hal_read_gpio(struct gpio_pin *pin)
{
    return sysfs_read_gpio(pin->fd);
}

int hal_write_gpio(struct gpio_pin *pin, int value)
{
    char buff = value ? '1' : '0';
    return (int) pwrite(pin->fd, &buff, sizeof(buff), 0);
}

int hal_apply_edge_mode(struct gpio_pin *pin)
{
    char edge_path[64];
    sprintf(edge_path, "/sys/class/gpio/gpio%d/edge", pin->pin_number);
    if (access(edge_path, F_OK) != -1) {
        /* Allow 1000 * 1ms = 1 second max for retries. This is a workaround
         * for a first-time initialization issue where the file doesn't appear
         * quickly after export */
        int retries = 1000;
        while (sysfs_write_file(edge_path, edge_mode_string(pin->config.edge)) <= 0 && retries > 0) {
            usleep(1000);
            retries--;
        }
        if (retries == 0)
            return -1;
    }

    // Tell polling thread to wait for notifications
    if (update_polling_thread(pin) < 0)
        return -1;

    return 0;
}

int hal_apply_direction(struct gpio_pin *pin)
{
    char direction_path[64];
    sprintf(direction_path, "/sys/class/gpio/gpio%d/direction", pin->pin_number);
    if (access(direction_path, F_OK) != -1) {
        const char *dir_string = (pin->config.is_output ? "out" : "in");
        int retries = 1000; /* Allow 1000 * 1ms = 1 second max for retries */
        while (sysfs_write_file(direction_path, dir_string) <= 0 && retries > 0) {
            usleep(1000);
            retries--;
        }
        if (retries == 0)
            return -1;
    }
    return 0;
}

#include <errno.h>
#include <sys/wait.h>

static int run_helper(const char *path, char *argv[])
{
    pid_t pid = fork();
    if (pid == 0) {
        // child
        execvp(path, argv);

        // Not supposed to reach here.
        exit(EXIT_FAILURE);
    } else {
        // parent
        int status = -1;
        int rc;
        do {
            rc = waitpid(pid, &status, 0);
        } while (rc < 0 && errno == EINTR);

        if ((rc < 0 && errno != ECHILD) || rc != pid) {
            error("gpio_helper: unexpected return from waitpid: rc=%d, errno=%d", rc, errno);
            return -1;
        }
        return status;
    }
}

int hal_apply_pull_mode(struct gpio_pin *pin)
{
    // Setting the pull mode is unsafe for a NIF and platform-specific
    // Call out to the helper for run this.

    struct sysfs_priv *priv = pin->hal_priv;
    if (priv->helper_path == NULL) {
        // No helper, so this is an error unless the pull mode is "don't set"
        if (pin->config.pull == PULL_NOT_SET)
            return 0;
        else
            return -1;
    }

    char pin_string[8];
    char pull_string[8];
    sprintf(pin_string, "%d", pin->pin_number);
    sprintf(pull_string, "%d", pin->config.pull);

    char *args[4];
    args[0] = "gpio_helper";
    args[1] = pin_string;
    args[2] = pull_string;
    args[3] = NULL;
    int rc = run_helper(priv->helper_path, args);

    if (rc == 0)
        return 0;
    else
        return -1;
}
