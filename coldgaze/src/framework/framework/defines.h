#pragma once

#define CG_INIT_SUCCESS 0

#ifdef _WIN32
#define CG_DEPRECATED_MSG(msg) \
	__declspec(deprecated(msg))

#else
#define CG_DEPRECATED_MSG(msg) \
	[[DEPRECATED(msg)]]

#endif

#define CG_DEPRECATED CG_DEPRECATED_MSG("")