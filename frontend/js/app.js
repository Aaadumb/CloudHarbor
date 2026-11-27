import { initFileBrowser, getCurrentDirectory, refreshAfterUpload } from "./features/file-browser.js?v=20261127-2";
import { initUpload } from "./features/upload.js?v=20261125-4";
import { initSettings } from "./features/setting.js";

const $=selector=>document.querySelector(selector);
let redirecting=false;
function redirectToLogin(){
    if(redirecting)return;
    redirecting=true;
    location.assign("/login.html");
}
function showView(view){
    document.querySelectorAll("[data-view]").forEach(button=>{
        if(button.dataset.view===view)button.setAttribute("aria-current","page");
        else button.removeAttribute("aria-current");
    });
    for(const name of ["home","uploads","settings"])$("#view-"+name).hidden=name!==view;
}
async function initApp(){
    window.addEventListener("session-expired",redirectToLogin);
    window.addEventListener("pageshow",event=>{if(event.persisted)location.reload();});
    $(".sidebar").addEventListener("click",event=>{
        const button=event.target.closest("[data-view]");
        if(button)showView(button.dataset.view);
    });
    document.querySelectorAll("[data-close]").forEach(button=>button.addEventListener("click",()=>{
        document.getElementById(button.dataset.close).close();
    }));
    const loadError=document.createElement("div");
    loadError.className="app-load-error";
    loadError.hidden=true;
    loadError.setAttribute("role","alert");
    const errorTitle=document.createElement("h1");
    errorTitle.textContent="页面加载失败";
    const errorMessage=document.createElement("p");
    errorMessage.className="muted";
    const retry=document.createElement("button");
    retry.type="button";
    retry.className="btn btn-primary";
    retry.textContent="重新加载";
    loadError.append(errorTitle,errorMessage,retry);
    $(".main").append(loadError);
    retry.addEventListener("click",()=>location.reload());
    document.querySelectorAll(".toolbar button").forEach(button=>button.disabled=true);
    try{
        const profile=await initSettings({onLogout:redirectToLogin});
        document.querySelectorAll(".toolbar button").forEach(button=>button.disabled=false);
        await initFileBrowser(profile.rootId);
        initUpload({rootId:profile.rootId,getCurrentDirectory,onUploadComplete:refreshAfterUpload,showView});
        $(".user-button").addEventListener("click",()=>showView("settings"));
    }catch(error){
        if(redirecting)return;
        document.querySelectorAll(".main>section").forEach(section=>section.hidden=true);
        document.querySelectorAll(".sidebar button").forEach(button=>button.disabled=true);
        errorMessage.textContent=error.message;
        loadError.hidden=false;
    }
}
initApp();
