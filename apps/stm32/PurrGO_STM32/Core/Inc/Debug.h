#ifndef __DEBUG_H
#define __DEBUG_H

#include "purrgo/logger.h"

#define DEBUG 1
#if DEBUG
	#define Debug(__info,...) PURRGO_LOG("Debug: " __info,##__VA_ARGS__)
#else
	#define Debug(__info,...)
#endif

#endif
