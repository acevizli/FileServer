#include "multipart_parser.h"

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <android/log.h>

#define LOG_TAG "MultipartParser"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

bool writeAll(int fd, const char* data, size_t size) {
    size_t written = 0;
    while (written < size) {
        ssize_t n = write(fd, data + written, size - written);
        if (n <= 0) {
            return false;
        }
        written += n;
    }
    return true;
}

bool fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

std::string trim(const std::string& value) {
    size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

} // namespace

MultipartParser::MultipartParser(const std::string& boundary, const std::string& uploadDir)
    : boundary_(boundary), uploadDir_(uploadDir), remaining_(0) {
    // Strip trailing slash so paths join cleanly
    while (uploadDir_.size() > 1 && uploadDir_.back() == '/') {
        uploadDir_.pop_back();
    }
}

bool MultipartParser::fill() {
    if (remaining_ == 0) {
        return false;
    }

    char chunk[CHUNK_SIZE];
    size_t want = std::min(remaining_, CHUNK_SIZE);
    long got = reader_(chunk, want);
    if (got <= 0) {
        return false;
    }

    buffer_.append(chunk, static_cast<size_t>(got));
    remaining_ -= static_cast<size_t>(got);
    return true;
}

bool MultipartParser::ensure(size_t n) {
    while (buffer_.size() < n) {
        if (!fill()) {
            return false;
        }
    }
    return true;
}

size_t MultipartParser::findWithFill(const std::string& needle, size_t limit) {
    size_t searchFrom = 0;
    while (true) {
        size_t pos = buffer_.find(needle, searchFrom);
        if (pos != std::string::npos) {
            return pos;
        }
        if (buffer_.size() > limit) {
            return std::string::npos;
        }
        // Overlap by needle length - 1 so a split needle is still found
        searchFrom = buffer_.size() >= needle.size() ? buffer_.size() - needle.size() + 1 : 0;
        if (!fill()) {
            return std::string::npos;
        }
    }
}

bool MultipartParser::parse(const Reader& reader, const std::string& initialData,
                            size_t contentLength) {
    reader_ = reader;
    buffer_ = initialData;
    remaining_ = contentLength > initialData.size() ? contentLength - initialData.size() : 0;

    const std::string dashBoundary = "--" + boundary_;
    const std::string delim = "\r\n" + dashBoundary;

    // Skip the preamble up to the first boundary
    size_t pos = findWithFill(dashBoundary, 64 * 1024);
    if (pos == std::string::npos) {
        error_ = "Opening boundary not found";
        return false;
    }
    buffer_.erase(0, pos + dashBoundary.size());

    while (true) {
        if (!ensure(2)) {
            error_ = "Unexpected end of request body";
            return false;
        }

        // "--" right after a boundary marks the end of the body
        if (buffer_.compare(0, 2, "--") == 0) {
            break;
        }

        size_t crlf = findWithFill("\r\n", 1024);
        if (crlf == std::string::npos) {
            error_ = "Malformed boundary line";
            return false;
        }
        buffer_.erase(0, crlf + 2);

        std::string fileName;
        if (!readPartHeaders(fileName)) {
            return false;
        }

        if (!fileName.empty()) {
            if (!readFilePart(fileName, delim)) {
                return false;
            }
        } else {
            // A plain form field - not something we store
            if (!skipPart(delim)) {
                return false;
            }
        }
    }

    if (files_.empty()) {
        error_ = "No file parts in request";
        return false;
    }

    return true;
}

void MultipartParser::drainRemaining(size_t maxBytes) {
    buffer_.clear();

    size_t drained = 0;
    char chunk[CHUNK_SIZE];

    while (remaining_ > 0 && drained < maxBytes) {
        size_t want = std::min(remaining_, std::min(CHUNK_SIZE, maxBytes - drained));
        long got = reader_(chunk, want);
        if (got <= 0) {
            return;
        }
        remaining_ -= static_cast<size_t>(got);
        drained += static_cast<size_t>(got);
    }
}

bool MultipartParser::readPartHeaders(std::string& outFileName) {
    outFileName.clear();

    size_t end = findWithFill("\r\n\r\n", MAX_PART_HEADERS);
    if (end == std::string::npos) {
        error_ = "Part headers too large or malformed";
        return false;
    }

    std::string headerBlock = buffer_.substr(0, end);
    buffer_.erase(0, end + 4);

    // Find Content-Disposition and pull out filename="..."
    std::string lower = headerBlock;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    size_t dispPos = lower.find("content-disposition:");
    if (dispPos == std::string::npos) {
        return true; // no disposition - treat as a nameless field, skipped by caller
    }

    size_t lineEnd = headerBlock.find("\r\n", dispPos);
    std::string disposition = headerBlock.substr(dispPos, lineEnd == std::string::npos
                                                          ? std::string::npos
                                                          : lineEnd - dispPos);

    std::string dispLower = disposition;
    std::transform(dispLower.begin(), dispLower.end(), dispLower.begin(), ::tolower);

    size_t namePos = dispLower.find("filename=\"");
    if (namePos == std::string::npos) {
        return true;
    }

    namePos += strlen("filename=\"");
    size_t nameEnd = disposition.find('"', namePos);
    if (nameEnd == std::string::npos) {
        return true;
    }

    outFileName = disposition.substr(namePos, nameEnd - namePos);
    outFileName = trim(outFileName);
    return true;
}

bool MultipartParser::readFilePart(const std::string& fileName, const std::string& delim) {
    std::string storedName;
    std::string path = makeUniquePath(sanitizeFileName(fileName), storedName);

    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0) {
        LOGE("Failed to create %s: %s", path.c_str(), strerror(errno));
        error_ = "Could not create file on device";
        return false;
    }

    size_t total = 0;
    const size_t keep = delim.size() - 1;

    while (true) {
        size_t pos = buffer_.find(delim);
        if (pos != std::string::npos) {
            if (pos > 0 && !writeAll(fd, buffer_.data(), pos)) {
                close(fd);
                unlink(path.c_str());
                error_ = "Write failed - device may be out of space";
                return false;
            }
            total += pos;
            buffer_.erase(0, pos + delim.size());
            break;
        }

        // Delimiter could still straddle the buffer tail, so hold that much back
        if (buffer_.size() > keep) {
            size_t writable = buffer_.size() - keep;
            if (!writeAll(fd, buffer_.data(), writable)) {
                close(fd);
                unlink(path.c_str());
                error_ = "Write failed - device may be out of space";
                return false;
            }
            total += writable;
            buffer_.erase(0, writable);
        }

        if (!fill()) {
            close(fd);
            unlink(path.c_str());
            error_ = "Upload ended before the file was complete";
            return false;
        }
    }

    close(fd);

    UploadedFile uploaded;
    uploaded.fileName = storedName;
    uploaded.path = path;
    uploaded.size = total;
    files_.push_back(uploaded);

    LOGI("Saved upload: %s (%zu bytes)", path.c_str(), total);
    return true;
}

bool MultipartParser::skipPart(const std::string& delim) {
    const size_t keep = delim.size() - 1;

    while (true) {
        size_t pos = buffer_.find(delim);
        if (pos != std::string::npos) {
            buffer_.erase(0, pos + delim.size());
            return true;
        }

        if (buffer_.size() > keep) {
            buffer_.erase(0, buffer_.size() - keep);
        }

        if (!fill()) {
            error_ = "Unexpected end of request body";
            return false;
        }
    }
}

std::string MultipartParser::sanitizeFileName(const std::string& raw) const {
    // Some browsers send a full path - keep only the last component
    size_t slash = raw.find_last_of("/\\");
    std::string name = (slash == std::string::npos) ? raw : raw.substr(slash + 1);

    std::string clean;
    for (char c : name) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 0x20 || c == ':' || c == '"' || c == '*' || c == '?' ||
            c == '<' || c == '>' || c == '|') {
            clean += '_';
        } else {
            clean += c;
        }
    }

    clean = trim(clean);
    if (clean.empty() || clean == "." || clean == "..") {
        clean = "upload.bin";
    }
    if (clean.size() > 200) {
        clean = clean.substr(0, 200);
    }
    return clean;
}

std::string MultipartParser::makeUniquePath(const std::string& fileName,
                                            std::string& outName) const {
    std::string candidate = uploadDir_ + "/" + fileName;
    if (!fileExists(candidate)) {
        outName = fileName;
        return candidate;
    }

    std::string stem = fileName;
    std::string ext;
    size_t dot = fileName.rfind('.');
    if (dot != std::string::npos && dot > 0) {
        stem = fileName.substr(0, dot);
        ext = fileName.substr(dot);
    }

    for (int i = 1; i < 10000; ++i) {
        std::string name = stem + " (" + std::to_string(i) + ")" + ext;
        candidate = uploadDir_ + "/" + name;
        if (!fileExists(candidate)) {
            outName = name;
            return candidate;
        }
    }

    outName = fileName;
    return uploadDir_ + "/" + fileName;
}
