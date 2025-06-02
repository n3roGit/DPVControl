#pragma once

namespace SPIFFS {
    inline bool begin() { return true; }
    inline bool exists(const char*) { return false; }
} 