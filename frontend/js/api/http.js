export async function postJson(url,body, {timeoutMs=15000}={}) {
    const requestBody=JSON.stringify(body)
    const signal=AbortSignal.timeout(timeoutMs)
    let response,result
    try{
        response=await fetch(url,{
            method:"POST",
            headers:{"Content-Type":"application/json"},
            body:requestBody,
            signal
        });
        result=await response.json();
    }catch(error){
        if(signal.aborted){
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
        error.status=response.status
        error.code=result?.code;
        throw error;
    }
    return result
}