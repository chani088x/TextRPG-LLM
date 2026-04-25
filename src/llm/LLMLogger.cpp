#include "llm/LLMLogger.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <sstream>
#include <utility>

namespace textrpg::llm {

LLMLogger::LLMLogger(std::filesystem::path logDirectory, LogSettings settings)
    : logDirectory_(std::move(logDirectory))
    , settings_(settings)
{
}

void LLMLogger::logPrompt(int turnNumber, const std::string& playerInput, const std::string& prompt) const
{
    if (!settings_.enabled || !settings_.logPrompt) {
        return;
    }

    std::ostringstream message;
    message << "turn=" << turnNumber << " input=\"" << playerInput << "\"\n" << prompt;
    write("llm_prompt", "llm_prompt.log", message.str());
}

void LLMLogger::logRawResponse(int turnNumber, const std::string& rawResponse) const
{
    if (!settings_.enabled || !settings_.logRawResponse) {
        return;
    }

    std::ostringstream message;
    message << "turn=" << turnNumber << "\n" << rawResponse;
    write("llm_raw_response", "llm_raw_response.log", message.str());
}

void LLMLogger::logParseError(int turnNumber, const std::string& errorMessage) const
{
    if (!settings_.enabled) {
        return;
    }

    std::ostringstream message;
    message << "turn=" << turnNumber << " parse_error=\"" << errorMessage << "\"";
    write("llm_parse_error", "llm_parse_error.log", message.str());
}

void LLMLogger::logValidation(int turnNumber, const std::vector<std::string>& messages) const
{
    if (!settings_.enabled || !settings_.logValidationError || messages.empty()) {
        return;
    }

    std::ostringstream message;
    message << "turn=" << turnNumber << '\n';
    for (const auto& entry : messages) {
        message << "- " << entry << '\n';
    }
    write("llm_validation", "llm_validation.log", message.str());
}

void LLMLogger::logFallback(int turnNumber, const std::string& reason) const
{
    if (!settings_.enabled) {
        return;
    }

    std::ostringstream message;
    message << "turn=" << turnNumber << " fallback_reason=\"" << reason << "\"";
    write("llm_fallback", "llm_fallback.log", message.str());
}

void LLMLogger::write(const std::string& loggerName, const std::string& fileName, const std::string& message) const
{
    std::filesystem::create_directories(logDirectory_);
    const auto path = logDirectory_ / fileName;

    auto logger = spdlog::get(loggerName);
    if (!logger) {
        logger = spdlog::basic_logger_mt(loggerName, path.string());
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
    }

    logger->info(message);
    logger->flush();
}

} // namespace textrpg::llm
