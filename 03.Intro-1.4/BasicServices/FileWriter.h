#pragma once

#include "../Defines.h"
#include <fstream>

namespace services {

class FileWriter {
public:
    FileWriter() = default;
    explicit FileWriter(const str& filepath);
    ~FileWriter();

    // Disable copy, allow move
    FileWriter(const FileWriter&) = delete;
    FileWriter& operator=(const FileWriter&) = delete;
    FileWriter(FileWriter&&) = default;
    FileWriter& operator=(FileWriter&&) = default;

    bool open(const str& filepath);
    void write(const str& text);
    void writeLine(const str& text);
    bool isOpen() const;
    void close();

private:
    std::ofstream stream;
};

} // namespace services
