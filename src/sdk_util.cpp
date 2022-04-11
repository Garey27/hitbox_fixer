#include "precompiled.h"

int UTIL_ReadFlags(const char* c)
{
	int flags = 0;
	
	while (*c)
		flags |= (1<<(*c++ - 'a'));
	
	return flags;
}

void UTIL_LogPrintf(const char *fmt, ...)
{
	va_list argptr;
	static char string[1024];

	va_start(argptr, fmt);
	vsprintf(string, fmt, argptr);
	va_end(argptr);

	ALERT(at_logged, "%s", string);
}


short FixedSigned16(float value, float scale)
{
	return (short)clamp(int(value * scale), SHRT_MIN, SHRT_MAX);
}

unsigned short FixedUnsigned16(float value, float scale)
{
	return (unsigned short)clamp(int(value * scale), 0, USHRT_MAX);
}
