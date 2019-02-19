#pragma once

#define STATIC_ASSERT(condition) \
	((void)sizeof(char[1 - 2*!(condition)]))
