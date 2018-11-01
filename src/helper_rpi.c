#ifdef TARGET_RPI

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <bcm_host.h>

#include "gpio_helper.h"

/**
 * gpio_helper (rpi)
 *
 * This is a helper application for handling GPIO operations that don't
 * feel comfortable being in the same process as the rest of the Erlang
 * VM.
 *
 * Call by:
 *
 * gpio_helper <pin_number> <mode>
 *
 * mode is 0 to disable pull ups/pull downs, 1 for a pulldown and 2 for
 * a pull up.
 */

#define GPIO_HELPER_DISABLE_PULLUPS 0
#define GPIO_HELPER_ENABLE_PULLDOWN 1
#define GPIO_HELPER_ENABLE_PULLUP 2

#define GPIO_MAP_BLOCK_SIZE (4*1024)
#define PAGE_SIZE  (4*1024)
#define GPIO_BASE_OFFSET    0x200000

#define GPPUD_OFFSET        37
#define GPPUDCLK0_OFFSET    38
#define DISABLE_PULLUP_DOWN 0
#define ENABLE_PULLDOWN     1
#define ENABLE_PULLUP       2

static uint32_t *get_gpio_map()
{
    debug("get_gpio_map()");

    // Try "/dev/gpiomem" and then "/dev/mem"
    int mem_fd = open("/dev/gpiomem", O_RDWR | O_SYNC);
    if (mem_fd >= 0) {
        debug("get_gpio_map() open() /dev/gpiomem, success");
        void *map = mmap(NULL, GPIO_MAP_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0);
        if (*((int32_t*)map) < 0) {
            err(EXIT_FAILURE, "get_gpio_map() mmap(), failed");
        } else {
            debug("get_gpio_map() mmap(), success");
            return (uint32_t *) map;
        }
    }
    debug("get_gpio_map() open('/dev/gpiomem') failed. Going to try '/dev/mem'");

    // mmap the GPIO memory registers
    mem_fd = open("/dev/mem", O_RDWR|O_SYNC);
    if (mem_fd < 0)
        err(EXIT_FAILURE, "Can't open /dev/gpiomem or /dev/mem");

    uint8_t *gpio_mem = malloc(GPIO_MAP_BLOCK_SIZE + (PAGE_SIZE-1));
    if (gpio_mem == NULL)
        err(EXIT_FAILURE, "get_gpio_map() 2 malloc(), failed");

    if ((uint32_t)gpio_mem % PAGE_SIZE)
        gpio_mem += PAGE_SIZE - ((uint32_t)gpio_mem % PAGE_SIZE);

    uint32_t gpio_base = bcm_host_get_peripheral_address() + GPIO_BASE_OFFSET;
    debug("get_gpio_map() 2 peri_addr 0x%08x", gpio_base);

    uint32_t *map = (uint32_t *) mmap((void *) gpio_mem, GPIO_MAP_BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, mem_fd, gpio_base);
    if ((int32_t) map < 0)
        err(EXIT_FAILURE, "get_gpio_map() 2 mmap(), failed");

    debug("get_gpio_map() 2 mmap(), success");
    return map;
}

static int write_pull_mode(uint32_t *gpio_map, int pin_number, int pull_mode)
{
    uint32_t  clk_bit_to_set = 1 << (pin_number%32);
    uint32_t *gpio_pud_clk = gpio_map + GPPUDCLK0_OFFSET + (pin_number/32);
    uint32_t *gpio_pud = gpio_map + GPPUD_OFFSET;

    // Steps to connect or disconnect pull up/down resistors on a gpio pin:

    // 1. Write to GPPUD to set the required control signal
    if (pull_mode == GPIO_HELPER_ENABLE_PULLDOWN)
        *gpio_pud = (*gpio_pud & ~3) | ENABLE_PULLDOWN;
    else if (pull_mode == GPIO_HELPER_ENABLE_PULLUP)
        *gpio_pud = (*gpio_pud & ~3) | ENABLE_PULLUP;
    else  // pull_mode == GPIO_HELPER_DISABLE_PULLUPS
        *gpio_pud &= ~3;

    // 2. Wait 150 cycles  this provides the required set-up time for the control signal
    usleep(1);

    // 3. Write to GPPUDCLK0/1 to clock the control signal into the GPIO pads you wish to modify
    *gpio_pud_clk = clk_bit_to_set;

    // 4. Wait 150 cycles  this provides the required hold time for the control signal
    usleep(1);

    // 5. Write to GPPUD to remove the control signal
    *gpio_pud &= ~3;

    // 6. Write to GPPUDCLK0/1 to remove the clock
    *gpio_pud_clk = 0;

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
        errx(EXIT_FAILURE, "(rpi) pass pin_number and pull_mode (0-3)\n");

    // Parse the arguments. Since this is only intended to be called from
    // the NIF for GPIO operations that aren't safe to run in the Erlang
    // VM, not much parameter validation is done.
    int pin_number = strtoul(argv[1], NULL, 10);
    int mode = strtoul(argv[2], NULL, 10);

    uint32_t *gpio_map = get_gpio_map();

    write_pull_mode(gpio_map, pin_number, mode);

    exit(EXIT_SUCCESS);
}
#endif // TARGET_RPI
