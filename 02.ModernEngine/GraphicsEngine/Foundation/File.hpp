#pragma once
#include "Platform.hpp"
#include <stdio.h>

struct Allocator;
struct StringArray;


#if defined(_WIN64)

typedef struct __FILETIME
{
    unsigned long dwLowDateTime;
    unsigned long dwHighDateTime;
};/* FILETIME, * PFILETIME, * LPFILETIME;*/

using FileTime = __FILETIME;

#endif


using FileHandle = FILE*;

static const u32 maxPath = 512;


struct Directory
{
    char path[maxPath];

#if defined(_WIN64)
    void* osHandle;
#endif
};


struct FileReadResult
{
    char* data;
    sizet size;
};


// Read file and allocate memory from allocator.
// User is responsible for freeing the memory.
char* FileReadBinary(cstring filename, Allocator* allocator, sizet* size);
char* FileReadText(cstring filename, Allocator* allocator, sizet* size);

FileReadResult FileReadBinary(cstring filename, Allocator* allocator);
FileReadResult FileReadText(cstring filename, Allocator* allocator);

void FileWriteBinary(cstring filename, void* memory, sizet size);

bool FileExists(cstring path);
void FileOpen(cstring filename, cstring mode, FileHandle* file);
void FileClose(FileHandle file);
sizet FileWrite(uint8_t* memory, u32 elementSize, u32 count, FileHandle file);
bool FileDelete(cstring path);

#if defined(_WIN64)
FileTime FileLastWriteTime(cstring filename);
#endif

// Try to resolve path to non-relative version.
u32 FileResolveToFullPath(cstring path, char* outFullPath, u32 maxSize);

// Inplace path methods

/// <summary>
/// Retrieve path without the filename. Path is a preallocated string buffer. 
/// It moves the terminator before the name of the file.
/// </summary>
/// <param name="path"></param>
void FileDirectoryFromPath(char* path);

void FileNameFromPath(char* path);
char* FileExtensionFromPath(char* path);

bool DirectoryExists(cstring path);
bool DirectoryCreate(cstring path);
bool DirectoryDelete(cstring path);

void DirectoryCurrent(Directory* directory);
void DirectoryChange(cstring path);

void FileOpenDirectory(cstring path, Directory* out_directory);
void FileCloseDirectory(Directory* directory);
void FileParentDirectory(Directory* directory);
void FileSubDirectory(Directory* directory, cstring sub_directory_name);

// Search files matching file_pattern and puts them in files array.
void FileFindFilesInPath(cstring file_pattern, StringArray& files); 

/// <summary>
/// Search files and directories using search_patterns.
/// Examples: "..\\data\\*", "*.bin", "*.*"
/// </summary>
/// <param name="extension"></param>
/// <param name="search_pattern"></param>
/// <param name="files"></param>
/// <param name="directories"></param>
void FileFindFilesInPath(cstring extension, cstring search_pattern,
    StringArray& files, StringArray& directories); 

// TODO: move
void EnvironmentVariableGet(cstring name, char* output, u32 output_size);

struct ScopedFile
{
    ScopedFile(cstring filename, cstring mode);
    ~ScopedFile();

    FileHandle file;
};