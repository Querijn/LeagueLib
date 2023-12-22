#pragma once

#include <cassert>
#if SPEK_EMSCRIPTEN

#include <emscripten.h>
#include <sstream>
#define SPEK_ASSERT(Condition, Message) do \
{ \
	if (Condition) \
		break; \
	std::ostringstream t_Stream; \
	t_Stream << Message; \
	auto t_Message = t_Stream.str(); \
	EM_ASM(console.error('SPEK_ASSERT: ' + UTF8ToString($0)); console.trace(); alert(UTF8ToString($0));, t_Message.c_str()); \
} while(0)

#else

#include <cassert>
#define SPEK_ASSERT(Condition, Message) assert(Condition)

#endif