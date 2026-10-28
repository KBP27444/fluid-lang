#ifndef LANG_ERROR_H
#define LANG_ERROR_H

#include <exception>
#include <string>

class LangError : public std::exception {
public:
    LangError(std::string file, int line, int col, std::string message);

    const char* what() const noexcept override;

    const std::string file;
    const int line;
    const int col;
    const std::string message;

private:
    std::string formatted_;
};

std::string formatDiagnostic(
    const std::string& file,
    int line,
    int col,
    const std::string& severity,
    const std::string& message);

void printWarning(
    const std::string& file,
    int line,
    int col,
    const std::string& message);

#endif
