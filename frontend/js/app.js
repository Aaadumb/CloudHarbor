import { initFileBrowser } from "./features/file-browser.js";

const $ = selector => document.querySelector(selector);
const nav=document.querySelector('.sidebar')
const navBtns=document.querySelector('.sidebar button')
function initApp(){
    
    nav.addEventListener('click',showView)
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
