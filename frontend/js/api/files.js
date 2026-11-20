import { postFormData,postJson,getFetch, deleteFetch } from "./http.js";
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

export function uploadFile(parentId,file){
    const fd=new FormData();
    fd.set('parentId',String(parentId));
    fd.set('lastModified',String(file.lastModified));
    fd.set('file',file);

    return postFormData("/files/upload",fd);
}

export function initChunkUpload(metadata){
    return postJson('/files/upload/init',{
        ...metadata,
        parentId:String(metadata.parentId)
    });
}

export function uploadChunk(uploadId,index,chunk){
    const fd=new FormData()
    fd.set('uploadId',uploadId)
    fd.set('index',+index)
    fd.set('file',chunk)
    return postFormData('/files/upload/chunk',fd)
}

export function queryUploadStatus(uploadId){
    return getFetch('/files/upload/status?'+new URLSearchParams({uploadId}));
}
export function completeChunkUpload(uploadId){
    return postJson('/files/upload/complete',{uploadId});
}
export function cancelChunkUpload(uploadId){
    return postJson('/files/upload/cancel',{uploadId});
}
