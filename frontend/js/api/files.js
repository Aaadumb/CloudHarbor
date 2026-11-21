import { postFormData,postJson,getFetch, deleteFetch } from "./http.js";
const chunkSize = 5 * 1024 * 1024;
export function queryDirectory(id){
    return getFetch("/files/query?id="+String(id));
}
export function createDirectory({parentId,name}){
    parentId=String(parentId)
    return postJson("/files/create",{parentId,name});
}
export function deleteEntries(ids){
    return deleteFetch('/files/delete',{entryIds:[...ids]});
}
export function deleteEntry(id){
    return deleteFetch("/files/delete",{entryIds:[String(id)]});
}

export function getDownloadUrl(id){
    return postJson('/files/download',{id:String(id)});
}

export async function uploadFile(parentId,file,{signal,onProgress=()=>{},task={}}={}){
    const options={signal,timeoutMs:120000};
    if(file.size<=chunkSize){
        const fd=new FormData();
        fd.set("parentId",parentId);
        fd.set("lastModified",String(file.lastModified));
        fd.set("file",file);
        onProgress(0,"上传中");
        const result=await postFormData("/files/upload",fd,options);
        onProgress(100,"完成");
        return result;
    }
    if(!task.uploadId){
        const result=await postJson("/files/upload/init",{
            parentId,name:file.name,size:file.size,lastModified:file.lastModified
        },options);
        task.uploadId=result.data.uploadId;
    }
    const complete=async()=>{
        signal?.throwIfAborted();
        task.mergeRequested=true;
        onProgress(100,"正在合并");
        try{
            return await postJson("/files/upload/complete",{uploadId:task.uploadId},options);
        }catch(error){
            if(error.code==="UPLOAD_CHUNKS_MISSING")task.mergeRequested=false;
            throw error;
        }
    };
    if(task.mergeRequested){
        try{return await complete();}
        catch(error){if(error.code!=="UPLOAD_CHUNKS_MISSING")throw error;}
    }
    let result;
    try{
        result=await getFetch("/files/upload/status?"+new URLSearchParams({uploadId:task.uploadId}),options);
    }catch(error){
        if(error.code==="UPLOAD_BUSY")return complete();
        throw error;
    }
    const status=result.data;
    if(!Number.isSafeInteger(status.chunkSize)||status.chunkSize<=0 ||
       !Number.isSafeInteger(status.totalChunks)||status.totalChunks!==Math.ceil(file.size/status.chunkSize)){
        throw new Error("服务器返回的分片参数无效");
    }
    const received=new Set(status.uploadedChunks);
    for(let index=1;index<=status.totalChunks;index++){
        signal?.throwIfAborted();
        if(!received.has(index)){
            const fd=new FormData();
            fd.set("uploadId",task.uploadId);
            fd.set("index",String(index));
            fd.set("file",file.slice((index-1)*status.chunkSize,Math.min(index*status.chunkSize,file.size)),file.name);
            await postFormData("/files/upload/chunk",fd,options);
            received.add(index);
        }
        onProgress(Math.floor(received.size/status.totalChunks*100),"上传分片");
    }
    return complete();
}
export function cancelChunkUpload(uploadId){
    return postJson("/files/upload/cancel",{uploadId});
}
