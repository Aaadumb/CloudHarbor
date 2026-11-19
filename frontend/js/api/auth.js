import { postJson } from "./http.js";
export function login({auth,pwd}) {
    return postJson("/account/login",{auth,pwd});
}

export function register({name,mail,code,pwd}) {
    return postJson("/account/register",{name,mail,code,pwd})
}
export function sendRegisterCode(mail){
    return postJson("/account/send_code",{mail},{timeoutMs:40_000})
}
export function logout(){
    // 向服务器发送请求
    return postJson('/account/logout',{});
    
}