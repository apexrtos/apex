#pragma once

/*
 * GPIO IRQ Modes
 */

#ifdef __cplusplus
namespace gpio {

enum class irq_mode {
	edge_rising,
	edge_falling,
	edge_both,
	high,
	low,
};

}
#endif
