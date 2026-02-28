#include "FileWriter.h"

namespace services {

FileWriter::FileWriter(const str& filepath) {
    open(filepath);
}

FileWriter::~FileWriter() {
    close();
}

bool FileWriter::open(const str& filepath) {
    close();  // Close any existing file
    stream.open(filepath);
    return stream.is_open();
}

void FileWriter::write(const str& text) {
    if (stream.is_open()) {
        stream << text;
    }
}

void FileWriter::writeLine(const str& text) {
    if (stream.is_open()) {
        stream << text << "\n";
    }
}

bool FileWriter::isOpen() const {
    return stream.is_open();
}

void FileWriter::close() {
    if (stream.is_open()) {
        stream.close();
    }
}

} // namespace services
