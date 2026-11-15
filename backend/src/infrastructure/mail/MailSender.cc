#include "MailSender.h"
#include <cstdlib>
#include <exception>

namespace {
std::string readEnv(const char* name){
    const char* value=std::getenv(name);
    return value ? value : "";
}
}

MailAwaiter::MailAwaiter(const std::string& m,const std::string& c):
    mail(m),code(c),smtpUser(readEnv("SMTP_USER")),smtpPassword(readEnv("SMTP_PASS")){};
bool MailAwaiter::sendCodeBlock(){
    if(smtpUser.empty() || smtpPassword.empty())return false;
    CURL *curl=curl_easy_init();
    if(!curl)return false;

    char date[64],h_date[80],from_addr[320],to_addr[320],body[256],h_to[340],h_from[340];
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now,&tmv);                                            
    strftime(date, sizeof date, "%a, %d %b %Y %H:%M:%S %z", &tmv);
    snprintf(h_date,sizeof h_date,"Date: %s",date);                     
    snprintf(from_addr,sizeof from_addr,"<%s>",smtpUser.c_str());
    snprintf(to_addr,sizeof to_addr,"<%s>",mail.c_str());
    snprintf(h_to,sizeof h_to,"To: %s",to_addr);
    snprintf(h_from,sizeof h_from,"From: %s",from_addr);
    snprintf(body,sizeof body,"您的验证码为:%s,5 分钟内有效",code.c_str());

    // 邮件头
    curl_slist *header=nullptr;
    const char *header_lines[]={h_date,h_to,h_from,"Subject: Your verification code"};
    for(const char *line:header_lines){
        curl_slist *next=curl_slist_append(header,line);
        if(!next){
            curl_slist_free_all(header);
            curl_easy_cleanup(curl);
            return false;
        }
        header=next;
    }

    // 邮件正文
    curl_mime *mime=curl_mime_init(curl);
    curl_mimepart *part=curl_mime_addpart(mime);
    curl_mime_data(part,body,CURL_ZERO_TERMINATED);
    curl_mime_type(part,"text/plain; charset=UTF-8");
    curl_mime_encoder(part,"base64");

    // 信封收件人
    curl_slist *rcpt=curl_slist_append(NULL,to_addr);
    if(!rcpt){
        curl_slist_free_all(header);
        curl_mime_free(mime);
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl,CURLOPT_URL,SMTP_URL);
    curl_easy_setopt(curl,CURLOPT_USERNAME,smtpUser.c_str());
    curl_easy_setopt(curl,CURLOPT_PASSWORD,smtpPassword.c_str());
    curl_easy_setopt(curl,CURLOPT_LOGIN_OPTIONS,"AUTH=LOGIN");
    curl_easy_setopt(curl,CURLOPT_MAIL_FROM,from_addr);
    curl_easy_setopt(curl,CURLOPT_MAIL_RCPT,rcpt);
    curl_easy_setopt(curl,CURLOPT_HTTPHEADER,header);
    curl_easy_setopt(curl,CURLOPT_MIMEPOST,mime);
    curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,10L);
    curl_easy_setopt(curl,CURLOPT_TIMEOUT,30L);

    CURLcode rc=curl_easy_perform(curl);
    curl_slist_free_all(rcpt);
    curl_slist_free_all(header);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);
    return rc==CURLE_OK;
}
void MailAwaiter::await_suspend(std::coroutine_handle<> handle){
    auto* loop=trantor::EventLoop::getEventLoopOfCurrentThread();
    bool success=ThreadPool::getInstance().enqueue([this,handle,loop](){
        bool res=false;
        std::exception_ptr error;
        try{
            res=sendCodeBlock();
        }catch(...){
            error=std::current_exception();
        }
        loop->queueInLoop([this,handle,res,error](){
            if(error){
                setException(error);
            }else{
                setValue(res);
            }
            handle.resume();
        });
    });
    if(!success){
        setValue(false);
        handle.resume();
    }
}
Task<bool> MailSender::sendCode(const std::string& mail,const std::string& code){
    co_return co_await MailAwaiter{mail,code};
}

MailSender& MailSender::getInstance(){
    static MailSender instance;
    return instance;
}
bool MailSender::checkMail(const std::string& mail){
    if(mail.empty() || mail.length()>254)return false;
    static const std::regex re(
        R"(^[A-Za-z0-9.!#$%&'*+/=?^_`{|}~-]{1,64}@)"
        R"([A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?)"
        R"((\.[A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?)+$)");
    if (!std::regex_match(mail, re)) return false;
    return true;
}
