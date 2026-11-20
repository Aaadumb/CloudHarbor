import { initFileBrowser } from "./features/file-browser.js";
import { showToast } from "./ui/toast.js";
const $ = selector => document.querySelector(selector);
const $$=selector=> document.querySelectorAll(selector);
const nav=document.querySelector('.sidebar')
const navBtns=document.querySelector('.sidebar button')
function initApp(){
    // showToast($('.toast-region'),'哦鲸鲸',"success");
    nav.addEventListener('click',showView)
    $$('.close-button').forEach(item=>item.addEventListener('click',()=>{
        $(`#${item.dataset.close}`).close();
    }));
    $$('.dialog-footer button[data-close]').forEach(item=>item.addEventListener('click',()=>{
        $(`#${item.dataset.close}`).close();
    }));
    initFileBrowser()
}

function showView(event){
    const target=event.target.closest('button')
    const origin=$('.sidebar button[aria-current="page"]')
    if(!target || target==origin)return
    

    $(`#view-${origin.dataset.view}`).setAttribute('hidden','')
    $('.sidebar button[aria-current="page"]').removeAttribute("aria-current")
    $(`#view-${target.dataset.view}`).removeAttribute('hidden')
    target.setAttribute("aria-current","page")
}

function redirectToLogin(){

}
initApp()
