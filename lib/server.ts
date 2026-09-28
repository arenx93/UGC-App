import {downloadImage} from './image-download';
import {env} from 'cloudflare:workers';
export function database(){if(!env.DB)throw Error('Account storage is temporarily unavailable. Please try again.');return env.DB;}
export function bucket(){if(!env.BUCKET)throw Error('Image storage is temporarily unavailable. Please try again.');return env.BUCKET;}
export async function cipher(value:string,userId:string,decrypt=false){
 const secret=(env as any).KEY_ENCRYPTION_SECRET;if(!secret)throw Error('Secure key storage is not configured.');
 const raw=Uint8Array.from(atob(secret),c=>c.charCodeAt(0));const key=await crypto.subtle.importKey('raw',raw,'AES-GCM',false,['encrypt','decrypt']);const additionalData=new TextEncoder().encode(userId);
 if(decrypt){const data=Uint8Array.from(atob(value),c=>c.charCodeAt(0));return new TextDecoder().decode(await crypto.subtle.decrypt({name:'AES-GCM',iv:data.slice(0,12),additionalData},key,data.slice(12)));}
 const iv=crypto.getRandomValues(new Uint8Array(12));const data=new Uint8Array(await crypto.subtle.encrypt({name:'AES-GCM',iv,additionalData},key,new TextEncoder().encode(value)));return btoa(String.fromCharCode(...iv,...data));
}
export async function userKey(user:string){const row=await database().prepare('SELECT key_cipher FROM accounts WHERE user_id=?').bind(user).first<{key_cipher:string}>();if(!row?.key_cipher)throw Error('Connect your KIE API key in account settings first.');return cipher(row.key_cipher,user,true);}
export async function kie(path:string,key:string,body?:unknown,timeoutMs=55000){
 const r=await fetch('https://api.kie.ai'+path,{method:body===undefined?'GET':'POST',headers:{Authorization:'Bearer '+key,'Content-Type':'application/json'},body:body===undefined?undefined:JSON.stringify(body),signal:AbortSignal.timeout(timeoutMs)});
 const data:any=await r.json();if(!r.ok||(data.code!==undefined&&data.code!==200)){const err:any=Error(String(data.msg||data.message||data.error?.message||'KIE could not complete this request.').replaceAll(key,'[redacted]'));err.definite=true;throw err;}return data;
}
export async function uploadReferenceToKie(file:any,object:R2ObjectBody,key:string){
 const extensions:Record<string,string>={'image/png':'.png','image/jpeg':'.jpg','image/webp':'.webp','video/mp4':'.mp4','video/quicktime':'.mov','video/x-matroska':'.mkv','audio/mpeg':'.mp3','audio/wav':'.wav','audio/x-wav':'.wav','audio/aac':'.aac','audio/mp4':'.m4a','audio/ogg':'.ogg'};const extension=extensions[file.mime]||'';
 const form=new FormData();form.append('file',new Blob([await object.arrayBuffer()],{type:file.mime}),file.name);form.append('uploadPath',(file.mime.startsWith('image/')?'images':file.mime.startsWith('video/')?'videos':'audio')+'/framecraft');form.append('fileName',file.id+extension);
 const r=await fetch('https://kieai.redpandaai.co/api/file-stream-upload',{method:'POST',headers:{Authorization:'Bearer '+key},body:form,signal:AbortSignal.timeout(45000)});let data:any;try{data=await r.json();}catch{throw Error('KIE reference upload returned an invalid response.');}
 if(!r.ok||!data.success||!data.data?.downloadUrl)throw Error('KIE reference upload failed. Please try again.');return data.data.downloadUrl as string;
}
export async function rateLimit(user:string,operation:string,max:number){const now=Date.now();const id=user+':'+operation+':'+Math.floor(now/60000);const row=await database().prepare('INSERT INTO limits(id,count,expires) VALUES(?,1,?) ON CONFLICT(id) DO UPDATE SET count=count+1 RETURNING count').bind(id,now+120000).first<{count:number}>();await database().prepare('DELETE FROM limits WHERE expires<?').bind(now).run();if((row?.count||0)>max)throw Error('Too many requests. Please wait a minute and try again.');}
export async function jobList(user:string){const r=await database().prepare('SELECT id,model,prompt,settings,status,progress,images,error,created FROM jobs WHERE user_id=? ORDER BY created DESC LIMIT 100').bind(user).all();return r.results.map((j:any)=>({...j,settings:JSON.parse(j.settings),images:JSON.parse(j.images)}));}
export async function refreshJob(job:any,key:string){
 const db=database();if(!job.task_id)return;
 const data=await kie('/api/v1/jobs/recordInfo?taskId='+encodeURIComponent(job.task_id),key);const d=data.data;
 const progress=Math.max(0,Math.min(99,Number.isFinite(Number(d.progress))?Math.round(Number(d.progress)):d.state==='generating'?10:2));
 if(d.state==='fail'){await db.prepare('UPDATE jobs SET status=?,progress=?,error=?,updated=? WHERE id=? AND user_id=?').bind('fail',progress,String(d.failMsg||'KIE generation failed.').replaceAll(key,'[redacted]'),Date.now(),job.id,job.user_id).run();return;}
 if(d.state!=='success'){await db.prepare('UPDATE jobs SET status=?,progress=?,updated=? WHERE id=? AND user_id=?').bind('generating',progress,Date.now(),job.id,job.user_id).run();return;}
 const result=typeof d.resultJson==='string'?JSON.parse(d.resultJson):d.resultJson;const urls=result?.resultUrls;if(!Array.isArray(urls)||!urls.length)throw Error('KIE returned no media for this generation.');
 const settings=JSON.parse(job.settings||'{}');const isVideo=settings.kind==='video';
 await db.prepare('UPDATE jobs SET status=?,progress=? WHERE id=? AND user_id=?').bind('saving',99,job.id,job.user_id).run();const ids=[];
 for(let i=0;i<urls.length;i++){
  const url=new URL(urls[i]);if(url.protocol!=='https:')throw Error('KIE returned an unsupported media URL.');
  const response=await downloadImage(url.toString(),fetch,isVideo?180000:45000);if(!response.ok)throw Error('Could not save the generated media yet. It will retry automatically.');
  const mime=(response.headers.get('content-type')||'').split(';')[0];
  if(isVideo){
   if(!['video/mp4','video/quicktime','application/octet-stream'].includes(mime))throw Error('Unsupported video format returned by KIE.');
   const size=Number(response.headers.get('content-length')||0),max=250*1024*1024;if(size>max)throw Error('The generated video exceeds the 250 MB storage limit.');
   const id=job.id+'-'+i,objectKey=job.user_id+'/'+id;await bucket().put(objectKey,response.body,{httpMetadata:{contentType:mime==='application/octet-stream'?'video/mp4':mime}});await db.prepare('INSERT OR IGNORE INTO files(id,user_id,name,mime,kind,object_key,created) VALUES(?,?,?,?,?,?,?)').bind(id,job.user_id,'framecraft-'+id+(mime==='video/quicktime'?'.mov':'.mp4'),mime==='application/octet-stream'?'video/mp4':mime,'generated',objectKey,Date.now()).run();ids.push(id);continue;
  }
  if(!['image/png','image/jpeg','image/webp'].includes(mime))throw Error('Unsupported image format returned by KIE.');
  const max=30*1024*1024; if(Number(response.headers.get('content-length'))>max)throw Error('The generated image exceeds the 30 MB storage limit.');
  const reader=response.body!.getReader();const chunks:Uint8Array[]=[];let size=0;while(true){const {done,value}=await reader.read();if(done)break;size+=value.length;if(size>max){await reader.cancel();throw Error('The generated image exceeds the 30 MB storage limit.');}chunks.push(value);}
  const bytes=new Uint8Array(size);let offset=0;for(const chunk of chunks){bytes.set(chunk,offset);offset+=chunk.length;}
  const id=job.id+'-'+i, objectKey=job.user_id+'/'+id;await bucket().put(objectKey,bytes,{httpMetadata:{contentType:mime}});await db.prepare('INSERT OR IGNORE INTO files(id,user_id,name,mime,kind,object_key,created) VALUES(?,?,?,?,?,?,?)').bind(id,job.user_id,'framecraft-'+id+(mime==='image/png'?'.png':mime==='image/webp'?'.webp':'.jpg'),mime,'generated',objectKey,Date.now()).run();ids.push(id);
 }
 await db.prepare('UPDATE jobs SET status=?,progress=?,images=?,error=NULL,updated=? WHERE id=? AND user_id=?').bind('success',100,JSON.stringify(ids),Date.now(),job.id,job.user_id).run();
}
