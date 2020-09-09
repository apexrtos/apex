#include "init.h"

#include <arch.h>
#include <debug.h>
#include <dev/gpio/controller.h>
#include <errno.h>
#include <irq.h>

namespace {

/*
 * regs
 */
struct regs {
	enum icr {
		ICR_LOW_LEVEL = 0,
		ICR_HIGH_LEVEL = 1,
		ICR_RISING_EDGE = 2,
		ICR_FALLING_EDGE = 3,
	};
	uint32_t DR;
	uint32_t GDIR;
	uint32_t PSR;
	uint32_t ICR1;
	uint32_t ICR2;
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
static_assert(sizeof(regs) == 0x90, "");

/*
 * imxrt10xx_gpio
 */
class imxrt10xx_gpio final : public gpio::controller {
public:
	imxrt10xx_gpio(std::string_view name, regs *r);

	void isr();

private:
	virtual bool v_get(size_t) override;
	virtual void v_set(size_t, bool) override;
	virtual void v_direction_input(size_t) override;
	virtual void v_direction_output(size_t) override;
	virtual int v_interrupt_setup(size_t, gpio::irq_mode) override;
	virtual void v_interrupt_mask(size_t) override;
	virtual void v_interrupt_unmask(size_t) override;

	a::spinlock_irq lock_;
	regs *const r_;

	static constexpr auto pins_ = 32;
};

/*
 * imxrt10xx_gpio::imxrt10xx_gpio
 */
imxrt10xx_gpio::imxrt10xx_gpio(std::string_view name, regs *r)
: gpio::controller{name, pins_}
, r_{r}
{ }

/*
 * imxrt10xx_gpio::isr
 */
void
imxrt10xx_gpio::isr()
{
	uint32_t s = read32(&r_->ISR) & read32(&r_->IMR);
	write32(&r_->ISR, s);

	int i;
	while ((i = __builtin_ffsl(s))) {
		i -= 1;		    /* ffsl returns 1 + bit number */
		irq(i);
		s -= 1ul << i;
	}
}

/*
 * imxrt10xx_gpio::v_get
 */
bool
imxrt10xx_gpio::v_get(size_t pin)
{
	return read32(&r_->PSR) & (1ul << pin);
}

/*
 * imxrt10xx_gpio::v_set
 */
void
imxrt10xx_gpio::v_set(size_t pin, bool state)
{
	if (state)
		write32(&r_->DR_SET, 1ul << pin);
	else
		write32(&r_->DR_CLEAR, 1ul << pin);
}

/*
 * imxrt10xx_gpio::v_direction_input
 */
void
imxrt10xx_gpio::v_direction_input(size_t pin)
{
	std::lock_guard l{lock_};
	uint32_t val = read32(&r_->GDIR);
	val &= ~(1ul << pin);
	write32(&r_->GDIR, val);
}

/*
 * imxrt10xx_gpio::v_direction_output
 */
void
imxrt10xx_gpio::v_direction_output(size_t pin)
{
	std::lock_guard l{lock_};
	uint32_t val = read32(&r_->GDIR);
	val |= 1ul << pin;
	write32(&r_->GDIR, val);
}

/*
 * imxrt10xx_gpio::v_interrupt_setup
 */
int
imxrt10xx_gpio::v_interrupt_setup(size_t pin, gpio::irq_mode mode)
{
	uint32_t icr;

	switch (mode) {
	case gpio::irq_mode::edge_rising:
		icr = regs::ICR_RISING_EDGE;
		break;
	case gpio::irq_mode::edge_falling:
		icr = regs::ICR_FALLING_EDGE;
		break;
	case gpio::irq_mode::edge_both:
		break;
	case gpio::irq_mode::high:
		icr = regs::ICR_HIGH_LEVEL;
		break;
	case gpio::irq_mode::low:
		icr = regs::ICR_LOW_LEVEL;
		break;
	default:
		return DERR(-EINVAL);
	}

	std::lock_guard l{lock_};

	if (mode == gpio::irq_mode::edge_both)
		write32(&r_->EDGE_SEL, read32(&r_->EDGE_SEL) | 1ul << pin);
	else {
		write32(&r_->EDGE_SEL, read32(&r_->EDGE_SEL) & ~(1ul << pin));

		if (pin < 16)
			write32(&r_->ICR1, read32(&r_->ICR1) | icr << pin * 2);
		else
			write32(&r_->ICR2, read32(&r_->ICR2) | icr << (pin - 16) * 2);
	}

	/* clear stale interrupts */
	write32(&r_->ISR, 1ul << pin);

	return 0;
}

/*
 * imxrt10xx_gpio::v_interrupt_mask
 */
void
imxrt10xx_gpio::v_interrupt_mask(size_t pin)
{
	std::lock_guard l{lock_};
	write32(&r_->IMR, read32(&r_->IMR) & ~(1ul << pin));
}

/*
 * imxrt10xx_gpio::v_interrupt_unmask
 */
void
imxrt10xx_gpio::v_interrupt_unmask(size_t pin)
{
	std::lock_guard l{lock_};
	write32(&r_->IMR, read32(&r_->IMR) | 1ul << pin);
}

/*
 * isr
 */
int
isr(int vector, void *data)
{
	auto u = static_cast<imxrt10xx_gpio *>(data);
	u->isr();
	return INT_DONE;
}

}

extern "C" {

/*
 * fsl_imxrt10xx_gpio_init
 */
void
fsl_imxrt10xx_gpio_init(const fsl_imxrt10xx_gpio_desc *d)
{
	auto g = new imxrt10xx_gpio{d->name, reinterpret_cast<regs *>(d->base)};
	gpio::controller::add(g);
	irq_attach(d->irqs[0], d->ipl, 0, isr, nullptr, g);
	irq_attach(d->irqs[1], d->ipl, 0, isr, nullptr, g);
}

}
