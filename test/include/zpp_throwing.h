// A hack to make the promise type declare operator new, to detect lack of halo.
#define unhandled_exception * operator new(std::size_t); void unhandled_exception
#include "../../zpp_throwing.h"
#undef unhandled_exception
