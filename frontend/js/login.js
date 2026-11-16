import { postJson } from "./api/http.js";
import { sendRegisterCode,login,register } from "./api/auth.js";
const cookieDialog=document.querySelector(".cookie-notice")
const authTabs=document.querySelectorAll(".auth-tab")
const authentication=document.querySelector(".authentication")
const loginForm=document.querySelector('.form-login')
const registerForm=document.querySelector('.form-register')
const registerError=document.querySelector('#register-error')
const confirmPassword=document.querySelector('#register-confirm')
const password=document.querySelector('#register-password')
const sendCodeBtn=registerForm.querySelector(".code-button")
const mailInput=document.querySelector("#register-email")
function initLoginPage(){
    initCookieNotice()
    cookieDialog.addEventListener("click",event=>{
        const target=event.target.closest("button")
        if(!target)return
        const choice=target.dataset.choice
        saveCookiePreference(choice)
        cookieDialog.close()
    })
    authTabs.forEach((tab,index)=>{
        tab.addEventListener("click",()=>selectMode(index===0?"login":"register"))
        tab.addEventListener("keydown",event=>{
            if(!["ArrowLeft","ArrowRight","Home","End"].includes(event.key))return
            event.preventDefault()
            const nextIndex=event.key==="Home"?0:event.key==="End"?1:1-index
            selectMode(nextIndex===0?"login":"register",true)
        })
    });
    [password,confirmPassword].forEach(input=>{
        input.addEventListener('input',()=>{
            registerError.hidden=true
        })
    })
    loginForm.addEventListener("invalid", hideValidationBubble, true);
    registerForm.addEventListener("invalid", hideValidationBubble, true);
    loginForm.addEventListener('submit',handleLoginSubmit)
    registerForm.addEventListener('submit',handleRegisterSubmit)
    sendCodeBtn.addEventListener('click',handleSendCode)
}
function hideValidationBubble(event) {
    event.preventDefault();
}
function selectMode(mode,moveFocus=false){
    const nextTab=mode==="login"?authTabs[0]:authTabs[1]
    if(authentication.dataset.mode===mode){
        if(moveFocus)nextTab.focus()
        return
    }
    authentication.dataset.mode=mode
    loginForm.hidden=mode!=="login"
    registerForm.hidden=mode!=="register"
    authTabs.forEach(tab=>{
        const selected=tab===nextTab
        tab.setAttribute("aria-selected",String(selected))
        tab.tabIndex=selected?0:-1
    })
    if(moveFocus)nextTab.focus()
}
async function handleLoginSubmit(event){
    event.preventDefault()
    const form=event.currentTarget
    const submitButton=form.querySelector('button[type="submit"]')
    if(submitButton.disabled)return
    const fileds=new FormData(form)
    const auth=fileds.get('account').trim()
    const pwd=fileds.get('password')
    if(!auth || !pwd){
        // 弹窗提示字段不能为空
        return
    }
    submitButton.disabled=true
    try{
        await login({auth,pwd})
        window.location.assign("/index.html")
    }catch(error){
        // 显示错误信息
    }finally{
        submitButton.disabled=false
    }

}
async function handleRegisterSubmit(event){
    event.preventDefault()
    const form=event.currentTarget
    const submitButton=form.querySelector('button[type="submit"]')
    if(submitButton.disabled)return
    const fileds=new FormData(form)
    const name=fileds.get('username')
    const mail=fileds.get('email')
    const code=fileds.get('code')
    const pwd=fileds.get('password')
    const confirmPwd=fileds.get('confirmPassword')
    if(pwd!==confirmPwd){
        registerError.textContent="两次输入的密码不一致,请重新确认"
        registerError.hidden=false
        confirmPassword.reportValidity()
        confirmPassword.focus()
        confirmPassword.value=""
        return
    }
    submitButton.disabled=true
    try{
        await register({name,mail,code,pwd})
        // TODO: 根据返回的 data.loggedIn 决定跳转。自动登录失败时切回登录表单。
        window.location.assign("/index.html")
    }catch(error){
        //显示错误信息
    }finally{
        submitButton.disabled=false
    }

}
async function handleSendCode(){
    // 先拿到邮箱内容
    if(sendCodeBtn.disabled)return
    if(!mailInput.checkValidity()){
        return
    }
    const mail=mailInput.value.trim()
    sendCodeBtn.disabled=true
    try{
        await sendRegisterCode(mail)
        // toast
    }catch(error){
        // toast
    }finally{
        sendCodeBtn.disabled=false
    }
}
function initCookieNotice(){
    const cookie=readCookiePreference()
    if(cookie===null){
        cookieDialog.show()
    }
}
function readCookiePreference(){
    const prefix="cookiePreference="
    const cookie=document.cookie
        .split(';')
        .map(item=>item.trim())
        .find(item=>item.startsWith(prefix))
    if(!cookie)return null
    const value=cookie.slice(prefix.length)
    return value==="accept"||value==="reject"?value:null;
}
function saveCookiePreference(choice){
    if(choice!=="accept"&&choice!=="reject")return false
    const secure=location.protocol==="https:"?"; Secure":"";
    document.cookie=
        `cookiePreference=${choice}; Path=/; Max-Age=2592000; SameSite=Lax${secure}`
    return readCookiePreference()===choice
}
initLoginPage()
