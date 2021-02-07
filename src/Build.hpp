#pragma once

#include <unordered_map>
#include <string>
#include <vector>

#include "FileTypes.hpp"

extern const char* compilerObjectExtension;
extern const char* linkerDynamicLibraryPrefix;
extern const char* linkerDynamicLibraryExtension;
extern const char* defaultExecutableName;

void makeIncludeArgument(char* buffer, int bufferSize, const char* searchDir);

// On Windows, extra formatting is required to output objects
void makeObjectOutputArgument(char* buffer, int bufferSize, const char* objectName);
void makeDynamicLibraryOutputArgument(char* buffer, int bufferSize, const char* libraryName,
                                      const char* buildExecutable);
void makeExecutableOutputArgument(char* buffer, int bufferSize, const char* executableName,
                                  const char* linkExecutable);
void makeLinkLibraryArgument(char* buffer, int bufferSize, const char* libraryName,
                             const char* linkExecutable);
void makeLinkLibrarySearchDirArgument(char* buffer, int bufferSize, const char* searchDir,
                                      const char* linkExecutable);
void makeLinkLibraryRuntimeSearchDirArgument(char* buffer, int bufferSize, const char* searchDir,
                                             const char* linkExecutable);
void makeLinkerArgument(char* buffer, int bufferSize, const char* argument,
                        const char* linkExecutable);

// On Windows, extra work is done to find the compiler and linker executables. This function handles
// looking up those environment variables to determine which executable to use
bool resolveExecutablePath(const char* fileToExecute, char* resolvedPathOut,
                           int resolvedPathOutSize);

typedef std::unordered_map<std::string, FileModifyTime> HeaderModificationTimeTable;

// If an existing cached build was run, check the current build's commands against the previous
// commands via CRC comparison. This ensures changing commands will cause rebuilds
typedef std::unordered_map<std::string, uint32_t> ArtifactCrcTable;
typedef std::pair<const std::string, uint32_t> ArtifactCrcTablePair;

void buildWriteCacheFile(const char* buildOutputDir, ArtifactCrcTable& cachedCommandCrcs,
                         ArtifactCrcTable& newCommandCrcs);

// Returns false if there were errors; the file not existing is not an error
bool buildReadCacheFile(const char* buildOutputDir, ArtifactCrcTable& cachedCommandCrcs);

// commandArguments should have terminating null sentinel
bool commandEqualsCachedCommand(ArtifactCrcTable& cachedCommandCrcs, const char* artifactKey,
                                const char** commandArguments, uint32_t* crcOut);

struct EvaluatorEnvironment;

// Check command, headers, and cache for whether the artifact is still valid
bool cppFileNeedsBuild(EvaluatorEnvironment& environment, const char* sourceFilename,
                       const char* artifactFilename, const char** commandArguments,
                       ArtifactCrcTable& cachedCommandCrcs, ArtifactCrcTable& newCommandCrcs,
                       HeaderModificationTimeTable& headerModifiedCache,
                       std::vector<std::string>& headerSearchDirectories);
