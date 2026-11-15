#include <sodium.h>

#include <cstdlib>
#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "用法 " << argv[0] << " <测试密码>\n";
        return 2;
    }

    if (sodium_init() < 0) {
        std::cerr << "libsodium 初始化失败\n";
        return EXIT_FAILURE;
    }

    const std::string_view password(argv[1]);
    char hash[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hash, password.data(), password.size(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        std::cerr << "密码哈希生成失败\n";
        return EXIT_FAILURE;
    }

    std::cout << "Hash: " << hash << '\n';
    if (crypto_pwhash_str_verify(hash, password.data(), password.size()) != 0) {
        std::cerr << "密码匹配失败\n";
        return EXIT_FAILURE;
    }

    std::cout << "密码匹配成功\n";
    return EXIT_SUCCESS;
}
