#pragma once
#include "Log.hpp"

#define GASSERT( condition ) if (!(condition)) { GPrint(G_FILELINE("FALSE\n")); G_DEBUG_BREAK }
#if defined(_MSC_VER)
	#define GASSERTM( condition, message, ... ) if (!(condition)) { GPrint(G_FILELINE(G_CONCAT(message, "\n")), __VA_ARGS__); G_DEBUG_BREAK }
#else
	#define GASSERTM( condition, message, ... ) if (!(condition)) { GPrint(G_FILELINE(G_CONCAT(message, "\n")), ## __VA_ARGS__); G_DEBUG_BREAK }
#endif

