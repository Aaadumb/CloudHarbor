#pragma once

#include <json/json.h>

#include <fstream>
#include <stdexcept>
#include <string>

inline Json::Value loadDefaultClient(const char* path, const char* section) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error(std::string("无法读取配置文件 ") + path);
    }

    Json::Value config;
    Json::CharReaderBuilder reader;
    if (!Json::parseFromStream(reader, input, &config, nullptr)) {
        throw std::runtime_error("配置文件 JSON 格式错误");
    }

    const auto& clients = config[section];
    if (clients.isArray()) {
        for (const auto& client : clients) {
            if (client.isObject() && client.get("name", "").asString() == "default") {
                return client;
            }
        }
    }
    throw std::runtime_error(std::string(section) + " 中缺少 default 客户端");
}
