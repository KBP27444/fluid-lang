#ifndef LANG_SOURCE_H
#define LANG_SOURCE_H

#include <string>

struct Source {
    std::string file;
    std::string text;
};

Source loadSource(const std::string& path);

#endif
