#ifndef dev_regulator_voltage_voltage_h
#define dev_regulator_voltage_voltage_h

/*
 * Generic voltage regulator support
 */

#include "desc.h"

#ifdef __cplusplus
extern "C" {
#endif

struct volt_reg;

struct volt_reg *volt_reg_bind(const struct volt_reg_desc *);
float volt_reg_get(const struct volt_reg *);
int volt_reg_set(struct volt_reg *, float voltage_min, float voltage_max);
int volt_reg_supports(const struct volt_reg *, float voltage_min, float voltage_max);
bool volt_reg_equal(const struct volt_reg *, const struct volt_reg *);

#ifdef __cplusplus
}
#endif

#endif
