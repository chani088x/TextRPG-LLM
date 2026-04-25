#include "llm/OllamaClient.hpp"

#include <ollama.hpp>

#include <utility>

namespace textrpg::llm {

OllamaClient::OllamaClient(OllamaConfig config)
    : config_(std::move(config))
{
}

std::string OllamaClient::generate(const std::string& prompt)
{
    // Ollama 객체는 요청 단위로 생성해 설정 변경이 다른 호출에 새지 않게 한다.
    Ollama client(config_.endpoint);
    client.setConnectionTimeout(config_.connectionTimeoutSeconds);
    client.setReadTimeout(config_.readTimeoutSeconds);
    client.setWriteTimeout(config_.readTimeoutSeconds);

    ollama::request request(ollama::message_type::chat);
    request["model"] = config_.model;
    request["messages"] = nlohmann::json::array({
        {{"role", "user"}, {"content", prompt}}
    });
    request["stream"] = config_.stream;
    request["think"] = config_.think;
    request["keep_alive"] = "5m";
    request["options"] = {{"temperature", config_.temperature}};

    const ollama::response response = client.chat(request);
    // chat 응답의 message.content만 반환하고, 이후 JSON 검증은 LLMOutputParser가 맡는다.
    return response.as_simple_string();
}

} // namespace textrpg::llm
