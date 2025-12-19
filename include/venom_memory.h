/*
 * VenomMemory - High-Performance Shared Memory IPC Library
 * 
 * venom_memory.h - Main public header (single include)
 */

#ifndef VENOM_MEMORY_H
#define VENOM_MEMORY_H

/* Version information */
#define VENOM_VERSION_MAJOR 1
#define VENOM_VERSION_MINOR 0
#define VENOM_VERSION_PATCH 0

#define VENOM_VERSION_STRING "1.0.0"

/* Include all public headers */
#include "venom_channel.h"
#include "venom_shm.h"
#include "venom_sync.h"

#endif /* VENOM_MEMORY_H */
