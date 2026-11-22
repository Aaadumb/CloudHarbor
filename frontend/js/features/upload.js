import { uploadFile, createDirectory, cancelChunkUpload } from "../api/files.js";
import { showToast } from "../ui/toast.js";
const $=selector=>document.querySelector(selector);
const tasks=[];
let running=false,mode="files",target,selected=[];
let context;

function element(tag,text,className){
    const node=document.createElement(tag);
    if(text!==undefined)node.textContent=text;
    if(className)node.className=className;
    return node;
}
function notify(message,status="failure"){
    showToast($(".toast-region"),message,status);
}
export function initUpload(options){
    context=options;
    $("#upload-toggle").addEventListener("click",()=>{
        const open=$("#upload-menu").hidden;
        $("#entry-menu").hidden=true;
        $("#upload-menu").hidden=!open;
        $("#upload-toggle").setAttribute("aria-expanded",String(open));
    });
    document.querySelectorAll("[data-pick]").forEach(button=>button.addEventListener("click",()=>pickFile(button.dataset.pick)));
    $("#uploads-choose").addEventListener("click",()=>pickFile("files"));
    for(const kind of ["files","folder"]){
        const input=$(kind==="files"?"#file-input":"#folder-input");
        input.addEventListener("change",()=>{
            const files=Array.from(input.files);
            input.value="";
            if(!files.length)return;
            if(kind==="folder" && new Set(files.map(file=>file.webkitRelativePath.split("/")[0])).size!==1){
                notify("一次请选择一个文件夹");return;
            }
            mode=kind;
            selected=files;
            renderConfirmation();
            if(!$("#upload-dialog").open)$("#upload-dialog").showModal();
        });
    }
    document.querySelectorAll("[data-upload-mode]").forEach(button=>{
        button.addEventListener("click",()=>{mode=button.dataset.uploadMode;selected=[];renderConfirmation();});
        button.addEventListener("keydown",event=>{
            if(!["ArrowLeft","ArrowRight","Home","End"].includes(event.key))return;
            event.preventDefault();
            const next=event.key==="Home"?"files":event.key==="End"?"folder":mode==="files"?"folder":"files";
            document.querySelector('[data-upload-mode="'+next+'"]').click();
            document.querySelector('[data-upload-mode="'+next+'"]').focus();
        });
    });
    $("#choose-again").addEventListener("click",()=>pickFile(mode,true));
    $("#upload-dialog").addEventListener("close",()=>{if(!$("#upload-dialog").open)selected=[];});
    $("#confirm-upload").addEventListener("click",()=>{
        if(!selected.length)return;
        const files=[...selected],destination={...target};
        const batch={directories:new Map(),blocked:new Set()};
        for(const file of files){
            tasks.push({file,size:file.size,target:destination,batch,path:mode==="folder"?file.webkitRelativePath:file.name,
                folder:mode==="folder",state:"排队中",progress:0,uploadId:null});
        }
        $("#upload-dialog").close();
        context.showView("uploads");
        renderTasks();
        runQueue();
    });
}
function pickFile(kind,keepTarget=false){
    mode=kind;
    if(!keepTarget)target=context.getCurrentDirectory();
    $("#upload-menu").hidden=true;
    $("#upload-toggle").setAttribute("aria-expanded","false");
    $(kind==="files"?"#file-input":"#folder-input").click();
}
function renderConfirmation(){
    document.querySelectorAll("[data-upload-mode]").forEach(button=>{
        const active=button.dataset.uploadMode===mode;
        button.setAttribute("aria-selected",String(active));
        button.tabIndex=active?0:-1;
    });
    $("#upload-panel").setAttribute("aria-labelledby",mode==="files"?"files-tab":"folder-tab");
    $("#upload-target").textContent=target.path;
    $("#upload-list").replaceChildren(...selected.map(file=>{
        const row=element("div",undefined,"upload-item");
        row.append(element("span",mode==="folder"?file.webkitRelativePath:file.name,"upload-name"),
            element("span",file.size.toLocaleString()+" B","upload-size"));
        return row;
    }));
    $("#upload-summary").textContent="共 "+selected.length+" 个文件";
    $("#folder-hint").hidden=mode!=="folder";
    $("#confirm-upload").disabled=!selected.length;
}
async function resolveParent(task){
    if(!task.folder)return task.target.id;
    const parts=task.path.split("/");
    parts.pop();
    let parent=task.target.id,path="";
    for(const name of parts){
        if(!name || name==="." || name===".." || /[\\\u0000-\u001f\u007f]/u.test(name))throw new Error("文件夹路径无效");
        path+=(path?"/":"")+name;
        if(task.batch.blocked.has(path))throw new Error("该目录创建失败，请重新选择并改名上传");
        if(!task.batch.directories.has(path)){
            try{
                const result=await createDirectory({parentId:parent,name});
                task.batch.directories.set(path,result.data.id);
            }catch(error){task.batch.blocked.add(path);throw error;}
        }
        parent=task.batch.directories.get(path);
    }
    return parent;
}
async function cancelTask(task){
    if(task.state==="取消中")return;
    task.cancelled=true;
    task.controller?.abort(new DOMException("用户取消","AbortError"));
    task.state="取消中";
    renderTasks();
    if(!running || !task.controller)await finishCancellation(task);
}
async function finishCancellation(task){
    try{
        if(task.uploadId)await cancelChunkUpload(task.uploadId);
        task.state="已取消";
        if(task.controller && !task.uploadId)task.error="请求已停止，请以目录查询结果确认服务端状态";
        task.file=null;
    }catch(error){
        task.state="取消失败";
        task.error=error.message;
    }
    renderTasks();
}
async function runQueue(){
    if(running)return;
    running=true;
    try{
        while(true){
            const task=tasks.find(item=>item.state==="排队中");
            if(!task)break;
            task.controller=new AbortController();
            task.state="准备上传";
            renderTasks();
            try{
                const parent=await resolveParent(task);
                task.controller.signal.throwIfAborted();
                await uploadFile(parent,task.file,{
                    task,signal:task.controller.signal,
                    onProgress:(value,state)=>{task.progress=value;task.state=state;renderTasks();}
                });
                task.state="完成";
                task.file=null;
                tasks.splice(tasks.indexOf(task),1);
            }catch(error){
                if(task.cancelled)await finishCancellation(task);
                else {task.state="失败";task.error=error.message;}
            }finally{
                task.controller=null;
                await context.onUploadComplete(task.target);
                renderTasks();
            }
        }
    }finally{running=false;}
}
function renderTasks(){
    $("#view-uploads .empty-state").hidden=tasks.length>0;
    let list=$("#upload-tasks");
    if(!list){
        list=element("div",undefined,"upload-tasks");
        list.id="upload-tasks";
        $("#view-uploads").append(list);
    }
    list.replaceChildren(...tasks.map(task=>{
        const row=element("div",undefined,"upload-task");
        const failed=["失败","取消失败"].includes(task.state);
        const inactive=["排队中","已取消"].includes(task.state);
        if(failed)row.classList.add("is-error");
        if(inactive)row.classList.add("is-inactive");
        const icon=element("img",undefined,"upload-task-icon");
        icon.src="assets/icons/file.svg";
        icon.alt="";
        const details=element("div",undefined,"upload-task-details");
        const name=element("span",task.path,"upload-task-name");
        name.title=task.path;
        const size=task.size ?? task.file?.size;
        const destination=element("span",task.target.path,"upload-task-target");
        destination.title=task.target.path;
        const metadata=element("div",undefined,"upload-task-meta");
        metadata.append(element("span",size===undefined?"":formatBytes(size)),destination);
        details.append(name,metadata);
        const transfer=element("div",undefined,"upload-task-transfer");
        const status=element("div",undefined,"upload-task-status");
        status.append(element("span",task.state));
        if(task.state==="上传分片")status.append(element("span",task.progress+"%","upload-task-percent"));
        const progress=element("progress");
        progress.max=100;
        if(!["上传中","正在合并","准备上传","取消中"].includes(task.state))progress.value=task.progress;
        progress.setAttribute("aria-label",task.path+" · "+task.state);
        transfer.append(status,progress);
        if(task.error){
            const error=element("p",task.error,"upload-task-error");
            transfer.append(error);
        }
        const actions=element("div",undefined,"upload-task-actions");
        row.append(icon,details,transfer,actions);
        if(failed){
            const retry=element("button",task.state==="失败"?"重试":"重试取消","text-button");
            retry.type="button";
            retry.setAttribute("aria-label",retry.textContent+" "+task.path);
            retry.addEventListener("click",()=>{
                if(task.cancelled){finishCancellation(task);return;}
                task.error="";task.state="排队中";renderTasks();runQueue();
            });
            actions.append(retry);
        }
        if(task.file && !["已取消","完成"].includes(task.state)){
            const cancel=element("button","取消","text-button upload-task-cancel");
            cancel.type="button";cancel.disabled=task.state==="取消中";
            cancel.setAttribute("aria-label","取消上传 "+task.path);
            cancel.addEventListener("click",()=>cancelTask(task));
            actions.append(cancel);
        }
        return row;
    }));
}

function formatBytes(bytes){
    if(bytes<1024)return bytes+" B";
    if(bytes<1048576)return (bytes/1024).toFixed(1)+" KiB";
    if(bytes<1073741824)return (bytes/1048576).toFixed(1)+" MiB";
    return (bytes/1073741824).toFixed(1)+" GiB";
}
