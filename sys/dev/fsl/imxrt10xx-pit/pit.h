#pragma once

#include <chrono>
#include <functional>
#include <sync.h>

struct fsl_imxrt10xx_pit_desc;
extern "C" void fsl_imxrt10xx_pit_init(const fsl_imxrt10xx_pit_desc *);

namespace imxrt10xx {

/*
 * Periodic Interrupt Timer Module
 */
class pit {
public:
	static constexpr auto channels = 4u;
	using isr_fn = std::function<void(unsigned)>;

	int start(unsigned, std::chrono::nanoseconds);
	void stop(unsigned);
	std::chrono::nanoseconds get(unsigned);
	int irq_attach(unsigned, isr_fn);
	void irq_detach(unsigned);
	static pit *inst();

private:
	struct regs;
	pit(const fsl_imxrt10xx_pit_desc *d);
	pit(const pit&) = delete;
	pit& operator=(const pit&) = delete;

	a::spinlock_irq lock_;
	isr_fn irq_table_[channels];
	regs const *r_;
	const unsigned long clock_;

	void isr();
	static int isr_wrapper(int, void *);

	friend void ::fsl_imxrt10xx_pit_init(const fsl_imxrt10xx_pit_desc *);
};

}
