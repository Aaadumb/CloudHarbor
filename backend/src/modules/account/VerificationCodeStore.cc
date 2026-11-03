#include "VerificationCodeStore.h"
namespace account::mail{
    bool CreateCode(std::string &out,int len){
        out.clear();
        while(out.size()<len){
            unsigned char tmp;
            ssize_t res=getrandom(&tmp,sizeof tmp,0);
            if(res<0){
                if(errno==EINTR)continue;
                out.clear();
                return false;
            }
            if(tmp<250){
                // 大于等于250拒绝采样 结果更加随机
                out+=static_cast<char>('0'+tmp%10);
            }
        }
        return true;
    }
}