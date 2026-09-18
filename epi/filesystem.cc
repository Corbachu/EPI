//----------------------------------------------------------------------------
//  EDGE Filesystem Class
//----------------------------------------------------------------------------
//
//  Copyright (c) 2003-2008  The EDGE Team.
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

#include "file.h"
#include "filesystem.h"

#ifdef DREAMCAST
extern "C" {
#include <kos/dbgio.h>
}

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#endif

#define MAX_MODE_CHARS  32
#define COPY_BUF_SIZE   1024

namespace epi
{

#ifdef DREAMCAST
static bool FS_MatchesMask(const char *name, const char *mask)
{
	SYS_ASSERT(name);
	SYS_ASSERT(mask);

	if (strcmp(mask, "*") == 0 || strcmp(mask, "*.*") == 0 || strcmp(mask, ".*") == 0)
		return true;

	if (strncmp(mask, "*.", 2) == 0)
	{
		const char *dot = strrchr(name, '.');
		if (!dot)
			return false;

		return strcasecmp(dot, mask + 1) == 0;
	}

	return strcasecmp(name, mask) == 0;
}
#endif

// A Filesystem Directory

filesystem_dir_c::filesystem_dir_c() : array_c(sizeof(filesys_direntry_c))
{ }

filesystem_dir_c::~filesystem_dir_c()
{ }

bool filesystem_dir_c::AddEntry(filesys_direntry_c *fs_entry)
{
	if (InsertObject(fs_entry) < 0)
        return false;

	return true;
}

void filesystem_dir_c::CleanupObject(void *obj)
{ }

filesys_direntry_c *filesystem_dir_c::operator[](int idx)
{
	return (filesys_direntry_c*)FetchObject(idx);
}


//----------------------------------------------------------------------------

// common functions

bool FS_Access(const char *name, unsigned int flags)
{
	SYS_ASSERT(name);

    char mode[MAX_MODE_CHARS];

    if (! FS_FlagsToAnsiMode(flags, mode))
        return false;

    #ifndef DREAMCAST
        FILE *fp = fopen(name, mode);
        if (!fp)
            return false;
        fclose(fp);
        return true;
    #else
        (void)mode;
        file_t fp = fs_open(name, O_RDONLY);
        if (fp < 0)
        {
            dbgio_printf("[FS_Access] fs_open failed: %s\n", name);
            return false;
        }
        fs_close(fp);
        return true;
    #endif
}

file_c* FS_Open(const char *name, unsigned int flags)
{
	SYS_ASSERT(name);

    char mode[MAX_MODE_CHARS];

    if (! FS_FlagsToAnsiMode(flags, mode))
        return NULL;

    #ifndef DREAMCAST
        FILE *fp = fopen(name, mode);
        if (!fp)
            return NULL;
    #else
        (void)mode;
		int open_flags = 0;

		if (flags & file_c::ACCESS_READ)
		{
			if (flags & file_c::ACCESS_WRITE)
				open_flags = O_RDWR | O_CREAT | O_TRUNC;
			else if (flags & file_c::ACCESS_APPEND)
				open_flags = O_RDWR | O_APPEND | O_CREAT;
			else
				open_flags = O_RDONLY;
		}
		else
		{
			if (flags & file_c::ACCESS_WRITE)
				open_flags = O_WRONLY | O_CREAT | O_TRUNC;
			else if (flags & file_c::ACCESS_APPEND)
				open_flags = O_WRONLY | O_APPEND | O_CREAT;
			else
				return NULL;
		}

		file_t fh = fs_open(name, open_flags);
        if (fh < 0)
        {
            dbgio_printf("[FS_Open] fs_open failed: %s\n", name);
            return NULL;
        }
        FILE *fp = (FILE *)(fh + 1);
    #endif

	return new ansi_file_c(fp);
}

std::filesystem::path FS_GetCurrDir()
{
	#ifdef DREAMCAST
		char buffer[PATH_MAX];

		if (!getcwd(buffer, sizeof(buffer)))
			return std::filesystem::path();

		return std::filesystem::path(buffer);
	#else
	return std::filesystem::current_path();
	#endif
}

bool FS_SetCurrDir(std::filesystem::path dir)
{
	SYS_ASSERT(!dir.empty());
	#ifdef DREAMCAST
		return (chdir(dir.c_str()) == 0);
	#else
	try
	{
		std::filesystem::current_path(dir);
	}
	catch (std::filesystem::filesystem_error const& ex)
	{
		I_Warning("Failed to set current directory! Error: %s\n", ex.what());
		return false;
	}
	return true;
	#endif
}

bool FS_IsDir(const char *dir)
{
	SYS_ASSERT(dir);
	#ifdef DREAMCAST
		DIR *result = opendir(dir);

		if (result == NULL)
			return false;

		closedir(result);
		return true;
	#else
	return std::filesystem::is_directory(dir);
	#endif
}

bool FS_MakeDir(const char *dir)
{
	SYS_ASSERT(dir);
	#ifdef DREAMCAST
		return (mkdir(dir, 0775) == 0 || errno == EEXIST);
	#else
	return std::filesystem::create_directory(dir);
	#endif
}

bool FS_RemoveDir(const char *dir)
{
	SYS_ASSERT(dir);
	#ifdef DREAMCAST
		return (rmdir(dir) == 0);
	#else
	return std::filesystem::remove(dir);
	#endif
}

bool FS_ReadDir(filesystem_dir_c *fsd, const char *dir, const char *mask)
{
	if (!dir || !fsd || !mask)
		return false;

	#ifdef DREAMCAST
		DIR *handle = opendir(dir);
		if (handle == NULL)
			return false;

		std::filesystem::path prev_dir = FS_GetCurrDir();
		if (prev_dir.empty())
		{
			closedir(handle);
			return false;
		}

		if (!FS_SetCurrDir(dir))
		{
			closedir(handle);
			return false;
		}

		fsd->Clear();

		for (;;)
		{
			struct dirent *fdata = readdir(handle);
			if (fdata == NULL)
				break;

			if (strlen(fdata->d_name) == 0)
				continue;

			if (strcmp(fdata->d_name, ".") == 0 || strcmp(fdata->d_name, "..") == 0)
				continue;

			if (!FS_MatchesMask(fdata->d_name, mask))
				continue;

			struct stat finfo;
			if (stat(fdata->d_name, &finfo) != 0)
				continue;

			filesys_direntry_c *entry = new filesys_direntry_c();

			entry->name = std::string(fdata->d_name);
			entry->is_dir = S_ISDIR(finfo.st_mode) ? true : false;
			entry->size = entry->is_dir ? 0 : finfo.st_size;

			if (!fsd->AddEntry(entry))
			{
				delete entry;
				closedir(handle);
				FS_SetCurrDir(prev_dir);
				return false;
			}
		}

		FS_SetCurrDir(prev_dir);
		closedir(handle);
		return true;
	#else
	std::filesystem::path prev_dir = FS_GetCurrDir();
	std::filesystem::path mask_ext = std::filesystem::path(mask).extension(); // Allows us to retain the *.extension syntax - Dasho

	if (prev_dir.empty())
		return false;

	if (! FS_SetCurrDir(dir))
		return false;

	// Ensure the container is empty
	fsd->Clear();

	for (auto const& dir_entry: std::filesystem::directory_iterator{std::filesystem::current_path()})
	{
		if (strcasecmp(mask_ext.string().c_str(), ".*") != 0 && strcasecmp(mask_ext.string().c_str(), dir_entry.path().extension().string().c_str()) != 0)
			continue;

		filesys_direntry_c *entry = new filesys_direntry_c();

		entry->name = dir_entry.path().filename().string();
		entry->is_dir = dir_entry.is_directory();
		entry->size = entry->is_dir ? 0 : dir_entry.file_size();

		if (! fsd->AddEntry(entry))
		{
			delete entry;
			FS_SetCurrDir(prev_dir);
			return false;
		}
	}

	FS_SetCurrDir(prev_dir);
	return true;
	#endif
}

bool FS_Copy(const char *src, const char *dest)
{
	SYS_ASSERT(src && dest);

	#ifdef DREAMCAST
		bool ok = false;
		file_c *dest_file = NULL;
		file_c *src_file = NULL;
		unsigned char *buf = NULL;
		int size;
		int pkt_len;

		src_file = FS_Open(src, file_c::ACCESS_READ);
		if (!src_file)
			goto error_occurred;

		dest_file = FS_Open(dest, file_c::ACCESS_WRITE);
		if (!dest_file)
			goto error_occurred;

		buf = new unsigned char[COPY_BUF_SIZE];
		SYS_ASSERT(buf);

		size = src_file->GetLength();

		while (size > 0)
		{
			pkt_len = MIN(size, COPY_BUF_SIZE);

			if (src_file->Read(buf, pkt_len) != (unsigned int)pkt_len)
				goto error_occurred;

			if (dest_file->Write(buf, pkt_len) != (unsigned int)pkt_len)
				goto error_occurred;

			size -= pkt_len;
		}

		ok = true;

	error_occurred:
		if (src_file)
			delete src_file;

		if (dest_file)
			delete dest_file;

		if (buf)
			delete[] buf;

		return ok;
	#else
	// Copy src to dest overwriting dest if it exists
	return std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing);
	#endif
}

bool FS_Delete(const char *name)
{
	SYS_ASSERT(name);

	#ifdef DREAMCAST
		return (unlink(name) == 0);
	#else
	return std::filesystem::remove(name);
	#endif
}

bool FS_Rename(const char *oldname, const char *newname)
{
	SYS_ASSERT(oldname);
	SYS_ASSERT(newname);
	#ifdef DREAMCAST
		return (rename(oldname, newname) != -1);
	#else
	try
	{
		std::filesystem::rename(oldname, newname);
	}
	catch (std::filesystem::filesystem_error const& ex)
	{
		I_Warning("Failed to rename file! Error: %s\n", ex.what());
		return false;
	}
	return true;
	#endif
}

bool FS_GetModifiedTime(const char *filename, timestamp_c& t)
{
	struct stat buf;
	struct tm timeinf;

	SYS_ASSERT(filename);

	if (stat(filename, &buf) != 0)
		return false;

	if (!localtime_r(&buf.st_mtime, &timeinf))
		return false;

	t.Set(timeinf.tm_mday, timeinf.tm_mon + 1, timeinf.tm_year + 1900,
	      timeinf.tm_hour, timeinf.tm_min, timeinf.tm_sec);

	return true;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
