const state = {
    currentDirectoryId:null,
    breadcrumbs: [],
    entries: [],
    selectedIds: new Set()
}
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
export function createFileElement(file){
    const row=el("tr");
    const selectCell=el("td"),checkbox=el("input");
    row.dataset.id=file.id;
    checkbox.type="checkbox";
    checkbox.setAttribute("aria-label","选择"+file.name)
    checkbox.addEventListener("change",()=>{

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
        fileName.addEventListener('click',()=>{

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
    actionCell.append(downloadBtn,deleteBtn,moreBtn)
    row.append(actionCell)
    return row
}
function initFileBrowser(){

}

function loadDirectory(directoryId){

}

function renderDirectory(directory){

}

function updateSelection(){

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

function refreshDirectory(){

}