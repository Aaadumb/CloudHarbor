#include <curl/curl.h>

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <regex>
#include <string>

namespace {
constexpr char kSmtpUrl[] = "smtps://smtp.qq.com:465";

bool validAddress(const std::string& address) {
    static const std::regex pattern(
        R"(^[A-Za-z0-9.!#$%&'*+/=?^_`{|}~-]{1,64}@)"
        R"([A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?)"
        R"((\.[A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?)+$)");
    return address.size() <= 254 && std::regex_match(address, pattern);
}

using CurlList = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;

bool append(CurlList& list, const char* value) {
    curl_slist* updated = curl_slist_append(list.get(), value);
    if (!updated) {
        return false;
    }
    list.release();
    list.reset(updated);
    return true;
}

CURLcode sendMail(const std::string& address, const char* smtpUser, const char* password, char* errorBuffer) {
    const std::string from = std::string("<") + smtpUser + ">";
    const std::string to = "<" + address + ">";
    const std::string fromHeader = "From: " + from;
    const std::string toHeader = "To: " + to;
    char dateHeader[96];
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
    if (!localtime_r(&now, &localTime) ||
        std::strftime(dateHeader, sizeof(dateHeader), "Date: %a, %d %b %Y %H:%M:%S %z", &localTime) == 0) {
        return CURLE_FAILED_INIT;
    }

    CurlList headers(nullptr, curl_slist_free_all);
    CurlList recipients(nullptr, curl_slist_free_all);
    if (!append(headers, dateHeader) ||
        !append(headers, fromHeader.c_str()) ||
        !append(headers, toHeader.c_str()) ||
        !append(headers, "Subject: Your verification code") ||
        !append(recipients, to.c_str())) {
        return CURLE_OUT_OF_MEMORY;
    }

    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) {
        return CURLE_FAILED_INIT;
    }
    std::unique_ptr<curl_mime, decltype(&curl_mime_free)> mime(curl_mime_init(curl.get()), curl_mime_free);
    if (!mime) {
        return CURLE_OUT_OF_MEMORY;
    }
    curl_mimepart* part = curl_mime_addpart(mime.get());
    if (!part) {
        return CURLE_OUT_OF_MEMORY;
    }

    CURLcode result = CURLE_OK;
    auto setOption = [&](CURLoption option, auto value) {
        if (result == CURLE_OK) {
            result = curl_easy_setopt(curl.get(), option, value);
        }
    };

    result = curl_mime_data(part, "您的验证码为:123456,5 分钟内有效", CURL_ZERO_TERMINATED);
    if (result == CURLE_OK) {
        result = curl_mime_type(part, "text/plain; charset=UTF-8");
    }
    if (result == CURLE_OK) {
        result = curl_mime_encoder(part, "base64");
    }

    setOption(CURLOPT_ERRORBUFFER, errorBuffer);
    setOption(CURLOPT_URL, kSmtpUrl);
    setOption(CURLOPT_USERNAME, smtpUser);
    setOption(CURLOPT_PASSWORD, password);
    setOption(CURLOPT_LOGIN_OPTIONS, "AUTH=LOGIN");
    setOption(CURLOPT_MAIL_FROM, from.c_str());
    setOption(CURLOPT_MAIL_RCPT, recipients.get());
    setOption(CURLOPT_HTTPHEADER, headers.get());
    setOption(CURLOPT_MIMEPOST, mime.get());
    setOption(CURLOPT_NOSIGNAL, 1L);
    setOption(CURLOPT_CONNECTTIMEOUT, 10L);
    setOption(CURLOPT_TIMEOUT, 30L);

    return result == CURLE_OK ? curl_easy_perform(curl.get()) : result;
}
}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "用法 " << argv[0] << " <收件人邮箱>\n";
        return 2;
    }
    const std::string address = argv[1];
    if (!validAddress(address)) {
        std::cerr << "收件人邮箱格式错误\n";
        return 2;
    }
    const char* smtpUser = std::getenv("SMTP_USER");
    if (!smtpUser || *smtpUser == '\0') {
        std::cerr << "请在当前终端设置并导出 SMTP_USER（发件邮箱）\n";
        return 2;
    }
    const char* password = std::getenv("SMTP_PASS");
    if (!password || *password == '\0') {
        std::cerr << "请在当前终端设置并导出 SMTP_PASS（SMTP 授权码）\n";
        return 2;
    }
    const CURLcode initResult = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (initResult != CURLE_OK) {
        std::cerr << "libcurl 初始化失败 " << curl_easy_strerror(initResult) << '\n';
        return EXIT_FAILURE;
    }
    char errorBuffer[CURL_ERROR_SIZE]{};
    const CURLcode result = sendMail(address, smtpUser, password, errorBuffer);
    curl_global_cleanup();
    if (result != CURLE_OK) {
        std::cerr << "发送失败 (" << result << ") "
                  << (errorBuffer[0] ? errorBuffer : curl_easy_strerror(result)) << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "SMTP 服务器已接受发往 " << address << " 的测试邮件\n";
    return EXIT_SUCCESS;
}
