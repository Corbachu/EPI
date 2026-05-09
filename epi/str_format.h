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

#ifndef __EPI_STR_FORMAT_H__
#define __EPI_STR_FORMAT_H__

#include <string>
#include <cstdarg>

namespace epi
{

//------------------------------------------------------------------------
// Core format functions
//------------------------------------------------------------------------

std::string STR_Format(const char *fmt, ...);
// printf-style formatting into a std::string.

std::string STR_FormatV(const char *fmt, va_list args);
// va_list overload – use when you already have a va_list (e.g. from a
// variadic wrapper function).

char * STR_FormatCStr(const char *fmt, ...);
// printf-style formatting; returns a heap-allocated C string.
// Caller MUST delete[] the result.

//------------------------------------------------------------------------
// Higher-level string manipulation
//------------------------------------------------------------------------

std::string STR_PadLeft (const std::string &s, int width, char pad = ' ');
// Left-pad 's' with 'pad' characters until its length is at least 'width'.
// Returns 's' unchanged when len(s) >= width.

std::string STR_PadRight(const std::string &s, int width, char pad = ' ');
// Right-pad 's' with 'pad' characters until its length is at least 'width'.

std::string STR_Repeat(const std::string &s, int count);
// Concatenate 's' with itself 'count' times.  Returns "" when count <= 0.

std::string STR_Replace(const std::string &s, const std::string &from, const std::string &to);
// Replace all non-overlapping occurrences of 'from' with 'to'.

std::string STR_EscapeJSON(const std::string &s);
// Escape a string for embedding inside a JSON string literal:
// backslash, double-quote, control characters.

std::string STR_UnescapeJSON(const std::string &s);
// Reverse of STR_EscapeJSON – process \n, \t, \\, \", \uXXXX sequences.

std::string STR_NumberToHex(unsigned long long value, int min_digits = 1);
// Format an integer as a zero-padded lowercase hex string.

bool STR_ParseBool(const std::string &s, bool default_val = false);
// Accept "true"/"false", "yes"/"no", "on"/"off", "1"/"0" (case-insensitive).
// Returns default_val for unrecognised input.

} // namespace epi

#endif /* __EPI_STR_FORMAT_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
