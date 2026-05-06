#pragma once

#include "llm/LLM.hpp"

#include <string>

namespace textrpg::llm {

class OllamaChatClient final : public IChatClient {
public:
    explicit OllamaChatClient(LLMOptions options);

    std::string chat(const std::string& prompt) const override;

private:
    LLMOptions options_;
};

} // namespace textrpg::llm
