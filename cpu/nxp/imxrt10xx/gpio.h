#ifndef imxrt10xx_gpio_h
#define imxrt10xx_gpio_h

#include <assert.h>
#include <stdint.h>

/*
 * General Purpose Input/Output
 */
enum gpio_icr {
	GPIO_ICR_LOW_LEVEL = 0,
	GPIO_ICR_HIGH_LEVEL = 1,
	GPIO_ICR_RISING_EDGE = 2,
	GPIO_ICR_FALLING_EDGE = 3,
};

struct gpio {
	uint32_t DR;
	uint32_t GDIR;
	uint32_t PSR;
	union {
		struct {
			union {
				struct {
					enum gpio_icr ICR0 : 2;
					enum gpio_icr ICR1 : 2;
					enum gpio_icr ICR2 : 2;
					enum gpio_icr ICR3 : 2;
					enum gpio_icr ICR4 : 2;
					enum gpio_icr ICR5 : 2;
					enum gpio_icr ICR6 : 2;
					enum gpio_icr ICR7 : 2;
					enum gpio_icr ICR8 : 2;
					enum gpio_icr ICR9 : 2;
					enum gpio_icr ICR10 : 2;
					enum gpio_icr ICR11 : 2;
					enum gpio_icr ICR12 : 2;
					enum gpio_icr ICR13 : 2;
					enum gpio_icr ICR14 : 2;
					enum gpio_icr ICR15 : 2;
				};
				uint32_t r;
			} ICR1;
			union {
				struct {
					enum gpio_icr ICR16 : 2;
					enum gpio_icr ICR17 : 2;
					enum gpio_icr ICR18 : 2;
					enum gpio_icr ICR19 : 2;
					enum gpio_icr ICR20 : 2;
					enum gpio_icr ICR21 : 2;
					enum gpio_icr ICR22 : 2;
					enum gpio_icr ICR23 : 2;
					enum gpio_icr ICR24 : 2;
					enum gpio_icr ICR25 : 2;
					enum gpio_icr ICR26 : 2;
					enum gpio_icr ICR27 : 2;
					enum gpio_icr ICR28 : 2;
					enum gpio_icr ICR29 : 2;
					enum gpio_icr ICR30 : 2;
					enum gpio_icr ICR31 : 2;
				};
				uint32_t r;
			} ICR2;
		};
		uint32_t ICR[2];
	};
	uint32_t IMR;
	uint32_t ISR;
	uint32_t EDGE_SEL;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t : 32;
	uint32_t DR_SET;
	uint32_t DR_CLEAR;
	uint32_t DR_TOGGLE;
};
static_assert(sizeof(struct gpio) == 0x90, "");

static struct gpio *const GPIO1 = (struct gpio*)0x401b8000;
static struct gpio *const GPIO2 = (struct gpio*)0x401bc000;
static struct gpio *const GPIO3 = (struct gpio*)0x401c0000;
static struct gpio *const GPIO4 = (struct gpio*)0x401c4000;
static struct gpio *const GPIO5 = (struct gpio*)0x400c0000;
static struct gpio *const GPIO6 = (struct gpio*)0x42000000;
static struct gpio *const GPIO7 = (struct gpio*)0x42004000;
static struct gpio *const GPIO8 = (struct gpio*)0x42008000;
static struct gpio *const GPIO9 = (struct gpio*)0x4200c000;

#endif
