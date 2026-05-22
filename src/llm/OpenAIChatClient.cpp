#include "llm/OpenAIChatClient.hpp"

#include <openai.hpp>

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

namespace textrpg::llm {
namespace {

std::string readEnv(const char* name)
{
    if (const char* value = std::getenv(name)) {
        return value;
    }
    return {};
}

std::string withTrailingSlash(std::string value)
{
    if (!value.empty() && value.back() != '/') {
        value += '/';
    }
    return value;
}

std::string resolveApiKey(const LLMOptions& options)
{
    if (!options.apiKey.empty()) {
        return options.apiKey;
    }
    return readEnv("OPENAI_API_KEY");
}

std::string resolveOrganization(const LLMOptions& options)
{
    if (!options.organization.empty()) {
        return options.organization;
    }
    return readEnv("OPENAI_ORG");
}

std::string resolveEndpoint(const LLMOptions& options)
{
    if (!options.endpoint.empty()) {
        return withTrailingSlash(options.endpoint);
    }

    const auto envEndpoint = readEnv("OPENAI_API_BASE");
    if (!envEndpoint.empty()) {
        return withTrailingSlash(envEndpoint);
    }

    return "https://api.openai.com/v1/";
}

std::string resolveModel(const LLMOptions& options)
{
    if (!options.model.empty()) {
        return options.model;
    }

    const auto envModel = readEnv("OPENAI_MODEL");
    if (!envModel.empty()) {
        return envModel;
    }

    return "gpt-4.1-mini";
}

std::string readTextContent(const openai::Json& response)
{
    if (!response.contains("choices") || response["choices"].empty()) {
        throw std::runtime_error("OpenAI response did not include choices");
    }

    const auto& choice = response["choices"].at(0);
    if (!choice.contains("message")) {
        throw std::runtime_error("OpenAI response did not include a message");
    }

    const auto& message = choice["message"];
    if (message.contains("refusal") && message["refusal"].is_string()
        && !message["refusal"].get<std::string>().empty()) {
        throw std::runtime_error("OpenAI refused the request: " + message["refusal"].get<std::string>());
    }

    if (!message.contains("content")) {
        throw std::runtime_error("OpenAI response message did not include content");
    }

    const auto& content = message["content"];
    if (content.is_string()) {
        return content.get<std::string>();
    }

    if (content.is_array()) {
        std::string text;
        for (const auto& part : content) {
            if (part.contains("text") && part["text"].is_string()) {
                text += part["text"].get<std::string>();
            }
        }
        if (!text.empty()) {
            return text;
        }
    }

    throw std::runtime_error("OpenAI response content was not text");
}

} // namespace

OpenAIChatClient::OpenAIChatClient(LLMOptions options)
    : options_(std::move(options))
{
}

std::string OpenAIChatClient::chat(const std::string& prompt) const
{
    const auto apiKey = resolveApiKey(options_);
    if (apiKey.empty()) {
        throw std::runtime_error("OPENAI_API_KEY is not set");
    }

    openai::OpenAI client(apiKey, resolveOrganization(options_), true, resolveEndpoint(options_));
    const openai::Json request = {
        {"model", resolveModel(options_)},
        {"messages", openai::Json::array({
            {
                {"role", "system"},
                {"content", "You generate only valid JSON for a Korean medieval fantasy text RPG. Do not wrap JSON in markdown."}
            },
            {
                {"role", "user"},
                {"content", prompt}
            }
        })},
        {"temperature", options_.temperature},
        {"response_format", {{"type", "json_object"}}}
    };

    return readTextContent(client.chat.create(request));
}

} // namespace textrpg::llm
