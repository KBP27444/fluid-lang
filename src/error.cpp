#include "error.h"

#include <iostream>
#include <utility>

std::string formatDiagnostic(
    const std::string& file,
    int line,
    int col,
    const std::string& severity,
    const std::string& message) {
    return file + ":" + std::to_string(line) + ":" + std::to_string(col) +
           ": " + severity + ": " + message;
}

LangError::LangError(
    std::string fileValue,
    int lineValue,
    int colValue,
    std::string messageValue)
    : file(std::move(fileValue)),
      line(lineValue),
      col(colValue),
      message(std::move(messageValue)),
      formatted_(formatDiagnostic(file, line, col, "错误", message)) {}

const char* LangError::what() const noexcept {
    return formatted_.c_str();
}

void printWarning(
    const std::string& file,
    int line,
    int col,
    const std::string& message) {
    std::cerr << formatDiagnostic(file, line, col, "warning", message) << '\n';
}
