#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include "string.h"

// Prototipos de funciones de logger
void logger_init(void);
void logger_write(const char* event);

#endif