#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace textrpg::llm {

// 로그 양을 실행 모드에 맞춰 줄이기 위한 설정이다.
struct LogSettings {
    bool enabled = true;
    bool logPrompt = true;
    bool logRawResponse = true;
    bool logValidationError = true;
};

// LLM 모듈의 각 단계별 로그를 파일로 남긴다.
// 게임 진행을 막지 않기 위해 로깅 실패는 호출부에서 핵심 로직으로 다루지 않는다.
class LLMLogger {
public:
    explicit LLMLogger(std::filesystem::path logDirectory = "logs", LogSettings settings = {});

    void logPrompt(int turnNumber, const std::string& playerInput, const std::string& prompt) const;
    void logRawResponse(int turnNumber, const std::string& rawResponse) const;
    void logParseError(int turnNumber, const std::string& errorMessage) const;
    void logValidation(int turnNumber, const std::vector<std::string>& messages) const;
    void logFallback(int turnNumber, const std::string& reason) const;

private:
    std::filesystem::path logDirectory_;
    LogSettings settings_;

    void write(const std::string& loggerName, const std::string& fileName, const std::string& message) const;
};

} // namespace textrpg::llm
