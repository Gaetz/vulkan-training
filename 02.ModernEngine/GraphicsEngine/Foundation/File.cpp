#include "File.hpp"

#include "Memory.hpp"
#include "Assert.hpp"
#include "String.hpp"

#if defined(_WIN64)
#include <windows.h>
#else
#define MAX_PATH 65536
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <string.h>



void FileOpen(cstring filename, cstring mode, FileHandle* file)
{
#if defined(_WIN64)
    fopen_s(file, filename, mode);
#else
    * file = fopen(filename, mode);
#endif
}

void FileClose(FileHandle file)
{
    if (file)
        fclose(file);
}

sizet FileWrite(uint8_t* memory, u32 element_size, u32 count, FileHandle file)
{
    return fwrite(memory, element_size, count, file);
}

static long FileGetSize(FileHandle f)
{
    long fileSizeSigned;

    fseek(f, 0, SEEK_END);
    fileSizeSigned = ftell(f);
    fseek(f, 0, SEEK_SET);

    return fileSizeSigned;
}

#if defined(_WIN64)
FileTime FileLastWriteTime(cstring filename)
{
    FileTime lastWriteTime = {};

    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExA(filename, GetFileExInfoStandard, &data))
    {
        lastWriteTime.dwHighDateTime = data.ftLastWriteTime.dwHighDateTime;
        lastWriteTime.dwLowDateTime = data.ftLastWriteTime.dwLowDateTime;
    }

    return lastWriteTime;
}
#endif // _WIN64

u32 FileResolveToFullPath(cstring path, char* outFullPath, u32 maxSize)
{
#if defined(_WIN64)
    return GetFullPathNameA(path, maxSize, outFullPath, nullptr);
#else
    return readlink(path, outFullPath, maxSize);
#endif // _WIN64
}

void FileDirectoryFromPath(char* path)
{
    char* last_point = strrchr(path, '.');
    char* lastSeparator = strrchr(path, '/');
    if (lastSeparator != nullptr && last_point > lastSeparator)
    {
        *(lastSeparator + 1) = 0;
    }
    else
    {
        // Try searching backslash
        lastSeparator = strrchr(path, '\\');
        if (lastSeparator != nullptr && last_point > lastSeparator)
        {
            *(lastSeparator + 1) = 0;
        }
        else
        {
            // Wrong input!
            GASSERTM(false, "Malformed path %s!", path);
        }
    }
}

void FileNameFromPath(char* path)
{
    char* lastSeparator = strrchr(path, '/');
    if (lastSeparator == nullptr)
    {
        lastSeparator = strrchr(path, '\\');
    }

    if (lastSeparator != nullptr)
    {
        sizet name_length = strlen(lastSeparator + 1);

        memcpy(path, lastSeparator + 1, name_length);
        path[name_length] = 0;
    }
}

char* FileExtensionFromPath(char* path)
{
    char* lastSeparator = strrchr(path, '.');

    return lastSeparator + 1;
}

bool FileExists(cstring path)
{
#if defined(_WIN64)
    WIN32_FILE_ATTRIBUTE_DATA unused;
    return GetFileAttributesExA(path, GetFileExInfoStandard, &unused);
#else
    int result = access(path, F_OK);
    return (result == 0);
#endif // _WIN64
}

bool FileDelete(cstring path)
{
#if defined(_WIN64)
    int result = remove(path);
    return result != 0;
#else
    int result = remove(path);
    return (result == 0);
#endif
}

bool DirectoryExists(cstring path)
{
#if defined(_WIN64)
    WIN32_FILE_ATTRIBUTE_DATA unused;
    return GetFileAttributesExA(path, GetFileExInfoStandard, &unused);
#else
    int result = access(path, F_OK);
    return (result == 0);
#endif // _WIN64
}

bool DirectoryCreate(cstring path)
{
#if defined(_WIN64)
    int result = CreateDirectoryA(path, NULL);
    return result != 0;
#else
    int result = mkdir(path, S_IRWXU | S_IRWXG);
    return (result == 0);
#endif // _WIN64
}

bool DirectoryDelete(cstring path)
{
#if defined(_WIN64)
    int result = RemoveDirectoryA(path);
    return result != 0;
#else
    int result = rmdir(path);
    return (result == 0);
#endif // _WIN64
}

void DirectoryCurrent(Directory* directory)
{
#if defined(_WIN64)
    DWORD written_chars = GetCurrentDirectoryA(maxPath, directory->path);
    directory->path[written_chars] = 0;
#else
    getcwd(directory->path, maxPath);
#endif // _WIN64
}

void DirectoryChange(cstring path)
{
#if defined(_WIN64)
    if (!SetCurrentDirectoryA(path))
    {
        GPrint("Cannot change current directory to %s\n", path);
    }
#else
    if (chdir(path) != 0)
    {
        GPrint("Cannot change current directory to %s\n", path);
    }
#endif // _WIN64
}

//
static bool string_ends_with_char(cstring s, char c)
{
    cstring last_entry = strrchr(s, c);
    const sizet index = last_entry - s;
    return index == (strlen(s) - 1);
}

void FileOpenDirectory(cstring path, Directory* outDirectory)
{

    // Open file trying to conver to full path instead of relative.
    // If an error occurs, just copy the name.
    if (FileResolveToFullPath(path, outDirectory->path, MAX_PATH) == 0)
    {
        strcpy(outDirectory->path, path);
    }

    // Add '\\' if missing
    if (!string_ends_with_char(path, '\\'))
    {
        strcat(outDirectory->path, "\\");
    }

    if (!string_ends_with_char(outDirectory->path, '*'))
    {
        strcat(outDirectory->path, "*");
    }

#if defined(_WIN64)
    outDirectory->osHandle = nullptr;

    WIN32_FIND_DATAA findData;
    HANDLE found_handle;
    if ((found_handle = FindFirstFileA(outDirectory->path, &findData)) != INVALID_HANDLE_VALUE)
    {
        outDirectory->osHandle = found_handle;
    }
    else
    {
        GPrint("Could not open directory %s\n", outDirectory->path);
    }
#else
    GASSERTM(false, "Not implemented");
#endif
}

void FileCloseDirectory(Directory* directory)
{
#if defined(_WIN64)
    if (directory->osHandle)
    {
        FindClose(directory->osHandle);
    }
#else
    GASSERTM(false, "Not implemented");
#endif
}

void FileParentDirectory(Directory* directory)
{

    Directory newDirectory;

    const char* lastDirectorySeparator = strrchr(directory->path, '\\');
    sizet index = lastDirectorySeparator - directory->path;

    if (index > 0)
    {

        strncpy(newDirectory.path, directory->path, index);
        newDirectory.path[index] = 0;

        lastDirectorySeparator = strrchr(newDirectory.path, '\\');
        sizet secondIndex = lastDirectorySeparator - newDirectory.path;

        if (lastDirectorySeparator)
        {
            newDirectory.path[secondIndex] = 0;
        }
        else
        {
            newDirectory.path[index] = 0;
        }

        FileOpenDirectory(newDirectory.path, &newDirectory);

#if defined(_WIN64)
        // Update directory
        if (newDirectory.osHandle)
        {
            *directory = newDirectory;
        }
#else
        GASSERTM(false, "Not implemented");
#endif
    }
}

void FileSubDirectory(Directory* directory, cstring subDirectoryName)
{

    // Remove the last '*' from the path. It will be re-added by the file_open.
    if (string_ends_with_char(directory->path, '*'))
    {
        directory->path[strlen(directory->path) - 1] = 0;
    }

    strcat(directory->path, subDirectoryName);
    FileOpenDirectory(directory->path, directory);
}

void FileFindFilesInPath(cstring filePattern, StringArray& files)
{

    files.Clear();

#if defined(_WIN64)
    WIN32_FIND_DATAA findData;
    HANDLE hFind;
    if ((hFind = FindFirstFileA(filePattern, &findData)) != INVALID_HANDLE_VALUE)
    {
        do
        {

            files.Intern(findData.cFileName);

        } while (FindNextFileA(hFind, &findData) != 0);
        FindClose(hFind);
    }
    else
    {
        GPrint("Cannot find file %s\n", filePattern);
    }
#else
    GASSERTM(false, "Not implemented");
    // TODO(marco): opendir, readdir
#endif
}

void FileFindFilesInPath(cstring extension, cstring searchPattern, StringArray& files, StringArray& directories)
{

    files.Clear();
    directories.Clear();

#if defined(_WIN64)
    WIN32_FIND_DATAA findData;
    HANDLE hFind;
    if ((hFind = FindFirstFileA(searchPattern, &findData)) != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                directories.Intern(findData.cFileName);
            }
            else
            {
                // If filename contains the extension, add it
                if (strstr(findData.cFileName, extension))
                {
                    files.Intern(findData.cFileName);
                }
            }

        } while (FindNextFileA(hFind, &findData) != 0);
        FindClose(hFind);
    }
    else
    {
        GPrint("Cannot find directory %s\n", searchPattern);
    }
#else
    GASSERTM(false, "Not implemented");
#endif
}

void EnvironmentVariableGet(cstring name, char* output, u32 outputSize)
{
#if defined(_WIN64)
    ExpandEnvironmentStringsA(name, output, outputSize);
#else
    cstring realOutput = getenv(name);
    strncpy(output, realOutput, outputSize);
#endif
}

char* FileReadBinary(cstring filename, Allocator* allocator, sizet* size)
{
    char* outData = 0;

    FILE* file = fopen(filename, "rb");

    if (file)
    {

        // TODO: Use filesize or read result ?
        sizet filesize = FileGetSize(file);

        outData = (char*)GAlloca(filesize + 1, allocator);
        fread(outData, filesize, 1, file);
        outData[filesize] = 0;

        if (size)
            *size = filesize;

        fclose(file);
    }

    return outData;
}

char* FileReadText(cstring filename, Allocator* allocator, sizet* size)
{
    char* text = 0;

    FILE* file = fopen(filename, "r");

    if (file)
    {

        sizet filesize = FileGetSize(file);
        text = (char*)GAlloca(filesize + 1, allocator);
        // Correct: use elementcount as filesize, bytesRead becomes the actual bytes read
        // AFTER the end of line conversion for Windows (it uses \r\n).
        sizet bytesRead = fread(text, 1, filesize, file);

        text[bytesRead] = 0;

        if (size)
            *size = filesize;

        fclose(file);
    }

    return text;
}

FileReadResult FileReadBinary(cstring filename, Allocator* allocator)
{
    FileReadResult result{ nullptr, 0 };

    FILE* file = fopen(filename, "rb");

    if (file)
    {

        // TODO: Use filesize or read result ?
        sizet filesize = FileGetSize(file);

        result.data = (char*)GAlloca(filesize, allocator);
        fread(result.data, filesize, 1, file);

        result.size = filesize;

        fclose(file);
    }

    return result;
}

FileReadResult FileReadText(cstring filename, Allocator* allocator)
{
    FileReadResult result{ nullptr, 0 };

    FILE* file = fopen(filename, "r");

    if (file)
    {

        sizet filesize = FileGetSize(file);
        result.data = (char*)GAlloca(filesize + 1, allocator);
        // Correct: use elementcount as filesize, bytesRead becomes the actual bytes read
        // AFTER the end of line conversion for Windows (it uses \r\n).
        sizet bytesRead = fread(result.data, 1, filesize, file);

        result.data[bytesRead] = 0;

        result.size = bytesRead;

        fclose(file);
    }

    return result;
}

void FileWriteBinary(cstring filename, void* memory, sizet size)
{
    FILE* file = fopen(filename, "wb");
    fwrite(memory, size, 1, file);
    fclose(file);
}

// Scoped file //////////////////////////////////////////////////////////////////
ScopedFile::ScopedFile(cstring filename, cstring mode)
{
    FileOpen(filename, mode, &file);
}

ScopedFile::~ScopedFile()
{
    FileClose(file);
}
