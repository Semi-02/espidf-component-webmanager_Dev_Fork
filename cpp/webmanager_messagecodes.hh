#pragma once
#include <inttypes.h>

namespace messagecodes
{
#define DEF(thecode, thestring) thecode,
#define DEF_(thecode) thecode,
    enum class C:uint32_t
    {
#include "default_messages.inc"
#if __has_include(<messages.inc>)
#include <messages.inc>
#endif
    };
#undef DEF
#undef DEF_
#define DEF(thecode, thestring) thestring,
#define DEF_(thecode) #thecode,
    constexpr const char *N[] = {
#include "default_messages.inc"
#if __has_include(<messages.inc>)
#include <messages.inc>
#endif

    };
#undef DEF
#undef DEF_
}
