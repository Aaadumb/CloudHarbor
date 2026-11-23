import { getDownloadUrl, queryDirectory, createDirectory, deleteEntries, moveEntries, copyEntries } from "../api/files.js";
import { showToast } from "../ui/toast.js";
const $ = selector => document.querySelector(selector);
const $$=selector=> document.querySelectorAll(selector);
const state = {
    currentDirectoryId:null,
    breadcrumbs: [],
    entries: [],
    selectedIds: new Set()
}
let rootId;
let directoryRequest = 0;
let directoryNavigation = 0;
let menuEntry;
let pendingDeleteIds = [];
let pendingDeleteDirectoryId;
let transfer;

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
    $$('.toolbar .btn:not(.btn-primary):not(#new-folder)').forEach(item=>{item.disabled=(state.selectedIds.size===0);})
}
function createFileElement(file){
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
    downloadBtn.addEventListener('click',async()=>{await downloadEntries([file])});
    deleteBtn.addEventListener('click',()=>{ confirmDeleteEntries([file]); });
    moreBtn.addEventListener('click',()=>{ openEntryMenu(file,moreBtn); });

    actionCell.append(downloadBtn,deleteBtn,moreBtn)
    row.append(actionCell)
    return row
}
export async function initFileBrowser(userRootId){
    rootId=userRootId;
    state.currentDirectoryId=rootId;
    state.breadcrumbs=[{id:rootId,name:"/"}];
    $('#select-all').addEventListener('click',()=>{
        const checked=$('#select-all').checked
        // console.log($$('#file-list tr'))
        if(checked){
            // 如果被选中 file-list下所有的checkbox都要设置为选中 并且要更新set
            state.selectedIds.clear()
            $$('#file-list input[type="checkbox"]').forEach(checkbox=>{ checkbox.checked=true} );
            $$('#file-list tr[data-id]').forEach( tr => {tr.classList.add('is-selected');state.selectedIds.add(tr.dataset.id)} );
            
        }else{
            // 如果取消全选 set清空 并且file-list下的checkbox设置为未被选中状态
            state.selectedIds.clear()
            $$('#file-list input[type="checkbox"]').forEach(checkbox=>{ checkbox.checked=false} );
            $$('#file-list tr[data-id]').forEach( tr => {tr.classList.remove('is-selected')} );
        }
        updateSelection();
    })
    $('#go-parent').addEventListener('click',async ()=>{
        if(state.breadcrumbs.length<2)return;
        const breadcrumbs=state.breadcrumbs.slice(0,-1);
        await loadDirectory(breadcrumbs.at(-1).id,breadcrumbs);
    });
    document.addEventListener('click',event=>{if(!event.target.closest('.menu-wrap, #entry-menu, .more-button'))closeMenus();})
    document.addEventListener('keydown',event=>{if(event.key==='Escape')closeMenus();});
    $('#new-folder').addEventListener('click',()=>{
        startCreateFolder();
    });
    $('#confirm-dialog').addEventListener('cancel',event=>{
        if($('#confirm-action').disabled)event.preventDefault();
    });
    $('#confirm-action').addEventListener('click',async ()=>{
        const button=$('#confirm-action');
        if(button.disabled || button.dataset.type!=='delete'){
            return;
        }
        $('#confirm-dialog .close-button').disabled=$('#confirm-dialog .btn[data-close]').disabled=button.disabled=true;
        const ids=[...pendingDeleteIds];
        const directoryId=pendingDeleteDirectoryId;
        const navigationId=directoryNavigation;
        try{
            await deleteEntries(ids);
            showToast($('.toast-region'),'删除成功','success');
        }catch(error){
            showToast($('.toast-region'),error.message,'failure');
            return;
        }finally{
            $('#confirm-dialog .close-button').disabled=$('#confirm-dialog .btn[data-close]').disabled=button.disabled=false;
            $('#confirm-dialog').close();
        }
        await refreshDirectoryIfCurrent(directoryId,navigationId);
    })

    $('#entry-menu').addEventListener('click',event=>{
        const button=event.target.closest('button');
        if(!button || !menuEntry)return;
        switch(button.dataset.action){
            case 'download':
                downloadEntries([menuEntry]);
                break;
            case 'delete':
                confirmDeleteEntries([menuEntry]);
                break;
            case 'move':
            case 'copy':
                openTransferDialog(button.dataset.action, [menuEntry.id]);
                break;
        }
        closeMenus();
    })

    $('#delete-selected').addEventListener('click',()=>{
        const items=state.entries.filter(item=>state.selectedIds.has(item.id));
        if(items.length===0){
            showToast($('.toast-region'),'空操作','failure');
            return;
        }
        confirmDeleteEntries(items);
    });
    $('#download-selected').addEventListener('click',()=>{
        const items=state.entries.filter(item=>state.selectedIds.has(item.id));
        if(items.length===0){
            showToast($('.toast-region'),'空操作','failure');
            return;
        }
        downloadEntries(items);
    });
    $('#move-selected').addEventListener('click',()=>{
        openTransferDialog('move', [...state.selectedIds]);
    });
    $('#copy-selected').addEventListener('click',()=>{
        openTransferDialog('copy', [...state.selectedIds]);
    });
    $('#transfer-parent').addEventListener('click',()=>{
        loadTransferDirectory(transfer, transfer.path.slice(0,-1));
    });
    $('#transfer-retry').addEventListener('click',()=>{
        loadTransferDirectory(transfer, transfer.path);
    });
    $('#transfer-action').addEventListener('click', submitTransfer);
    $('#transfer-dialog').addEventListener('cancel',event=>{
        if(transfer?.submitting)event.preventDefault();
    });

    window.addEventListener('popstate',restoreDirectory);
    renderDirectory();
    if(!await restoreDirectory())throw new Error("目录加载失败，请重新加载");
}
async function restoreDirectory(){
    const id=new URLSearchParams(location.hash.slice(1)).get('dir');
    const path=history.state?.breadcrumbs;
    if(id && Array.isArray(path) && path[0]?.id===rootId && path.at(-1)?.id===id){
        return loadDirectory(id,path,false,false);
    }else{
        return loadDirectory(rootId,[{id:rootId,name:"/"}],false,false);
    }
}
export function getCurrentDirectory(){
    return {id:state.currentDirectoryId,
        path:"/"+state.breadcrumbs.slice(1).map(item=>item.name).join("/"),
        navigationId:directoryNavigation};
}
export async function refreshAfterUpload(target){
    await refreshDirectoryIfCurrent(target.id,target.navigationId);
}

async function loadDirectory(directoryId,breadcrumbs=state.breadcrumbs,isRefresh=false,recordHistory=true){
    if(!isRefresh)directoryNavigation++;
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
    closeMenus();
    if(!isRefresh){
        const url="#dir="+encodeURIComponent(directoryId);
        const snapshot={breadcrumbs:state.breadcrumbs.map(item=>({...item}))};
        if(recordHistory && location.hash!==url)history.pushState(snapshot,"",url);
        else history.replaceState(snapshot,"",url);
    }
    renderDirectory(isRefresh);
    return true;
}

async function refreshDirectoryIfCurrent(directoryId,navigationId){
    if(state.currentDirectoryId===directoryId && directoryNavigation===navigationId){
        await loadDirectory(directoryId,state.breadcrumbs,true);
    }
}

function closeMenus(){
    $('#upload-menu').hidden=$('#entry-menu').hidden=true;
    $('#upload-toggle').setAttribute('aria-expanded','false');
}

function openEntryMenu(item,button){
    closeMenus();
    menuEntry=item;
    const menu=$('#entry-menu');
    const rect=button.getBoundingClientRect();
    menu.hidden=false;
    menu.style.left = Math.max(8, Math.min(innerWidth - 178, rect.right - 170)) + "px";
    menu.style.top = Math.max(8, Math.min(innerHeight - menu.offsetHeight - 8, rect.bottom + 4)) + "px";
    menu.querySelector('button').focus();
}
function startCreateFolder(){
    closeMenus();
    if($('#new-folder-row')){
        $('#new-folder-name').focus();
        return;
    }
    const row=el('tr');row.id='new-folder-row';
    const cell=el('td',undefined,'new-folder-cell');cell.colSpan=6;
    const form=el('form',undefined,'new-folder-form'),input=el('input',undefined,'field-input');
    input.id='new-folder-name';
    input.required=true;
    input.maxLength=255;
    input.placeholder='请输入文件夹名称';
    input.setAttribute('aria-label','新文件名称');
    const submit=el('button','确认','btn btn-primary');submit.type='submit';
    const cancel=el('button','取消','btn');cancel.type='button';
    cancel.addEventListener('click',()=>row.remove());
    input.addEventListener('input',()=>input.removeAttribute("aria-invalid"));
    input.addEventListener('keydown',event=>{if (event.key==='Escape')row.remove();});
    form.addEventListener('submit',async event=>{
        event.preventDefault();
        if(submit.disabled)return;
        const name=input.value;
        if (!name.trim() || name === "." || name === ".." || /[\/\\\u0000-\u001f\u007f]/u.test(name)) {
            input.setAttribute('aria-invalid','true');
            showToast($('.toast-region'),'名称不能为空 不能是 . 或 .. 不能含路径分隔符或控制字符','failure')
            return;
        }else if(state.entries.some(item=>item.name===name)){
            input.setAttribute('aria-invalid','true');
            showToast($('.toast-region'),'存在同名文件/文件夹','failure');
            return;
        }
        if(!form.reportValidity())return;
        // 向后端发送请求
        cancel.disabled=submit.disabled=true;
        try{
            await submitCreateFolder(name);
        }finally{
            row.remove();
        }
    });
    const img=el('img',undefined,'icon');
    img.src='assets/icons/folder.svg';
    form.append(img,input,submit,cancel);
    cell.append(form);
    row.append(cell);
    $('#file-list').prepend(row);
    input.focus();
    
}

async function submitCreateFolder(name){
    const directoryId=state.currentDirectoryId;
    const navigationId=directoryNavigation;
    try{
        await createDirectory({parentId:directoryId,name});
        await refreshDirectoryIfCurrent(directoryId,navigationId);
    }catch(error){
        showToast($('.toast-region'),error.message,'failure');
        return;
    }
}

function confirmDeleteEntries(items){
    if(!items.length)return;
    pendingDeleteIds=items.map(item=>item.id);
    pendingDeleteDirectoryId=state.currentDirectoryId;
    $('#confirm-title').textContent='确认删除';
    $('#confirm-description').textContent=items.some(item=> item.type=='folder')
        ?'是否删除以下条目? 文件夹将包含全部子内容'
        :'是否删除以下文件?';
    $('#confirm-list').replaceChildren(...items.map(item=>el('li',item.name)));
    $('#confirm-list').hidden=false;
    $('#confirm-action').textContent='确认删除'
    $('#confirm-dialog').showModal();
    $('#confirm-action').dataset.type='delete';
}

function openTransferDialog(action, ids){
    if(!ids.length)return;
    const dialog=$('#transfer-dialog');
    const isMove=action==='move';
    const label=isMove?'移动':'复制';
    transfer={ids:new Set(ids), isMove, label, sourceId:state.currentDirectoryId, submitting:false};

    $('#transfer-title').textContent=`${label}到`;
    $('#transfer-summary').textContent=`已选择 ${ids.length} 项`;
    $('#transfer-action').textContent=`确认${label}`;
    dialog.showModal();
    loadTransferDirectory(transfer, [{id:rootId,name:'/'}]);
}

async function loadTransferDirectory(operation, path){
    const dialog=$('#transfer-dialog');
    const parent=$('#transfer-parent');
    const confirm=$('#transfer-action');
    const retry=$('#transfer-retry');
    const status=$('#transfer-status');
    const folders=$('#transfer-folders');
    const current=path.at(-1);
    const pathLabel=$('#transfer-path');
    const pathText='/'+path.slice(1).map(item=>item.name).join('/');
    pathLabel.textContent=pathText;
    pathLabel.parentElement.title=pathText;
    if(folders.contains(document.activeElement) ||
        [parent,retry].includes(document.activeElement)){
        pathLabel.parentElement.focus({preventScroll:true});
    }
    operation.path=path;
    parent.disabled=confirm.disabled=true;
    retry.hidden=true;
    folders.replaceChildren();
    status.textContent='正在加载目录…';

    try{
        const result=await queryDirectory(current.id);
        if(!dialog.open || operation!==transfer)return;
        const entries=result?.data?.data;
        if(!Array.isArray(entries))throw new Error('服务器返回的目录数据无效');
        for(const entry of entries){
            if(entry.type!=='folder')continue;
            const button=el('button',undefined,'btn');
            button.type='button';
            const icon=el('img',undefined,'icon');
            icon.src='assets/icons/folder.svg';
            icon.alt='';
            button.append(icon,el('span',entry.name));
            button.disabled=operation.isMove && operation.ids.has(entry.id);
            if(button.disabled)button.title='不能移动到选中的文件夹或其子目录';
            button.addEventListener('click',()=>{
                loadTransferDirectory(operation, [...path,{id:entry.id,name:entry.name}]);
            });
            folders.append(button);
        }
        confirm.disabled=current.id===operation.sourceId;
        status.textContent=folders.childElementCount?'':'没有子文件夹';
        if(confirm.disabled)status.textContent='请选择其他目录';
    }catch(error){
        if(!dialog.open || operation!==transfer)return;
        status.textContent=error.message;
        retry.hidden=false;
    }
    parent.disabled=path.length<2;
}

async function submitTransfer(){
    const confirm=$('#transfer-action');
    if(confirm.disabled)return;
    const operation=transfer;
    const targetId=operation.path.at(-1).id;
    const dialog=$('#transfer-dialog');
    const status=$('#transfer-status');
    const controls=[...dialog.querySelectorAll('button:not(:disabled)')];
    const directoryId=state.currentDirectoryId;
    const navigationId=directoryNavigation;

    operation.submitting=true;
    controls.forEach(button=>button.disabled=true);
    status.textContent=`正在${operation.label}…`;
    try{
        const submit=operation.isMove?moveEntries:copyEntries;
        await submit([...operation.ids], targetId);
    }catch(error){
        status.textContent=error.message;
        return;
    }finally{
        operation.submitting=false;
        controls.forEach(button=>button.disabled=false);
        confirm.focus();
    }
    dialog.close();
    showToast($('.toast-region'),`${operation.label}成功`,'success');
    if(directoryId===targetId || directoryId===operation.sourceId){
        await refreshDirectoryIfCurrent(directoryId,navigationId);
    }
}

async function downloadEntries(items){
    if(!items.length)return;
    if(items.some(item=> item.type==="folder")){
        showToast($('.toast-region'),'暂不支持下载文件夹','failure');
        return;
    }
    for(const item of items){
        let url;
        try{
            const result=await getDownloadUrl(item.id);
            url=result.data.url;
        }catch(error){
            showToast($('.toast-region'),error.message,"failure");
            return;
        }
        const a=el('a');
        a.href=url;
        document.body.append(a);
        a.click();
        a.remove();
    }
}

function renderDirectory(preserveNewFolder=false){
    const newFolderRow=preserveNewFolder ? $('#new-folder-row') : null;
    // file-list
    $('#file-list').replaceChildren();
    state.entries.forEach(item=>{$('#file-list').append(createFileElement(item));});
    if(newFolderRow)$('#file-list').prepend(newFolderRow);
    state.selectedIds.forEach(item=>{
        $(`#file-list tr[data-id="${item}"] input`).checked=true;
        $(`#file-list tr[data-id="${item}"]`).classList.add('is-selected');
    });
    updateSelection();
    // 更新面包屑对应项
    $('#breadcrumbs').replaceChildren();
    state.breadcrumbs.forEach((item,index)=>{
        // 根目录按钮本身显示 /，后续子目录之间再添加分隔符。
        if(index>1){
            const separator=el('span','/','breadcrumb-separator');
            separator.setAttribute('aria-hidden','true');
            $('#breadcrumbs').append(separator);
        }
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
