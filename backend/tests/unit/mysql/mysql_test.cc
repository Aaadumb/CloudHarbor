#include "../test_config.h"

#include <mariadb/mysql.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {
bool runQuery(MYSQL* client, const char* sql, const char* label, bool expectOne = false) {
    if (mysql_query(client, sql) != 0) {
        std::cerr << label << "失败 " << mysql_error(client) << '\n';
        return false;
    }
    std::unique_ptr<MYSQL_RES, decltype(&mysql_free_result)> result(
        mysql_store_result(client), mysql_free_result);
    if (!result) {
        std::cerr << label << "读取结果失败 " << mysql_error(client) << '\n';
        return false;
    }
    if (expectOne) {
        MYSQL_ROW row = mysql_fetch_row(result.get());
        if (!row || !row[0] || std::string(row[0]) != "1") {
            std::cerr << "SELECT 1 返回值不符合预期\n";
            return false;
        }
    }
    std::cout << label << "通过\n";
    return true;
}
}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "用法 " << argv[0] << " <config.local.json>\n";
        return 2;
    }
    try {
        const auto config = loadDefaultClient(argv[1], "db_clients");
        if (config.get("rdbms", "").asString() != "mysql") {
            throw std::runtime_error("default 数据库客户端的 rdbms 必须为 mysql");
        }
        const auto host = config.get("host", "127.0.0.1").asString();
        const auto port = config.get("port", 3306).asUInt();
        const auto user = config["user"].asString();
        const auto password = config.get("passwd", "").asString();
        const auto database = config["dbname"].asString();
        if (host.empty() || user.empty() || database.empty() || port == 0 || port > 65535) {
            throw std::runtime_error("MySQL 地址、端口、用户名或数据库名无效");
        }

        std::unique_ptr<MYSQL, decltype(&mysql_close)> client(mysql_init(nullptr), mysql_close);
        if (!client) {
            throw std::runtime_error("MySQL 客户端初始化失败");
        }
        const unsigned int timeout = 5;
        const unsigned int protocol = MYSQL_PROTOCOL_TCP;
        if (mysql_options(client.get(), MYSQL_OPT_PROTOCOL, &protocol) != 0 ||
            mysql_options(client.get(), MYSQL_OPT_CONNECT_TIMEOUT, &timeout) != 0 ||
            mysql_options(client.get(), MYSQL_OPT_READ_TIMEOUT, &timeout) != 0 ||
            mysql_options(client.get(), MYSQL_OPT_WRITE_TIMEOUT, &timeout) != 0) {
            throw std::runtime_error(mysql_error(client.get()));
        }
        if (!mysql_real_connect(client.get(), host.c_str(), user.c_str(), password.c_str(),
                                database.c_str(), port, nullptr, 0)) {
            std::cerr << "MySQL 连接失败 " << mysql_error(client.get()) << '\n';
            return EXIT_FAILURE;
        }
        if (!runQuery(client.get(), "SELECT 1", "MySQL 连通性检查", true) ||
            !runQuery(client.get(), "SELECT id, name, mail, password, root_id FROM user LIMIT 0", "user 表检查") ||
            !runQuery(client.get(), "SELECT id, name, type, owner, father, size, last_modify, path FROM file LIMIT 0",
                      "file 表检查")) {
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "MySQL 测试失败 " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
