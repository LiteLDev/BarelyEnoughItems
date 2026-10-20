// LeviLamina refuses to load mods that don't route new/delete through its allocator.
// This must be the only translation unit that includes MemoryOperators.h.
#ifndef LL_MEMORY_OPERATORS
#define LL_MEMORY_OPERATORS
#endif
#include "ll/api/memory/MemoryOperators.h" // IWYU pragma: keep
