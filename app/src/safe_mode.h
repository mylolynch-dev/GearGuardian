/**
 * @file safe_mode.h
 * @brief Safe mode entry handler.
 */

#ifndef SAFE_MODE_H
#define SAFE_MODE_H

#include <stdint.h>

void safe_mode_enter(uint32_t fault_flags);
void safe_mode_exit(void);

#endif /* SAFE_MODE_H */
