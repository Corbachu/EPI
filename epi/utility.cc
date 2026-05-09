//----------------------------------------------------------------------------
//  EDGE Platform Interface Utility
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2026  The EDGE Team.
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
#include "utility.h"

#include <cstring>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>

#define STRBOX_ALIGN(x) x = ((x + 4) & ~3)

namespace epi
{

//----------------------------------------------------------------------------
// strbox_c
//----------------------------------------------------------------------------

strbox_c::strbox_c()
{
	strs = NULL;
	numstrs = 0;
	data = NULL;
	datasize = 0;
}

strbox_c::strbox_c(strbox_c &rhs)
{
	Copy(rhs);
}

strbox_c::~strbox_c()
{
	Clear();
}

void strbox_c::Clear()
{
	if (strs)
		delete [] strs;
		
	if (data)
		delete [] data;
	
	strs = NULL;
	numstrs = 0;
	data = NULL;
	datasize = 0;
}

void strbox_c::Copy(strbox_c &src)
{
	Clear();
	
	data = new char[src.datasize];
	strs = new char*[src.numstrs];
	
	datasize = src.datasize;
	numstrs = src.numstrs;
	
	memcpy(data, src.data, sizeof(char)*datasize);
	memcpy(strs, src.strs, sizeof(char*)*numstrs);
	
	ptrdiff_t offset = data - src.data;
	for (int i=0; i<numstrs; i++)
	{
		if (strs[i])
			strs[i] += offset;
	}
}

void strbox_c::Set(strlist_c &src)
{
	array_iterator_c it;
	char *s;
	int pos, datapos;
	
	Clear();
	
	// Calculate space required
	for (datapos = 0, it = src.GetBaseIterator(); it.IsValid(); it++)
	{
		s = ITERATOR_TO_TYPE(it, char*);
		if (s)
		{
			datapos = datapos + (int)strlen(s) + 1;
			STRBOX_ALIGN(datapos);
		}
	}
	
	datasize = datapos;
	data = new char[datapos];
	memset(data, 0, datasize*sizeof(char));
	
	numstrs = src.GetSize();
	strs = new char*[numstrs];
	
	for (pos = 0, datapos = 0, it = src.GetBaseIterator(); 
		 it.IsValid(); it++)
	{
		s = ITERATOR_TO_TYPE(it, char*);
		if (s)
		{
			strs[pos] = &data[datapos];
			strcpy(strs[pos], s);
			
			datapos = datapos + (int)strlen(s) + 1;
			STRBOX_ALIGN(datapos);
		}
		else
		{
			strs[pos] = NULL;
		}
		
		pos++;
	}
}

strbox_c& strbox_c::operator=(strbox_c &rhs)
{
	if (&rhs != this)
		Copy(rhs);
		
	return *this;
}

//----------------------------------------------------------------------------
// strent_c
//----------------------------------------------------------------------------

void strent_c::Set(const char *s)
{
	clear();
	if (s)
	{
		data = new char[strlen(s)+1];
		strcpy(data, s);
	}
}

void strent_c::Set(const char *s, int max)
{
	clear();
	if (s)
	{
		const char *s2;
		int len;
		
		len = 0;
		s2 = s;
		while(*s2 && len<max)
		{
			len++;
			s2++;
		}
		
		data = new char[len+1];
		strncpy(data, s, len);
		data[len] ='\0';
	}
}

//----------------------------------------------------------------------------
// strlist_c
//----------------------------------------------------------------------------

strlist_c::strlist_c() : array_c(sizeof(char*))
{
}

strlist_c::strlist_c(strlist_c &rhs) : array_c(rhs.array_objsize)
{
	Copy(rhs);
}

strlist_c::~strlist_c()
{
	Clear();
}

void strlist_c::CleanupObject(void *obj)
{
	char *s = *(char**)obj;
	if (s)
		delete [] s;
}

void strlist_c::Copy(strlist_c &src)
{
	Size(src.array_entries);
	
	array_iterator_c it;
	for (it = src.GetBaseIterator(); it.IsValid(); it++)
		Insert(ITERATOR_TO_TYPE(it, char*));

	Trim();
}

int strlist_c::Find(const char* refname)
{
	array_iterator_c it;
	char *s;
	
	for (it = GetBaseIterator(); it.IsValid(); it++)
	{
		s = ITERATOR_TO_TYPE(it, char*);
		if (s && strcmp(s, refname) == 0)
			return it.GetPos();
	}
	
	return -1;
}

int strlist_c::FindNoCase(const char* refname)
{
	array_iterator_c it;
	char *s;

	for (it = GetBaseIterator(); it.IsValid(); it++)
	{
		s = ITERATOR_TO_TYPE(it, char*);
		if (s && STR_CaseCmp(s, refname) == 0)
			return it.GetPos();
	}

	return -1;
}

int strlist_c::Insert(const char *s)
{
	char *copy_of_s;
	if (s)
	{
		copy_of_s = new char[strlen(s)+1];
		strcpy(copy_of_s, s);
	}
	else
	{
		copy_of_s = NULL;
	}
	
	return InsertObject((void*)&copy_of_s);
}

void strlist_c::Set(strbox_c &src)
{
	Clear();
	
	if (!src.GetSize())
		return;

	Size(src.GetSize());
	
	for (int i=0; i<src.GetSize(); i++)
		Insert(src[i]);
}

strlist_c& strlist_c::operator=(strlist_c& rhs)
{
	if (&rhs != this)
	{
		Clear();
		Copy(rhs);
	}

	return *this;
}

//----------------------------------------------------------------------------
// String utility functions (epi namespace)
//----------------------------------------------------------------------------

int STR_CaseCmp(const char *A, const char *B)
{
#if defined(_WIN32) || defined(_WIN64)
	return _stricmp(A, B);
#else
	return strcasecmp(A, B);
#endif
}

int STR_CaseCmpN(const char *A, const char *B, int n)
{
#if defined(_WIN32) || defined(_WIN64)
	return _strnicmp(A, B, (size_t)n);
#else
	return strncasecmp(A, B, (size_t)n);
#endif
}

int STR_CaseCmpPartial(const char *A, const char *B)
{
	// Returns 0 when B is a prefix of A (case-insensitive).
	// A may be longer than B; the function only checks len(B) characters.
	for (; *B; A++, B++)
	{
		if (toupper((unsigned char)*A) != toupper((unsigned char)*B))
			return (toupper((unsigned char)*A) - toupper((unsigned char)*B));
	}
	return 0;
}

std::string STR_ToUpper(const std::string &s)
{
	std::string out(s);
	for (char &c : out)
		c = (char)toupper((unsigned char)c);
	return out;
}

std::string STR_ToLower(const std::string &s)
{
	std::string out(s);
	for (char &c : out)
		c = (char)tolower((unsigned char)c);
	return out;
}

std::string STR_TrimLeft(const std::string &s)
{
	size_t start = 0;
	while (start < s.size() && isspace((unsigned char)s[start]))
		start++;
	return s.substr(start);
}

std::string STR_TrimRight(const std::string &s)
{
	size_t end = s.size();
	while (end > 0 && isspace((unsigned char)s[end - 1]))
		end--;
	return s.substr(0, end);
}

std::string STR_Trim(const std::string &s)
{
	return STR_TrimLeft(STR_TrimRight(s));
}

bool STR_StartsWith(const std::string &s, const std::string &prefix)
{
	return s.size() >= prefix.size() &&
	       s.compare(0, prefix.size(), prefix) == 0;
}

bool STR_EndsWith(const std::string &s, const std::string &suffix)
{
	return s.size() >= suffix.size() &&
	       s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool STR_StartsWithNoCase(const std::string &s, const std::string &prefix)
{
	if (s.size() < prefix.size())
		return false;
	return STR_CaseCmpN(s.c_str(), prefix.c_str(), (int)prefix.size()) == 0;
}

bool STR_EndsWithNoCase(const std::string &s, const std::string &suffix)
{
	if (s.size() < suffix.size())
		return false;
	return STR_CaseCmpN(s.c_str() + s.size() - suffix.size(),
	                    suffix.c_str(), (int)suffix.size()) == 0;
}

std::vector<std::string> STR_Split(const std::string &s, char delim)
{
	std::vector<std::string> parts;
	size_t start = 0;
	for (size_t i = 0; i <= s.size(); i++)
	{
		if (i == s.size() || s[i] == delim)
		{
			parts.push_back(s.substr(start, i - start));
			start = i + 1;
		}
	}
	return parts;
}

std::vector<std::string> STR_SplitStr(const std::string &s, const std::string &delim)
{
	std::vector<std::string> parts;
	if (delim.empty())
	{
		parts.push_back(s);
		return parts;
	}
	size_t start = 0;
	size_t pos;
	while ((pos = s.find(delim, start)) != std::string::npos)
	{
		parts.push_back(s.substr(start, pos - start));
		start = pos + delim.size();
	}
	parts.push_back(s.substr(start));
	return parts;
}

std::string STR_Join(const std::vector<std::string> &parts, const std::string &sep)
{
	std::string out;
	for (size_t i = 0; i < parts.size(); i++)
	{
		if (i > 0) out += sep;
		out += parts[i];
	}
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

bool STR_Contains(const std::string &s, const std::string &needle)
{
	return s.find(needle) != std::string::npos;
}

//----------------------------------------------------------------------------
// Hash utilities (moved from utility_misc.cc)
//----------------------------------------------------------------------------

/* Thomas Wang's 32-bit Mix function */
u32_t STR_IntHash(u32_t key)
{
	key += ~(key << 15);
	key ^=  (key >> 10);
	key +=  (key << 3);
	key ^=  (key >> 6);
	key += ~(key << 11);
	key ^=  (key >> 16);

	return key;
}

u32_t STR_StringHash(const char *str)
{
	u32_t hash = 0;

	if (str)
		while (*str)
			hash = (hash << 5) - hash + (u32_t)(unsigned char)*str++;

	return hash;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
