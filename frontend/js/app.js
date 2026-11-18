import { createFileElement } from "./features/file-browser.js";

const $ = selector => document.querySelector(selector);
const nav=document.querySelector('.sidebar')
const navBtns=document.querySelector('.sidebar button')
function initApp(){
    nav.addEventListener('click',showView)
    $('#file-list').append(createFileElement({ id: "103", parent: "100", name: "需求分析.md", type: "file", size: 12288, lastModified: "10-18 12:00" }))
    $('#file-list').append(createFileElement( { id: "101", parent: "100", name: "学习资料", type: "folder" }))
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