#pragma once

// OTA firmware update helpers.
//
// Design goals:
// - Stream multipart uploads in small chunks (no full firmware in RAM)
// - Keep parsing logic testable (UNITTEST build)

#include <Arduino.h>
#include <memory>
#include <string>

namespace ota {

static constexpr size_t kDefaultChunkSize = 4096;
static constexpr size_t kMinFirmwareSizeBytes = 100 * 1024;

class IUpdateWriter {
 public:
  virtual ~IUpdateWriter() = default;
  virtual bool begin(size_t size) = 0;
  virtual size_t write(const uint8_t* data, size_t len) = 0;
  virtual bool end(bool evenIfRemaining) = 0;
  virtual void abort() = 0;
  virtual bool hasError() const = 0;
};

struct MultipartUploadResult {
  bool success = false;
  String error;
  size_t bytesWritten = 0;
  String filename;
};

inline bool filenameLooksLikeBin(const String& filename) {
  const char* s = filename.c_str();
  if (!s) return false;
  size_t n = strlen(s);
  if (n < 4) return false;
  const char* suffix = s + (n - 4);
  return (suffix[0] == '.' && (suffix[1] == 'b' || suffix[1] == 'B') &&
          (suffix[2] == 'i' || suffix[2] == 'I') && (suffix[3] == 'n' || suffix[3] == 'N'));
}

inline int indexOfSubstring(const String& haystack, const char* needle) {
  if (!needle) return -1;
  const std::string hs(haystack.c_str());
  const std::string nd(needle);
  size_t pos = hs.find(nd);
  return pos == std::string::npos ? -1 : (int)pos;
}

inline int indexOfSubstring(const String& haystack, const String& needle) {
  const std::string hs(haystack.c_str());
  const std::string nd(needle.c_str());
  size_t pos = hs.find(nd);
  return pos == std::string::npos ? -1 : (int)pos;
}

inline void trimInPlace(String& s) {
  std::string str(s.c_str());
  size_t start = str.find_first_not_of(" \t\r\n");
  size_t end = str.find_last_not_of(" \t\r\n");
  if (start == std::string::npos) {
    s = "";
    return;
  }
  s = str.substr(start, end - start + 1).c_str();
}

inline bool startsWithIgnoreCase(const String& s, const char* prefixLowercase) {
  const char* c = s.c_str();
  if (!c || !prefixLowercase) return false;
  while (*prefixLowercase) {
    char a = *c++;
    char b = *prefixLowercase++;
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (a != b) return false;
  }
  return true;
}

inline void appendChar(String& s, char c) {
  char tmp[2] = {c, 0};
  s += tmp;
}

inline String extractBoundaryFromContentType(const String& contentType) {
  // Example: multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW
  int idx = indexOfSubstring(contentType, "boundary=");
  if (idx < 0) {
    return String();
  }
  String boundary = contentType.substring(idx + 9);
  trimInPlace(boundary);
  const char* b = boundary.c_str();
  if (b && boundary.length() >= 2 && b[0] == '"' && b[boundary.length() - 1] == '"') {
    boundary = boundary.substring(1, boundary.length() - 1);
  }
  return boundary;
}

template <typename Client>
inline bool readLineCRLF(Client& client, String& outLine, uint32_t timeoutMs = 5000) {
  outLine = "";
  uint32_t deadline = millis() + timeoutMs;
  while (millis() < deadline) {
    while (client.available()) {
      char c = (char)client.read();
      if (c == '\n') {
        const char* s = outLine.c_str();
        if (s && outLine.length() > 0 && s[outLine.length() - 1] == '\r') {
          outLine = outLine.substring(0, outLine.length() - 1);
        }
        return true;
      }
      appendChar(outLine, c);
      if (outLine.length() > 1024) {
        return false;
      }
    }
    delay(1);
  }
  return false;
}

template <typename Client>
inline MultipartUploadResult streamMultipartFirmwareToUpdate(Client& client,
                                                            size_t contentLength,
                                                            const String& boundary,
                                                            IUpdateWriter& updateWriter,
                                                            size_t chunkSize = kDefaultChunkSize) {
  MultipartUploadResult result;
  (void)contentLength;
  if (boundary.length() == 0) {
    result.error = "Missing multipart boundary";
    return result;
  }

  const String startBoundaryLine = "--" + boundary;
  const String boundaryNeedle = "\r\n--" + boundary;
  const String finalBoundaryNeedle = "\r\n--" + boundary + "--";

  String line;
  if (!readLineCRLF(client, line)) {
    result.error = "Failed to read multipart start";
    return result;
  }
  trimInPlace(line);
  if (strcmp(line.c_str(), startBoundaryLine.c_str()) != 0) {
    result.error = "Invalid multipart start boundary";
    return result;
  }

  // Part headers.
  while (true) {
    if (!readLineCRLF(client, line)) {
      result.error = "Failed to read multipart headers";
      return result;
    }
    if (line.length() == 0) {
      break;
    }

    if (startsWithIgnoreCase(line, "content-disposition:")) {
      // Example: Content-Disposition: form-data; name="firmware"; filename="firmware.bin"
      int filenamePos = indexOfSubstring(line, "filename=");
      if (filenamePos >= 0) {
        const std::string ls(line.c_str());
        size_t quoteStart = ls.find('"', (size_t)filenamePos);
        if (quoteStart >= 0) {
          size_t quoteEnd = ls.find('"', quoteStart + 1);
          if (quoteEnd > quoteStart) {
            result.filename = ls.substr(quoteStart + 1, quoteEnd - quoteStart - 1).c_str();
          }
        }
      }
    }
  }

  if (!filenameLooksLikeBin(result.filename)) {
    result.error = "Only .bin files accepted";
    return result;
  }

  if (!updateWriter.begin(0)) {
    result.error = "Update.begin failed";
    return result;
  }

  // Stream file content until the next boundary.
  if (chunkSize < 256) {
    chunkSize = 256;
  }
  if (chunkSize > 8192) {
    chunkSize = 8192;
  }

  String overlap;
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[chunkSize]);
  bool foundEnd = false;

  while (client.connected() && !foundEnd) {
    int avail = client.available();
    if (avail <= 0) {
      delay(1);
      continue;
    }

    size_t toRead = (size_t)avail;
    if (toRead > chunkSize) {
      toRead = chunkSize;
    }

    int readBytes = client.read(buffer.get(), toRead);
    if (readBytes <= 0) {
      delay(1);
      continue;
    }

    // Combine overlap + new bytes into a working String for boundary search.
    String chunk = overlap;
    for (int i = 0; i < readBytes; i++) {
      appendChar(chunk, (char)buffer[i]);
    }

    int finalPos = indexOfSubstring(chunk, finalBoundaryNeedle);
    int normalPos = indexOfSubstring(chunk, boundaryNeedle);
    int boundaryPos = -1;
    if (finalPos >= 0) {
      boundaryPos = finalPos;
      foundEnd = true;
    } else if (normalPos >= 0) {
      boundaryPos = normalPos;
      foundEnd = true;
    }

    if (boundaryPos >= 0) {
      // Everything before boundaryPos is firmware bytes.
      if (boundaryPos > 0) {
        size_t firmwareLen = (size_t)boundaryPos;
        size_t written = updateWriter.write((const uint8_t*)chunk.c_str(), firmwareLen);
        result.bytesWritten += written;
        if (written != firmwareLen || updateWriter.hasError()) {
          updateWriter.abort();
          result.error = "Update.write failed";
          return result;
        }
      }
      break;
    }

    // No boundary in this chunk. Keep a small tail as overlap.
    size_t keep = finalBoundaryNeedle.length();
    if (keep > (size_t)chunk.length()) {
      keep = (size_t)chunk.length();
    }

    size_t firmwareLen = (size_t)chunk.length() - keep;
    if (firmwareLen > 0) {
      size_t written = updateWriter.write((const uint8_t*)chunk.c_str(), firmwareLen);
      result.bytesWritten += written;
      if (written != firmwareLen || updateWriter.hasError()) {
        updateWriter.abort();
        result.error = "Update.write failed";
        return result;
      }
    }
    overlap = chunk.substring((int)firmwareLen);

    #ifndef UNITTEST
    yield();
    vTaskDelay(0);
    #endif
  }

  if (!foundEnd) {
    updateWriter.abort();
    result.error = "Did not find multipart end boundary";
    return result;
  }

  if (result.bytesWritten < kMinFirmwareSizeBytes) {
    updateWriter.abort();
    result.error = "Firmware file too small";
    return result;
  }

  if (!updateWriter.end(true) || updateWriter.hasError()) {
    updateWriter.abort();
    result.error = "Update.end failed";
    return result;
  }

  result.success = true;
  return result;
}

}  // namespace ota
