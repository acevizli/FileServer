#pragma once

#include <functional>
#include <string>
#include <vector>

struct UploadedFile {
    std::string fileName;   // name as stored on disk
    std::string path;       // absolute path on disk
    size_t size;

    UploadedFile() : size(0) {}
};

/**
 * Streaming multipart/form-data parser.
 *
 * File parts are written straight to disk as they arrive, so uploads never
 * have to fit in memory.
 */
class MultipartParser {
public:
    // Reads up to n bytes into buf, returns the number of bytes read (<= 0 on error/EOF)
    using Reader = std::function<long(char* buf, size_t n)>;

    MultipartParser(const std::string& boundary, const std::string& uploadDir);

    // initialData holds body bytes that were already read while parsing the headers
    bool parse(const Reader& reader, const std::string& initialData, size_t contentLength);

    const std::vector<UploadedFile>& files() const { return files_; }
    const std::string& error() const { return error_; }

    // Read and discard whatever is left of the body so closing the socket
    // does not reset the connection before the client reads our response
    void drainRemaining(size_t maxBytes);

private:
    bool fill();
    bool ensure(size_t n);
    size_t findWithFill(const std::string& needle, size_t limit);

    bool readPartHeaders(std::string& outFileName);
    bool readFilePart(const std::string& fileName, const std::string& delim);
    bool skipPart(const std::string& delim);

    std::string sanitizeFileName(const std::string& raw) const;
    std::string makeUniquePath(const std::string& fileName, std::string& outName) const;

    std::string boundary_;
    std::string uploadDir_;
    std::string buffer_;
    size_t remaining_;
    Reader reader_;
    std::vector<UploadedFile> files_;
    std::string error_;

    static constexpr size_t CHUNK_SIZE = 64 * 1024;
    static constexpr size_t MAX_PART_HEADERS = 8192;
};
