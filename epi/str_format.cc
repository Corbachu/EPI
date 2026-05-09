//----------------------------------------------------------------------------
//  EPI String Formatting
//----------------------------------------------------------------------------
//
//  Copyright (c) 2007-2026  The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#include "epi.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cstdint>
#include <climits>

#include "str_format.h"

// Larger initial buffer reduces memory reallocations for typical formatted strings
#define STARTING_LENGTH  128

namespace epi
{

//------------------------------------------------------------------------
// Core format functions
//------------------------------------------------------------------------

char * STR_FormatCStr(const char *fmt, ...)
{
	/* Algorithm: keep doubling the allocated buffer size
	 * until the output fits. Based on code by Darren Salt.
	 */
	int buf_size = STARTING_LENGTH;

	for (;;)
	{
		char *buf = new char[buf_size];

		va_list args;

		va_start(args, fmt);
		int out_len = vsnprintf(buf, buf_size, fmt, args);
		va_end(args);

		// old versions of vsnprintf() simply return -1 when
		// the output doesn't fit.
		if (out_len >= 0 && out_len < buf_size)
			return buf;

		delete[] buf;

		buf_size *= 2;
	}
}

std::string STR_FormatV(const char *fmt, va_list args)
{
	int buf_size = STARTING_LENGTH;

	for (;;)
	{
		char *buf = new char[buf_size];

		// We must copy args because vsnprintf consumes them.
		va_list args_copy;
		va_copy(args_copy, args);
		int out_len = vsnprintf(buf, buf_size, fmt, args_copy);
		va_end(args_copy);

		if (out_len >= 0 && out_len < buf_size)
		{
			std::string result(buf);
			delete[] buf;
			return result;
		}

		delete[] buf;
		buf_size *= 2;
	}
}

std::string STR_Format(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	std::string result = STR_FormatV(fmt, args);
	va_end(args);
	return result;
}

//------------------------------------------------------------------------
// Higher-level string manipulation
//------------------------------------------------------------------------

std::string STR_PadLeft(const std::string &s, int width, char pad)
{
	if ((int)s.size() >= width)
		return s;
	return std::string((size_t)(width - (int)s.size()), pad) + s;
}

std::string STR_PadRight(const std::string &s, int width, char pad)
{
	if ((int)s.size() >= width)
		return s;
	return s + std::string((size_t)(width - (int)s.size()), pad);
}

std::string STR_Repeat(const std::string &s, int count)
{
	std::string out;
	out.reserve(s.size() * (size_t)(count > 0 ? count : 0));
	for (int i = 0; i < count; i++)
		out += s;
	return out;
}

std::string STR_Replace(const std::string &s, const std::string &from, const std::string &to)
{
	if (from.empty())
		return s;

	std::string out;
	size_t start = 0;
	size_t pos;
	while ((pos = s.find(from, start)) != std::string::npos)
	{
		out += s.substr(start, pos - start);
		out += to;
		start = pos + from.size();
	}
	out += s.substr(start);
	return out;
}

std::string STR_EscapeJSON(const std::string &s)
{
	std::string out;
	out.reserve(s.size() + 8);

	for (unsigned char c : s)
	{
		switch (c)
		{
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n";  break;
			case '\r': out += "\\r";  break;
			case '\t': out += "\\t";  break;
			case '\b': out += "\\b";  break;
			case '\f': out += "\\f";  break;
			default:
				if (c < 0x20u)
				{
					// control character – use \uXXXX
					char buf[8];
					snprintf(buf, sizeof(buf), "\\u%04x", (unsigned int)c);
					out += buf;
				}
				else
				{
					out += (char)c;
				}
				break;
		}
	}
	return out;
}

std::string STR_UnescapeJSON(const std::string &s)
{
	std::string out;
	out.reserve(s.size());

	for (size_t i = 0; i < s.size(); i++)
	{
		if (s[i] != '\\' || i + 1 >= s.size())
		{
			out += s[i];
			continue;
		}

		char next = s[++i];
		switch (next)
		{
			case '"':  out += '"';  break;
			case '\\': out += '\\'; break;
			case '/':  out += '/';  break;
			case 'n':  out += '\n'; break;
			case 'r':  out += '\r'; break;
			case 't':  out += '\t'; break;
			case 'b':  out += '\b'; break;
			case 'f':  out += '\f'; break;
			case 'u':
			{
				// \uXXXX – consume 4 hex digits
				if (i + 4 >= s.size())
					break;
				char hex[5] = { s[i+1], s[i+2], s[i+3], s[i+4], 0 };
				i += 4;
				unsigned long codepoint = strtoul(hex, nullptr, 16);
				if (codepoint < 0x80)
				{
					out += (char)(unsigned char)codepoint;
				}
				else if (codepoint < 0x800)
				{
					out += (char)(unsigned char)(0xC0 | (codepoint >> 6));
					out += (char)(unsigned char)(0x80 | (codepoint & 0x3F));
				}
				else
				{
					out += (char)(unsigned char)(0xE0 | (codepoint >> 12));
					out += (char)(unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
					out += (char)(unsigned char)(0x80 | (codepoint & 0x3F));
				}
				break;
			}
			default:
				out += next;
				break;
		}
	}
	return out;
}

std::string STR_NumberToHex(unsigned long long value, int min_digits)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%0*llx", min_digits, value);
	return buf;
}

bool STR_ParseBool(const std::string &s, bool default_val)
{
	// Lower-case the input for comparison
	std::string lc;
	lc.reserve(s.size());
	for (char c : s)
		lc += (char)tolower((unsigned char)c);

	if (lc == "true"  || lc == "yes" || lc == "on"  || lc == "1") return true;
	if (lc == "false" || lc == "no"  || lc == "off" || lc == "0") return false;

	return default_val;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
