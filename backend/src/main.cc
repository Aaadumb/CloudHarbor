#include <drogon/drogon.h>
#include <curl/curl.h>
#include "ApiResponse.h"
#include <sodium.h>
#include <cstdlib>
#include <exception>
#include <functional>
#include <initializer_list>

using namespace drogon;
int main(int argc,char* argv[]){
    if(argc!=2){
        LOG_ERROR<<"Usage: cloudharbor <config.json>";
        return EXIT_FAILURE;
    }
    try{
        app().loadConfigFile(argv[1]);
    }catch(const std::exception& e){
        LOG_ERROR<<"Failed to load config "<<argv[1]<<": "<<e.what();
        return EXIT_FAILURE;
    }
    for(const char* name : {"SMTP_PASS","FDFSCLIENTCONF"}){
        const char* value=std::getenv(name);
        if(!value || *value=='\0'){
            LOG_ERROR<<"Missing environment variable "<<name;
            return EXIT_FAILURE;
        }
    }
    if(sodium_init()<0){
        LOG_ERROR<<"sodium_init failed";
        return EXIT_FAILURE;
    }
    const auto rc=curl_global_init(CURL_GLOBAL_DEFAULT);
    if(rc!=CURLE_OK){
        LOG_ERROR<<"curl_global_init failed: "<<curl_easy_strerror(rc);
        return EXIT_FAILURE;
    }
    app().setCustomErrorHandler(
        [](HttpStatusCode status,const HttpRequestPtr& req){
            if(req && (req->path().starts_with("/account/") ||
                       req->path().starts_with("/files/"))){
                if(status==k500InternalServerError)return response::InternalError();
                return response::Failure(status,"HTTP_ERROR","请求处理失败");
            }

            auto resp=HttpResponse::newHttpResponse();
            resp->setStatusCode(status);
            return resp;
        });
    app().setExceptionHandler(
        [](const std::exception& e,const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& callback){
            LOG_ERROR<<"Request handler failed: "<<e.what();
            callback(app().getCustomErrorHandler()(k500InternalServerError,req));
        });
    app().run();
    return EXIT_SUCCESS;
}
