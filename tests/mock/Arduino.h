#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
using byte = uint8_t;
class String {
    std::string s;
public:
    String() = default;
    String(const char* p):s(p){}
    size_t length() const {return s.size();}
    const char* c_str() const {return s.c_str();}
    bool reserve(size_t n){s.reserve(n);return true;}
    bool concat(char c){s+=c;return true;}
};
uint32_t millis();
