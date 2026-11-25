async function Fetch(request,{timeoutMs,signal:externalSignal}) {
    const timeout=AbortSignal.timeout(timeoutMs);
    const signal=externalSignal ? AbortSignal.any([timeout,externalSignal]) : timeout;
    let response,result;
    try{
        response=await fetch(request,{signal});
        result=await response.json();
    }catch(error){
        if(externalSignal?.aborted)throw externalSignal.reason;
        if(timeout.aborted){
            throw new Error("请求超时 请稍后查看结果");
        }
        if(error instanceof SyntaxError){
            const message=response.ok
                ?"服务器返回了无法解析的数据"
                :`请求失败(http ${response.status})`
            throw new Error(message,{cause:error})
        }
        throw new Error("网络请求失败,请稍后再试",{cause:error})
    }
    if(!response.ok||result?.code!=="OK"){
        const error=new Error(result?.message||"请求失败")
        if(response.status===401)window.dispatchEvent(new Event("session-expired"));
        error.status=response.status
        error.code=result?.code;
        throw error;
    }
    return result
}
export async function postJson(url,body, {timeoutMs=15000,signal}={}) {
    const req=new Request(url,{
        method:"POST",
        headers:{"Content-Type":"application/json"},
        body:JSON.stringify(body)
    })
    return Fetch(req,{timeoutMs,signal});
}

export async function postFormData(url,formdata,{timeoutMs=15000,signal}={}){
    const req=new Request(url,{
        method:"POST",
        body:formdata
    })
    return Fetch(req,{timeoutMs,signal});
}

export async function getFetch(url,{timeoutMs=15000,signal,cache}={}){
    const req=new Request(url,{method:"GET",cache});
    return Fetch(req,{timeoutMs,signal});
}

export async function deleteFetch(url,body,{timeoutMs=15000,signal}={}) {
    const req=new Request(url,{
        method:"DELETE",
        headers:{"Content-Type":"application/json"},
        body:JSON.stringify(body)
    });
    return Fetch(req,{timeoutMs,signal});
}
