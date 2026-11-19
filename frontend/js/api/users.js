import { getFetch,postFormData,postJson } from "./http.js";
export function getCurrentUser(){
    return getFetch('/account/me');
}

export function updateAvatar(file){
    const fd=new FormData();
    fd.set('file',file);
    return postFormData('/account/avatar',fd);
}

export function updateName(name){
    return postJson('/account/name',{name});
}

export function updatePassword(oldPwd,newPwd){
    return postJson('/account/password',{oldPwd,newPwd});
}

export function updateMail(oldMail,newMail,oldCode,newCode){
    return postJson('/account/mail',{oldMail,newMail,oldCode,newCode});
}

export function getAvatar(){
    return getFetch('/account/avatar');
}