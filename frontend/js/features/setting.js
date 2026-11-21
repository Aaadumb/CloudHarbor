import { getCurrentUser, updateAvatar, updateName, updatePassword, updateMail } from "../api/users.js";
import { sendRegisterCode, logout } from "../api/auth.js";
import { showToast } from "../ui/toast.js";
const $=selector=>document.querySelector(selector);
let profile,editKind,editing=false,avatarBusy=false;
let onLogout;
function notify(message,status="failure"){
    showToast($(".toast-region"),message,status);
}
export async function initSettings(options){
    onLogout=options.onLogout;
    profile=(await getCurrentUser()).data;
    if(typeof profile.rootId!=="string" || !/^[1-9][0-9]*$/.test(profile.rootId))throw new Error("服务器返回的根目录编号无效");
    renderProfile();
    $("#avatar-picker").addEventListener("click",()=>{if(!avatarBusy)$("#avatar-input").click();});
    $("#avatar-input").addEventListener("change",async ()=>{
        const file=$("#avatar-input").files[0];
        $("#avatar-input").value="";
        if(!file)return;
        if(!["image/png","image/jpeg"].includes(file.type)||file.size>2*1024*1024){notify("请选择不超过 2 MiB 的 PNG/JPEG 头像");return;}
        avatarBusy=true;$("#avatar-picker").disabled=true;
        const url=URL.createObjectURL(file);
        const image=new Image();
        try{
            image.src=url;
            await image.decode();
            if(image.width>4096 || image.height>4096)throw new Error("头像尺寸过大");
            $("#default-avatar").src=url;
            await updateAvatar(file);
            await loadProfile();
            notify("头像已更新","success");
        }catch(error){
            renderProfile();
            notify(error.message);
        }finally{
            URL.revokeObjectURL(url);
            avatarBusy=false;$("#avatar-picker").disabled=false;
        }
    });
    document.querySelectorAll("[data-edit]").forEach(button=>button.addEventListener("click",()=>openProfileEditor(button.dataset.edit)));
    $("#edit-form").addEventListener("submit",submitProfileEdit);
    $("#edit-form").addEventListener("input",()=>{
        $("#edit-form").querySelectorAll("input").forEach(input=>input.setCustomValidity(""));
    });
    $("#edit-dialog").addEventListener("cancel",event=>{if(editing)event.preventDefault();});
    $("#edit-dialog").addEventListener("close",()=>{
        if($("#edit-dialog").open)return;
        $("#edit-form").reset();$("#edit-fields").replaceChildren();
    });
    $("#logout").addEventListener("click",async ()=>{
        if(!window.confirm("确认退出当前账号？"))return;
        $("#logout").disabled=true;
        try{await logout();onLogout();}
        catch(error){notify(error.message);$("#logout").disabled=false;}
    });
    return profile;
}
async function loadProfile(){
    profile=(await getCurrentUser()).data;
    renderProfile();
}
function renderProfile(){
    $(".user-button>span:not(.avatar)").textContent=profile.name;
    $(".user-button").setAttribute("aria-label",profile.name+"，打开个人设置");
    const values=document.querySelectorAll(".profile-value");
    values[0].textContent=profile.name;
    values[1].textContent=profile.mail;
    const url="/account/avatar?v="+Date.now();
    document.querySelectorAll(".avatar img").forEach(image=>{
        image.onerror=()=>{image.onerror=null;image.src="assets/images/default-avatar.png";};
        image.src=url;
    });
}
function addField(label,name,type="text",value=""){
    const wrapper=document.createElement("label");
    wrapper.className="form-field";
    wrapper.textContent=label;
    const input=document.createElement("input");
    input.className="field-input";input.name=name;input.type=type;input.value=value;input.required=true;
    if(type==="password")input.autocomplete=name==="oldPwd"?"current-password":"new-password";
    if(name.endsWith("Code")){input.pattern="[0-9]{6}";input.maxLength=6;input.inputMode="numeric";}
    wrapper.append(input);
    $("#edit-fields").append(wrapper);
    return input;
}
function codeButton(text,getMail){
    const button=document.createElement("button");
    button.type="button";button.className="btn";button.textContent=text;
    button.addEventListener("click",async ()=>{
        const mail=getMail();
        if(!mail.value || !mail.reportValidity())return;
        button.disabled=true;
        try{await sendRegisterCode(mail.value);notify("验证码已发送","success");}
        catch(error){notify(error.message);}
        finally{button.disabled=false;}
    });
    $("#edit-fields").append(button);
}
function openProfileEditor(kind){
    if(editing)return;
    editKind=kind;
    $("#edit-fields").replaceChildren();
    $("#edit-title").textContent={name:"修改用户名",mail:"修改邮箱",password:"修改密码"}[kind];
    if(kind==="name")addField("新用户名","name","text",profile.name).maxLength=50;
    if(kind==="password"){
        addField("当前密码","oldPwd","password");
        addField("新密码","newPwd","password");
        addField("确认新密码","confirmPwd","password");
    }
    if(kind==="mail"){
        const oldMail=addField("当前邮箱","oldMail","email",profile.mail);
        oldMail.readOnly=true;
        const newMail=addField("新邮箱","newMail","email");
        codeButton("发送当前邮箱验证码",()=>oldMail);
        addField("当前邮箱验证码","oldCode");
        codeButton("发送新邮箱验证码",()=>newMail);
        addField("新邮箱验证码","newCode");
    }
    $("#edit-dialog").showModal();
}
async function submitProfileEdit(event){
    event.preventDefault();
    if(editing)return;
    const form=$("#edit-form");
    const values=Object.fromEntries(new FormData(form));
    if(editKind==="password" && values.newPwd!==values.confirmPwd){
        form.elements.confirmPwd.setCustomValidity("两次密码不一致");form.reportValidity();return;
    }
    if(!form.reportValidity())return;
    editing=true;
    const buttons=[...$("#edit-dialog").querySelectorAll("button")];
    buttons.forEach(button=>button.disabled=true);
    try{
        if(editKind==="name")await updateName(values.name);
        if(editKind==="password")await updatePassword(values.oldPwd,values.newPwd);
        if(editKind==="mail")await updateMail(values.oldMail,values.newMail,values.oldCode,values.newCode);
        $("#edit-dialog").close();
        notify("资料已更新","success");
        await loadProfile();
    }catch(error){notify(error.message);}
    finally{editing=false;buttons.forEach(button=>button.disabled=false);}
}
