#include "../test_config.h"

#include <hiredis/hiredis.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {
using Reply = std::unique_ptr<redisReply, decltype(&freeReplyObject)>;

template <typename... Args>
Reply command(redisContext& client, const char* format, Args... args) {
    Reply reply(static_cast<redisReply*>(redisCommand(&client, format, args...)), freeReplyObject);
    if (!reply) {
        std::cerr << "Redis 命令失败 " << client.errstr << '\n';
    } else if (reply->type == REDIS_REPLY_ERROR) {
        std::cerr << "Redis 拒绝命令 " << reply->str << '\n';
    }
    return reply;
}

bool textEquals(const Reply& reply, int type, std::string_view expected) {
    return reply && reply->type == type &&
           std::string_view(reply->str, reply->len) == expected;
}
}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "用法 " << argv[0] << " <config.local.json>\n";
        return 2;
    }
    try {
        const auto config = loadDefaultClient(argv[1], "redis_clients");
        const auto host = config.get("host", "127.0.0.1").asString();
        const auto port = config.get("port", 6379).asInt();
        const auto database = config.get("db", 0).asInt();
        const auto password = config.get("passwd", "").asString();
        if (host.empty() || port <= 0 || port > 65535 || database < 0) {
            throw std::runtime_error("Redis 地址、端口或数据库编号无效");
        }

        const timeval timeout{5, 0};
        std::unique_ptr<redisContext, decltype(&redisFree)> client(
            redisConnectWithTimeout(host.c_str(), port, timeout), redisFree);
        if (!client) {
            throw std::runtime_error("Redis 客户端初始化失败");
        }
        if (client->err || redisSetTimeout(client.get(), timeout) != REDIS_OK) {
            throw std::runtime_error(client->errstr);
        }
        if (!password.empty() &&
            !textEquals(command(*client, "AUTH %b", password.data(), password.size()), REDIS_REPLY_STATUS, "OK")) {
            return EXIT_FAILURE;
        }
        if (!textEquals(command(*client, "SELECT %d", database), REDIS_REPLY_STATUS, "OK") ||
            !textEquals(command(*client, "PING"), REDIS_REPLY_STATUS, "PONG")) {
            return EXIT_FAILURE;
        }

        const auto now = std::chrono::system_clock::now().time_since_epoch().count();
        const std::string key = "cloudharbor:test:redis:" + std::to_string(getpid()) + ":" + std::to_string(now);
        const char* value = "cloudharbor-test";
        if (!textEquals(command(*client, "SET %s %s NX EX 60", key.c_str(), value), REDIS_REPLY_STATUS, "OK")) {
            std::cerr << "Redis 写入测试键失败\n";
            return EXIT_FAILURE;
        }
        const bool matches = textEquals(command(*client, "GET %s", key.c_str()), REDIS_REPLY_STRING, value);
        const auto removed = command(*client, "DEL %s", key.c_str());
        if (!matches || !removed || removed->type != REDIS_REPLY_INTEGER || removed->integer != 1) {
            std::cerr << "Redis 读回或删除测试键失败，测试键设置了 60 秒过期时间\n";
            return EXIT_FAILURE;
        }
        const auto absent = command(*client, "GET %s", key.c_str());
        if (!absent || absent->type != REDIS_REPLY_NIL) {
            std::cerr << "Redis 测试键删除后状态异常\n";
            return EXIT_FAILURE;
        }
        std::cout << "Redis PING、SET、GET、DEL 均通过\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "Redis 测试失败 " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
