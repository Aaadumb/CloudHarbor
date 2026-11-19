let toastTimer=null;
export function showToast(toastRegion,message,status){
    window.clearTimeout(toastTimer)
    const toast=document.createElement('div')
    toast.classList.add('toast')
    toast.textContent=message
    toast.dataset.status=status
    toastRegion.replaceChildren(toast)
    toastTimer=window.setTimeout(() => {
        toastRegion.replaceChildren()
    }, 2000);
}