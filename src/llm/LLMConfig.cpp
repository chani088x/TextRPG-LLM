#include "llm/LLMConfig.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace textrpg::llm {
namespace {

std::string trim(std::string value)
{
    const auto isSpace = [](unsigned char ch) {
        return std::isspace(ch) != 0;
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char ch) {
        return !isSpace(static_cast<unsigned char>(ch));
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char ch) {
                    return !isSpace(static_cast<unsigned char>(ch));
                }).base(),
        value.end());
    return value;
}

std::string stripInlineComment(const std::string& line)
{
    bool inString = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '"') {
            inString = !inString;
        } else if (ch == '#' && !inString) {
            return line.substr(0, i);
        }
    }
    return line;
}

std::string parseString(std::string value)
{
    value = trim(value);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

bool parseBool(std::string value)
{
    value = trim(value);
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    throw std::runtime_error("invalid boolean value: " + value);
}

int parseInt(const std::string& value)
{
    return std::stoi(trim(value));
}

double parseDouble(const std::string& value)
{
    return std::stod(trim(value));
}

void applySetting(LLMConfig& config, const std::string& section, const std::string& key, const std::string& value)
{
    if (section == "llm") {
        if (key == "backend") {
            config.backend = parseString(value);
        } else if (key == "endpoint") {
            config.ollama.endpoint = parseString(value);
        } else if (key == "api_path") {
            config.ollama.apiPath = parseString(value);
        } else if (key == "model") {
            config.ollama.model = parseString(value);
        } else if (key == "temperature") {
            config.ollama.temperature = parseDouble(value);
        } else if (key == "stream") {
            config.ollama.stream = parseBool(value);
        } else if (key == "think") {
            config.ollama.think = parseBool(value);
        } else if (key == "nothink") {
            config.ollama.think = !parseBool(value);
        } else if (key == "connection_timeout_seconds") {
            config.ollama.connectionTimeoutSeconds = parseInt(value);
        } else if (key == "read_timeout_seconds") {
            config.ollama.readTimeoutSeconds = parseInt(value);
        }
        return;
    }

    if (section == "prompt") {
        if (key == "max_recent_events") {
            config.prompt.maxRecentEvents = static_cast<std::size_t>(parseInt(value));
        } else if (key == "max_choices") {
            config.validation.maxChoices = static_cast<std::size_t>(parseInt(value));
        } else if (key == "language") {
            config.prompt.language = parseString(value);
        }
        return;
    }

    if (section == "validation") {
        if (key == "max_monster_hp") {
            config.validation.maxMonsterHp = parseInt(value);
        } else if (key == "max_monster_attack") {
            config.validation.maxMonsterAttack = parseInt(value);
        } else if (key == "max_monster_defense") {
            config.validation.maxMonsterDefense = parseInt(value);
        } else if (key == "max_hp_delta") {
            config.validation.maxHpDelta = parseInt(value);
        } else if (key == "max_gold_reward") {
            config.validation.maxGoldReward = parseInt(value);
        } else if (key == "max_exp_reward") {
            config.validation.maxExpReward = parseInt(value);
        }
        return;
    }

    if (section == "debug") {
        if (key == "log_prompt") {
            config.debug.logPrompt = parseBool(value);
        } else if (key == "log_raw_response") {
            config.debug.logRawResponse = parseBool(value);
        } else if (key == "log_validation_error") {
            config.debug.logValidationError = parseBool(value);
        }
    }
}

} // namespace

LLMConfig LLMConfigLoader::load(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open config file: " + path.string());
    }

    LLMConfig config;
    std::string section;
    std::string line;
    int lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;
        line = trim(stripInlineComment(line));
        if (line.empty()) {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }

        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            std::ostringstream message;
            message << "invalid config line " << lineNumber << ": " << line;
            throw std::runtime_error(message.str());
        }

        const auto key = trim(line.substr(0, equals));
        const auto value = trim(line.substr(equals + 1));
        applySetting(config, section, key, value);
    }

    return config;
}

} // namespace textrpg::llm
