import { getDownloadUrl, queryDirectory } from "../api/files.js";
import { showToast } from "../ui/toast.js";
const $ = selector => document.querySelector(selector);
const $$=selector=> document.querySelectorAll(selector);
const state = {
    currentDirectoryId:"100",
    breadcrumbs: [
        { id:"100", name:"/"}
    ],
    entries: [
        { id: "103", parent: "100", name: "需求分析.md", type: "file", size: 12288, lastModified: "10-18 12:00" },
        { id: "101", parent: "100", name: "学习资料", type: "folder" }
    ],
    selectedIds: new Set()
}
let directoryRequest = 0;
function formatSize(bytes) {
    if (bytes === undefined) return "—";
    if (bytes < 1024) return bytes + " B";
    if (bytes < 1048576) return (bytes / 1024).toFixed(bytes % 1024 ? 1 : 0) + " KB";
    return (bytes / 1048576).toFixed(1) + " MB";
}
function el(tag,text,className){
    const node=document.createElement(tag)
    if(text!==undefined)node.textContent=text
    if(className)node.className=className
    return node
}
function updateSelection(){
    // 首先更新select-all的状态
    const total=state.entries.length,selected=state.selectedIds.size;
    const selectAll=$('#select-all');
    selectAll.disabled=total===0;
    selectAll.checked=total>0 && selected===total;
    selectAll.indeterminate=selected>0 && selected<total;
    $('#file-count').textContent=`共 ${total} 项`;
    $('#selection-count').textContent=selected ? `已选择 ${selected} 项` : '';
}
export function createFileElement(file){
    const row=el("tr");
    const selectCell=el("td"),checkbox=el("input");
    row.dataset.id=file.id;
    checkbox.type="checkbox";
    checkbox.setAttribute("aria-label","选择"+file.name)
    checkbox.addEventListener("change",()=>{
        const checked=checkbox.checked;
        if(checked){
            state.selectedIds.add(file.id)
            row.classList.add('is-selected')
        }else{
            state.selectedIds.delete(file.id)
            row.classList.remove('is-selected')
        }
        updateSelection()
    })
    selectCell.append(checkbox);
    row.append(selectCell)

    const nameCell=el('td')
    const nameWrap=el('div',undefined,'file-name')
    const fileImg=el('img',undefined,'icon');
    let fileName;
    fileImg.setAttribute('aria-hidden',"true")
    if(file.type==='folder'){
        fileImg.src="assets/icons/folder.svg"
        fileName=el('button',file.name,"entry-label folder-link")
        fileName.type='button'
        fileName.title=file.name
        fileName.addEventListener('click',async ()=>{
            await loadDirectory(file.id,[...state.breadcrumbs,{id:file.id,name:file.name}]);
        })
    }else{
        fileImg.src="assets/icons/file.svg"
        fileName=el('span',file.name,'entry-label')
        fileName.title=file.name
    }
    nameWrap.append(fileImg)
    nameWrap.append(fileName)
    nameCell.append(nameWrap)
    row.append(nameCell)

    row.append(el('td',file.type==='folder'?'—':formatSize(file.size)))
    row.append(el('td',file.type==='folder'?'文件夹':'文件','type-cell'))
    row.append(el('td',file.type==='folder'?'—':file.lastModified,'time-cell'))

    const actionCell=el('td',undefined,'row-actions')
    const downloadBtn=el('button','下载','text-button direct-action')
    const deleteBtn=el('button','删除','text-button direct-action')
    const moreBtn=el('button',undefined,'text-button more-button')
    const moreImg=el('img',undefined,'icon')
    downloadBtn.type=deleteBtn.type=moreBtn.type='button'
    downloadBtn.setAttribute('aria-label','下载'+file.name)
    deleteBtn.setAttribute('aria-label','删除'+file.name)
    moreBtn.setAttribute('aria-label',file.name+'的更多操作')
    moreImg.src='assets/icons/more.svg'
    moreImg.setAttribute('aria-hidden','true')
    moreBtn.append(moreImg)
    downloadBtn.addEventListener(async()=>{
        if(file.type==="folder"){
            showToast($('.toast-region'),"暂不支持下载文件夹","failure");
            return;
        }
        let url;
        try{
            url=await getDownloadUrl(file.id).url;
        }catch(error){
            showToast($('.toast-region'),error.message,"failure");
            return;
        }
        const a=el('a');
        a.href=url;
        document.body.append(a);
        a.click();
        a.remove();
    });
    deleteBtn.addEventListener(()=>{

    });
    moreBtn.addEventListener(()=>{

    });

    actionCell.append(downloadBtn,deleteBtn,moreBtn)
    row.append(actionCell)
    return row
}
export function initFileBrowser(){
    $('#select-all').addEventListener('click',()=>{
        const checked=$('#select-all').checked
        // console.log($$('#file-list tr'))
        if(checked){
            // 如果被选中 file-list下所有的checkbox都要设置为选中 并且要更新set
            state.selectedIds.clear()
            $$('#file-list input').forEach(checkbox=>{ checkbox.checked=true} );
            $$('#file-list tr').forEach( tr => {tr.classList.add('is-selected');state.selectedIds.add(tr.dataset.id)} );
            
        }else{
            // 如果取消全选 set清空 并且file-list下的checkbox设置为未被选中状态
            state.selectedIds.clear()
            $$('#file-list input').forEach(checkbox=>{ checkbox.checked=false} );
            $$('#file-list tr').forEach( tr => {tr.classList.remove('is-selected')} );
        }
        updateSelection();
    })
    $('#go-parent').addEventListener('click',async ()=>{
        if(state.breadcrumbs.length<2)return;
        const breadcrumbs=state.breadcrumbs.slice(0,-1);
        await loadDirectory(breadcrumbs.at(-1).id,breadcrumbs);
    });
    renderDirectory();
}

async function loadDirectory(directoryId,breadcrumbs=state.breadcrumbs){
    const requestId=++directoryRequest;
    try{
        const result=await queryDirectory(directoryId);
        if(requestId!==directoryRequest)return false;
        if(!Array.isArray(result?.data?.data)){
            throw new Error("服务器返回的目录数据无效");
        }
        state.currentDirectoryId=directoryId;
        state.entries=result.data.data;
        state.breadcrumbs=breadcrumbs;
        state.selectedIds.clear();
    }catch(error){
        if(requestId!==directoryRequest)return false;
        showToast($('.toast-region'),error.message,"failure");
        return false;
    }
    renderDirectory();
    return true;
}


function startCreateFolder(){

}

function submitCreateFolder(name){

}

function confirmDeleteEntries(ids){

}

function downloadEntries(ids){

}

function getCurrentDirectory(){

}

function renderDirectory(){
    // 面包屑 和 file-list
    $('#file-list').replaceChildren();
    state.entries.forEach(item=>{$('#file-list').append(createFileElement(item));});
    state.selectedIds.forEach(item=>{
        $(`#file-list tr[data-id="${item}"] input`).checked=true;
        $(`#file-list tr[data-id="${item}"]`).classList.add('is-selected');
    });
    updateSelection();
    // 更新面包屑对应项
    $('#breadcrumbs').replaceChildren();
    state.breadcrumbs.forEach((item,index)=>{
        const button=el('button',item.name,'text-button');
        button.type='button';
        button.title=item.name;
        if(index==state.breadcrumbs.length-1){
            button.setAttribute('aria-current','location');
        }
        button.addEventListener('click',async ()=>{
            // 转到对应的页面 即使是aria-current=location 也应重新请求 并刷新页面
            await loadDirectory(item.id,state.breadcrumbs.slice(0,index+1));
        });
        $('#breadcrumbs').append(button);
    });
    $('#go-parent').disabled=state.breadcrumbs.length<2;
    $('#directory-empty').hidden=state.entries.length!==0;
}
