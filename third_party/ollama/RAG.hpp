/*
MIT License

Copyright (c) 2025 J.G.Adams

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "ollama.hpp"

namespace ollama {
namespace RAG {

constexpr double SIMILARITY_THRESHOLD = 0.0;

inline void RAG_add_chunk_to_database(
    std::vector<std::pair<std::string, std::vector<float>>>& rag_database,
    const std::string& embedding_model,
    const std::string& chunk)
{
    if (!chunk.empty()) {
        ollama::response response = ollama::generate_embeddings(embedding_model, chunk);
        nlohmann::json json_response = response.as_json();
        std::vector<float> embedding = json_response["embeddings"][0].get<std::vector<float>>();
        rag_database.emplace_back(chunk, embedding);
    }
}

static float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b)
{
    float dot_product = 0.0F;
    float norm_a = 0.0F;
    float norm_b = 0.0F;
    for (size_t i = 0; i < a.size(); ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    return dot_product / (std::sqrt(norm_a) * std::sqrt(norm_b));
}

inline std::vector<std::pair<std::string, float>> RAG_retrieve(
    std::vector<std::pair<std::string, std::vector<float>>>& rag_database,
    const std::string& embedding_model,
    const std::string& query,
    size_t top_n = 3)
{
    std::vector<std::pair<std::string, float>> similarities;
    if (!query.empty()) {
        ollama::response response = ollama::generate_embeddings(embedding_model, query);
        nlohmann::json json_response = response.as_json();
        if (json_response["embeddings"].is_array()) {
            std::vector<float> query_embedding = json_response["embeddings"][0].get<std::vector<float>>();

            for (auto it = rag_database.begin(); it != rag_database.end(); ++it) {
                float similarity = cosine_similarity(query_embedding, it->second);
                if (similarity > SIMILARITY_THRESHOLD) {
                    similarities.emplace_back(it->first, similarity);
                }
            }

            std::sort(
                similarities.begin(),
                similarities.end(),
                [](const std::pair<std::string, float>& a, const std::pair<std::string, float>& b) {
                    return a.second > b.second;
                });

            if (similarities.size() > top_n) {
                similarities.resize(top_n);
            }
        }
    }

    return similarities;
}

inline bool RAG_loadDocument_ByStatement(
    std::vector<std::pair<std::string, std::vector<float>>>& rag_database,
    const std::string& embedding_model,
    const std::string& path)
{
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Error: Could not open " + path + "\n";
        return false;
    }

    std::string chunk;
    char ch;
    while (file.get(ch)) {
        chunk.push_back(ch);
        if (ch == '.' || ch == '?' || ch == '!') {
            RAG_add_chunk_to_database(rag_database, embedding_model, chunk);
            chunk = "";
        }
        if (ch == ':') {
            chunk = "";
        }
    }
    file.close();
    return true;
}

inline bool RAG_loadDocument_ByLine(
    std::vector<std::pair<std::string, std::vector<float>>>& rag_database,
    const std::string& embedding_model,
    const std::string& path)
{
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Error: Could not open " + path + "\n";
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        RAG_add_chunk_to_database(rag_database, embedding_model, line);
    }
    file.close();
    return true;
}

} // namespace RAG
} // namespace ollama
