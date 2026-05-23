#include "llm/OllamaChatClient.hpp"

#include <ollama.hpp>
#include <nlohmann/json.hpp>

#include <utility>

namespace textrpg::llm {
namespace {

std::string resolveEndpoint(const LLMOptions& options)
{
    return options.endpoint.empty() ? "http://localhost:11434" : options.endpoint;
}

std::string resolveModel(const LLMOptions& options)
{
    if (!options.model.empty()) {
        return options.model;
    }
    return "aravhawk/gemma4:26b";
}

} // namespace

OllamaChatClient::OllamaChatClient(LLMOptions options)
    : options_(std::move(options))
{
}

std::string OllamaChatClient::chat(const std::string& prompt) const
{
    Ollama client(resolveEndpoint(options_));
    client.setConnectionTimeout(options_.connectionTimeoutSeconds);
    client.setReadTimeout(options_.readTimeoutSeconds);
    client.setWriteTimeout(options_.readTimeoutSeconds);

    ollama::request request(ollama::message_type::chat);
    request["model"] = resolveModel(options_);
    request["messages"] = nlohmann::json::array({
        {{"role", "user"}, {"content", prompt}}
    });
    request["stream"] = false;
    request["think"] = options_.think;
    request["format"] = "json";
    request["keep_alive"] = "5m";
    request["options"] = {{"temperature", options_.temperature}};

    const ollama::response response = client.chat(request);
    return response.as_simple_string();
}

} // namespace textrpg::llm
