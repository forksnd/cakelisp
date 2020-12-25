#include "FileUtilities.hpp"

#include <stdio.h>
#include <string.h>

#include "Logging.hpp"
#include "Utilities.hpp"

#ifdef UNIX
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#elif WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#else
#error Need to implement file utilities for this platform
#endif

unsigned long fileGetLastModificationTime(const char* filename)
{
#ifdef UNIX
	struct stat fileStat;
	if (stat(filename, &fileStat) == -1)
	{
		if (logging.fileSystem || errno != ENOENT)
			perror("fileGetLastModificationTime: ");
		return 0;
	}

	return (unsigned long)fileStat.st_mtime;
#elif WINDOWS
	// Doesn't actually create new due to OPEN_EXISTING
	HANDLE hFile =
	    CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, /*lpSecurityAttributes=*/nullptr,
	               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, /*hTemplateFile=*/nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return 0;

	FILETIME ftCreate, ftAccess, ftWrite;
	if (!GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite))
		return 0;

	ULARGE_INTEGER lv_Large;
	lv_Large.LowPart = ftWrite.dwLowDateTime;
	lv_Large.HighPart = ftWrite.dwHighDateTime;

	__int64 ftWriteTime = lv_Large.QuadPart;
	if (ftWriteTime < 0)
		return 0;
	return (unsigned long)ftWriteTime;
#else
	return 0;
#endif
}

bool fileIsMoreRecentlyModified(const char* filename, const char* reference)
{
#ifdef UNIX
	struct stat fileStat;
	struct stat referenceStat;
	if (stat(filename, &fileStat) == -1)
	{
		if (logging.fileSystem || errno != ENOENT)
			perror("fileIsMoreRecentlyModified: ");
		return true;
	}
	if (stat(reference, &referenceStat) == -1)
	{
		if (logging.fileSystem || errno != ENOENT)
			perror("fileIsMoreRecentlyModified: ");
		return true;
	}

	// Logf("%s vs %s: %lu %lu\n", filename, reference, fileStat.st_mtime, referenceStat.st_mtime);

	return fileStat.st_mtime > referenceStat.st_mtime;
#elif WINDOWS
	// Doesn't actually create new due to OPEN_EXISTING
	HANDLE hFile =
	    CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, /*lpSecurityAttributes=*/nullptr,
	               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, /*hTemplateFile=*/nullptr);
	HANDLE hReference =
	    CreateFile(reference, GENERIC_READ, FILE_SHARE_READ, /*lpSecurityAttributes=*/nullptr,
	               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, /*hTemplateFile=*/nullptr);
	if (hFile == INVALID_HANDLE_VALUE || hReference == INVALID_HANDLE_VALUE)
		return 0;
	FILETIME ftCreate, ftAccess, ftWrite;
	if (!GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite))
		return true;
	FILETIME ftCreateRef, ftAccessRef, ftWriteRef;
	if (!GetFileTime(hFile, &ftCreateRef, &ftAccessRef, &ftWriteRef))
		return true;

	return CompareFileTime(&ftWrite, &ftWriteRef) >= 1;

	return true;
#else
	// Err on the side of filename always being newer than the reference. The #error in the includes
	// block should prevent this from ever being compiled anyways
	return true;
#endif
}

bool fileExists(const char* filename)
{
#ifdef UNIX
	return access(filename, F_OK) != -1;
#elif WINDOWS
	return GetFileAttributes(filename) != INVALID_FILE_ATTRIBUTES;
#else
	return false;
#endif
}

void makeDirectory(const char* path)
{
#ifdef UNIX
	if (mkdir(path, 0755) == -1)
	{
		// We don't care about EEXIST, we just want the dir
		if (logging.fileSystem || errno != EEXIST)
			perror("makeDirectory: ");
	}
#elif WINDOWS
	if (!CreateDirectory(path, /*lpSecurityAttributes=*/nullptr))
	{
		if (GetLastError() != ERROR_ALREADY_EXISTS)
			Logf("makeDirectory failed to make %s", path);
	}
#else
#error Need to be able to make directories on this platform
#endif
}

void getDirectoryFromPath(const char* path, char* bufferOut, int bufferSize)
{
#ifdef UNIX
	char* pathCopy = strdup(path);
	const char* dirName = dirname(pathCopy);
	SafeSnprinf(bufferOut, bufferSize, "%s", dirName);
	free(pathCopy);
#elif WINDOWS
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	// char fname[_MAX_FNAME];
	// char ext[_MAX_EXT];
	_splitpath_s(path, drive, sizeof(drive), dir, sizeof(dir),
	             /*fname=*/nullptr, 0,
	             /*ext=*/nullptr, 0);
	_makepath_s(bufferOut, bufferSize, drive, dir, /*fname=*/nullptr,
	            /*ext=*/nullptr);
#else
#error Need to be able to strip file from path to get directory on this platform
#endif
}

void getFilenameFromPath(const char* path, char* bufferOut, int bufferSize)
{
#ifdef UNIX
	char* pathCopy = strdup(path);
	const char* fileName = basename(pathCopy);
	SafeSnprinf(bufferOut, bufferSize, "%s", fileName);
	free(pathCopy);
#elif WINDOWS
	// char drive[_MAX_DRIVE];
	// char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];
	_splitpath_s(path, /*drive=*/nullptr, 0, /*dir=*/nullptr, 0, fname, sizeof(fname), ext,
	             sizeof(ext));
	_makepath_s(bufferOut, bufferSize, /*drive=*/nullptr, /*dir=*/nullptr, fname, ext);
#else
#error Need to be able to strip path to get filename on this platform
#endif
}

void makePathRelativeToFile(const char* filePath, const char* referencedFilePath, char* bufferOut,
                            int bufferSize)
{
	getDirectoryFromPath(filePath, bufferOut, bufferSize);
	// TODO: Need to make this safe!
	strcat(bufferOut, "/");
	strcat(bufferOut, referencedFilePath);
}

const char* makeAbsolutePath_Allocated(const char* fromDirectory, const char* filePath)
{
#ifdef UNIX
	// Second condition allows for absolute paths
	if (fromDirectory && filePath[0] != '/')
	{
		char relativePath[MAX_PATH_LENGTH] = {0};
		PrintfBuffer(relativePath, "%s/%s", fromDirectory, filePath);
		return realpath(relativePath, nullptr);
	}
	else
	{
		// The path will be relative to the binary's working directory
		return realpath(filePath, nullptr);
	}
#elif WINDOWS
	char* absolutePath = (char*)calloc(MAX_PATH_LENGTH, sizeof(char));
	bool isValid = false;
	if (fromDirectory)
	{
		char relativePath[MAX_PATH_LENGTH] = {0};
		PrintfBuffer(relativePath, "%s/%s", fromDirectory, filePath);
		isValid = _fullpath(absolutePath, relativePath, MAX_PATH_LENGTH);
	}
	else
	{
		isValid = _fullpath(absolutePath, filePath, MAX_PATH_LENGTH);
	}

	if (!isValid)
	{
		free(absolutePath);
		return nullptr;
	}
	return absolutePath;
#else
#error Need to be able to normalize path on this platform
	return nullptr;
#endif
}

void makeAbsoluteOrRelativeToWorkingDir(const char* filePath, char* bufferOut, int bufferSize)
{
#ifdef UNIX
	// If it's already absolute, keep it that way
	// Accept a lone . as well, for current working directory
	if (filePath[0] == '/' || (filePath[0] == '.' && filePath[1] == '\0') ||
	    (filePath[0] == '.' && filePath[1] == '/' && filePath[2] == '\0'))
	{
		SafeSnprinf(bufferOut, bufferSize, "%s", filePath);
		return;
	}

	const char* workingDirAbsolute = realpath(".", nullptr);
	if (!workingDirAbsolute)
	{
		SafeSnprinf(bufferOut, bufferSize, "%s", filePath);
		return;
	}

	const char* filePathAbsolute = realpath(filePath, nullptr);
	if (!filePathAbsolute)
	{
		free((void*)workingDirAbsolute);
		SafeSnprinf(bufferOut, bufferSize, "%s", filePath);
		return;
	}

	// Logf("workingDirAbsolute %s\nfilePathAbsolute %s\n", workingDirAbsolute, filePathAbsolute);

	size_t workingDirPathLength = strlen(workingDirAbsolute);
	if (strncmp(workingDirAbsolute, filePathAbsolute, workingDirPathLength) == 0)
	{
		// The resolved path is within working dir
		int trimTrailingSlash = filePathAbsolute[workingDirPathLength] == '/' ? 1 : 0;
		const char* startRelativePath = &filePathAbsolute[workingDirPathLength + trimTrailingSlash];
		SafeSnprinf(bufferOut, bufferSize, "%s", startRelativePath);
	}
	else
	{
		// Resolved path is above working dir
		// Could still make this relative with ../ up to differing directory, if I find it's desired
		SafeSnprinf(bufferOut, bufferSize, "%s", filePathAbsolute);
	}

	free((void*)workingDirAbsolute);
	free((void*)filePathAbsolute);
#elif WINDOWS
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];
	_splitpath_s(filePath, drive, sizeof(drive), dir, sizeof(dir), fname, sizeof(fname), ext,
	             sizeof(ext));
	// If it's already absolute, keep it that way
	if (drive[0])
	{
		_makepath_s(bufferOut, bufferSize, drive, dir, fname, ext);
	}
	else
	{
		char workingDirAbsolute[MAX_PATH_LENGTH] = {0};
		GetCurrentDirectory(sizeof(workingDirAbsolute), workingDirAbsolute);
		const char* filePathAbsolute = makeAbsolutePath_Allocated(nullptr, filePath);
		if (!filePathAbsolute)
		{
			SafeSnprinf(bufferOut, bufferSize, "%s", filePath);
			return;
		}

		// Within the same directory?
		size_t workingDirPathLength = strlen(workingDirAbsolute);
		if (strncmp(workingDirAbsolute, filePathAbsolute, workingDirPathLength) == 0)
		{
			// The resolved path is within working dir
			int trimTrailingSlash = filePathAbsolute[workingDirPathLength] == '/' ||
			                                filePathAbsolute[workingDirPathLength] == '\\' ?
			                            1 :
			                            0;
			const char* startRelativePath =
			    &filePathAbsolute[workingDirPathLength + trimTrailingSlash];
			SafeSnprinf(bufferOut, bufferSize, "%s", startRelativePath);
		}
		else
		{
			// Resolved path is above working dir. Could still make this relative with ../ up to
			// differing directory, if I find it's desired
			SafeSnprinf(bufferOut, bufferSize, "%s", filePathAbsolute);
		}
		free((void*)filePathAbsolute);
	}
#else
#error Need to be able to normalize path on this platform
#endif

	// // Test example
	// {
	// 	const char* testCases[] = {"../gamelib/RunProfiler.sh",
	// 	                           "runtime/HotReloading.cake",
	// 	                           "runtime/../src/Evaluator.hpp",
	// 	                           "src/../badfile",
	// 	                           "ReadMe.org",
	// 							   ".", "./",
	// 	                           "/home/macoy/delme.txt"};
	// 	for (int i = 0; i < ArraySize(testCases); ++i)
	// 	{
	// 		char resultBuffer[MAX_PATH_LENGTH] = {0};
	// 		makeAbsoluteOrRelativeToWorkingDir(testCases[i], resultBuffer, ArraySize(resultBuffer));
	// 		Logf("%s = %s\n\n", testCases[i], resultBuffer);
	// 	}
	// 	return 0;
	// }
}

bool outputFilenameFromSourceFilename(const char* outputDir, const char* sourceFilename,
                                      const char* addExtension, char* bufferOut, int bufferSize)
{
	char buildFilename[MAX_NAME_LENGTH] = {0};
	getFilenameFromPath(sourceFilename, buildFilename, sizeof(buildFilename));
	if (!buildFilename[0])
		return false;

	// TODO: Trim .cake.cpp (etc.)
	if (!addExtension)
	{
		SafeSnprinf(bufferOut, bufferSize, "%s/%s", outputDir, buildFilename);
	}
	else
	{
		SafeSnprinf(bufferOut, bufferSize, "%s/%s.%s", outputDir, buildFilename, addExtension);
	}
	return true;
}

bool copyBinaryFileTo(const char* srcFilename, const char* destFilename)
{
	// Note: man 3 fopen says "b" is unnecessary on Linux, but I'll keep it anyways
	FILE* srcFile = fopen(srcFilename, "rb");
	FILE* destFile = fopen(destFilename, "wb");
	if (!srcFile || !destFile)
	{
		perror("fopen: ");
		Logf("error: failed to copy %s to %s\n", srcFilename, destFilename);
		return false;
	}

	char buffer[4096];
	size_t totalCopied = 0;
	size_t numRead = fread(buffer, sizeof(buffer[0]), ArraySize(buffer), srcFile);
	while (numRead)
	{
		fwrite(buffer, sizeof(buffer[0]), numRead, destFile);
		totalCopied += numRead;
		numRead = fread(buffer, sizeof(buffer[0]), ArraySize(buffer), srcFile);
	}

	if (logging.fileSystem)
		Logf("%lu bytes copied\n", totalCopied);

	fclose(srcFile);
	fclose(destFile);

	if (logging.fileSystem)
		Logf("Wrote %s\n", destFilename);

	return true;
}

bool copyFileTo(const char* srcFilename, const char* destFilename)
{
	FILE* srcFile = fopen(srcFilename, "r");
	FILE* destFile = fopen(destFilename, "w");
	if (!srcFile || !destFile)
	{
		perror("fopen: ");
		Logf("error: failed to copy %s to %s", srcFilename, destFilename);
		return false;
	}

	char buffer[4096];
	while (fgets(buffer, sizeof(buffer), srcFile))
		fputs(buffer, destFile);

	fclose(srcFile);
	fclose(destFile);

	if (logging.fileSystem)
		Logf("Wrote %s\n", destFilename);

	return true;
}

bool moveFile(const char* srcFilename, const char* destFilename)
{
	if (!copyFileTo(srcFilename, destFilename))
		return false;

	if (remove(srcFilename) != 0)
	{
		perror("remove: ");
		Logf("Failed to remove %s\n", srcFilename);
		return false;
	}

	return true;
}

void addExecutablePermission(const char* filename)
{
	// Not necessary on Windows
#ifdef UNIX
	// From man 2 chmod:
	// S_IRUSR  (00400)  read by owner
	//  S_IWUSR  (00200)  write by owner
	//  S_IXUSR  (00100)  execute/search by owner ("search" applies for directories, and means
	// that entries within the directory can be accessed)
	//  S_IRGRP  (00040)  read by group
	//  S_IWGRP  (00020)  write by group
	//  S_IXGRP  (00010)  execute/search by group
	//  S_IROTH  (00004)  read by others
	//  S_IWOTH  (00002)  write by others
	//  S_IXOTH  (00001)  execute/search by others

	if (chmod(filename, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1)
	{
		perror("addExecutablePermission: ");
	}
#endif
}
