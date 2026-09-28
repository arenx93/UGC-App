import {BUILTIN_SKILL_ID,builtinContext} from '@/lib/builtin-skill';
import {callOpenAI,promptRequest,readOpenAIPrompt} from '@/lib/openai-prompt';
import {buildPromptRequest,buildCodexPromptRequest,KIE_PROMPT_MODELS,readPromptResponse,readCodexPromptResponse,type KiePromptModel} from '@/lib/prompt-request';
import {getChatGPTUser} from '@/app/chatgpt-auth';
import {database,bucket,cipher,userKey,kie,rateLimit,jobList,refreshJob,uploadReferenceToKie} from '@/lib/server';
import {MODELS,CAMERAS,FILMS,composePrompt,ratios} from '@/lib/presets';
export const dynamic='force-dynamic';
function json(data:unknown,status=200){return Response.json(data,{status,headers:{'Cache-Control':'no-store','X-Content-Type-Options':'nosniff'}});}
function assert(condition:unknown,message:string):asserts condition{if(!condition)throw Error(message);}
async function handle(req:Request,{params}:{params:Promise<{path:string[]}>}){
 try{
 const user=await getChatGPTUser();if(!user)return json({error:'Sign in to access your workspace.'},401);const uid=user.userId;const {path}=await params;const op=path[0];const db=database();
 if(req.method==='POST'){
  const origin=req.headers.get('origin');if(origin&&origin!==new URL(req.url).origin)return json({error:'Invalid request origin.'},403);
   const requestBytes=Number(req.headers.get('content-length')||0);assert(requestBytes<(op==='upload'?210:12)*1024*1024,'This upload is too large.');
 }
 if(req.method==='GET'&&op==='state'){
   const [account,skills,references,jobs]=await Promise.all([db.prepare('SELECT key_cipher,openai_key_cipher FROM accounts WHERE user_id=?').bind(uid).first<any>(),db.prepare('SELECT id,name,content FROM skills WHERE user_id=? ORDER BY created DESC').bind(uid).all(),db.prepare("SELECT id,name,mime,kind,duration_ms FROM files WHERE user_id=? AND kind LIKE 'reference%' ORDER BY created DESC LIMIT 100").bind(uid).all(),jobList(uid)]);
  return json({connected:!!account?.key_cipher,openaiConnected:!!account?.openai_key_cipher,skills:skills.results,references:references.results,jobs});
 }
  if(req.method==='GET'&&op==='file'){
   const file:any=await db.prepare('SELECT * FROM files WHERE id=? AND user_id=?').bind(path[1]||'',uid).first();if(!file)return json({error:'Media not found.'},404);const store=bucket(),head=await store.head(file.object_key);if(!head)return json({error:'Media not found.'},404);const headers=new Headers({'Content-Type':file.mime,'Cache-Control':'private, no-store','X-Content-Type-Options':'nosniff','Accept-Ranges':'bytes'});if(new URL(req.url).searchParams.has('download'))headers.set('Content-Disposition','attachment; filename="'+file.name.replace(/[^a-zA-Z0-9_.-]/g,'_')+'"');
   const range=req.headers.get('range');if(range&&(file.mime.startsWith('video/')||file.mime.startsWith('audio/'))){const match=/^bytes=(\d*)-(\d*)$/.exec(range);if(!match)return new Response(null,{status:416,headers:{'Content-Range':'bytes */'+head.size}});const start=match[1]?Number(match[1]):0,end=match[2]?Math.min(Number(match[2]),head.size-1):head.size-1;if(!Number.isInteger(start)||!Number.isInteger(end)||start<0||start>end||start>=head.size)return new Response(null,{status:416,headers:{'Content-Range':'bytes */'+head.size}});const object=await store.get(file.object_key,{range:{offset:start,length:end-start+1}});if(!object)return json({error:'Media not found.'},404);headers.set('Content-Range',`bytes ${start}-${end}/${head.size}`);headers.set('Content-Length',String(end-start+1));return new Response(object.body,{status:206,headers});}
   const object=await store.get(file.object_key);if(!object)return json({error:'Media not found.'},404);headers.set('Content-Length',String(head.size));return new Response(object.body,{headers});
 }
 if(req.method!=='POST')return json({error:'Not found.'},404);
  if(op==='upload'){
   await rateLimit(uid,'upload',30);const data=await req.formData(),file=data.get('file'),referenceType=String(data.get('referenceType')||'image'),durationMs=Math.round(Number(data.get('durationMs')||0));assert(file instanceof File,'Choose a reference file.');assert(['image','video','audio'].includes(referenceType),'Choose a valid reference type.');
   const extension=file.name.toLowerCase().match(/\.[a-z0-9]+$/)?.[0]||'',mimeByExtension:Record<string,string>={'.png':'image/png','.jpg':'image/jpeg','.jpeg':'image/jpeg','.webp':'image/webp','.mp4':referenceType==='audio'?'audio/mp4':'video/mp4','.mov':'video/quicktime','.mkv':'video/x-matroska','.mp3':'audio/mpeg','.wav':'audio/wav','.aac':'audio/aac','.m4a':'audio/mp4','.ogg':'audio/ogg'},mime=file.type||mimeByExtension[extension]||'';const allowed:Record<string,string[]>={image:['image/png','image/jpeg','image/webp'],video:['video/mp4','video/quicktime','video/x-matroska'],audio:['audio/mpeg','audio/wav','audio/x-wav','audio/aac','audio/mp4','audio/ogg']},maxBytes=referenceType==='video'?200*1024*1024:referenceType==='audio'?15*1024*1024:30*1024*1024;assert(file.size<=maxBytes,`Choose a ${referenceType} smaller than ${referenceType==='video'?'200':referenceType==='audio'?'15':'30'} MB.`);assert(allowed[referenceType].includes(mime),referenceType==='image'?'Use PNG, JPG or WebP images.':referenceType==='video'?'Use MP4, MOV or MKV videos.':'Use MP3, WAV, AAC, M4A or OGG audio.');if(referenceType!=='image')assert(Number.isInteger(durationMs)&&durationMs>0&&durationMs<=30000,'Reference media must be between 0 and 30 seconds.');
   const bytes=new Uint8Array(await file.slice(0,16).arrayBuffer()),text=new TextDecoder().decode(bytes),valid=referenceType==='image'?(mime==='image/png'?bytes[0]===137&&bytes[1]===80&&bytes[2]===78&&bytes[3]===71:mime==='image/jpeg'?bytes[0]===255&&bytes[1]===216&&bytes[2]===255:text.slice(0,4)==='RIFF'&&text.slice(8,12)==='WEBP'):referenceType==='video'?(mime==='video/x-matroska'?bytes[0]===26&&bytes[1]===69&&bytes[2]===223&&bytes[3]===163:text.slice(4,8)==='ftyp'):(mime.includes('wav')?text.slice(0,4)==='RIFF'&&text.slice(8,12)==='WAVE':mime==='audio/ogg'?text.slice(0,4)==='OggS':mime==='audio/mp4'?text.slice(4,8)==='ftyp':mime==='audio/aac'?bytes[0]===255&&(bytes[1]&246)===240:text.slice(0,3)==='ID3'||(bytes[0]===255&&(bytes[1]&224)===224));assert(valid,'The file does not match its declared format.');
   const id=crypto.randomUUID(),objectKey=uid+'/'+id,kind=referenceType==='image'?'reference':'reference-'+referenceType;await bucket().put(objectKey,file.stream(),{httpMetadata:{contentType:mime}});await db.prepare('INSERT INTO files(id,user_id,name,mime,kind,object_key,duration_ms,created) VALUES(?,?,?,?,?,?,?,?)').bind(id,uid,file.name.slice(0,150),mime,kind,objectKey,referenceType==='image'?null:durationMs,Date.now()).run();return json({id,name:file.name,mime,kind,duration_ms:referenceType==='image'?null:durationMs});
 }
 const raw=await req.text();assert(raw.length<=100000,'Request is too large.');const b=JSON.parse(raw||'{}');
 if(op==='settings'){
  await rateLimit(uid,'settings',10);
  if(b.disconnect){await db.prepare('UPDATE accounts SET key_cipher=NULL,updated=? WHERE user_id=?').bind(Date.now(),uid).run();return json({ok:true});}
  assert(typeof b.key==='string'&&b.key.trim().length>=10&&b.key.length<=512,'Enter a valid KIE API key.');const key=b.key.trim();await kie('/api/v1/chat/credit',key);const encrypted=await cipher(key,uid);await db.prepare('INSERT INTO accounts(user_id,key_cipher,updated) VALUES(?,?,?) ON CONFLICT(user_id) DO UPDATE SET key_cipher=excluded.key_cipher,updated=excluded.updated').bind(uid,encrypted,Date.now()).run();return json({ok:true});
 }
 if(op==='skills'){
  await rateLimit(uid,'skills',20);if(b.deleteId){await db.prepare('DELETE FROM skills WHERE id=? AND user_id=?').bind(String(b.deleteId),uid).run();return json({ok:true});}
  assert(typeof b.name==='string'&&b.name.trim()&&b.name.length<=100,'Choose a skill name under 100 characters.');assert(typeof b.content==='string'&&b.content.trim()&&b.content.length<=50000,'Use skill instructions under 50,000 characters.');const id=crypto.randomUUID();await db.prepare('INSERT INTO skills(id,user_id,name,content,created) VALUES(?,?,?,?,?)').bind(id,uid,b.name.trim(),b.content,Date.now()).run();return json({id});
 }
 if(op==='delete-generation'){
  await rateLimit(uid,'delete-generation',30);
  assert(typeof b.id==='string'&&b.id.length<=100,'Choose a generation to delete.');
  const job=await db.prepare('SELECT id,status,images FROM jobs WHERE id=? AND user_id=?').bind(b.id,uid).first<any>();
  if(!job)return json({error:'Generation not found.'},404);
  assert(['success','fail','unknown'].includes(job.status),'Wait for this generation to finish before deleting it.');
  const ids=JSON.parse(job.images) as string[];
  for(const id of ids){
   const file=await db.prepare("SELECT object_key FROM files WHERE id=? AND user_id=? AND kind='generated'").bind(id,uid).first<any>();
   if(file)await bucket().delete(file.object_key);
  }
  await db.batch([...ids.map(id=>db.prepare("DELETE FROM files WHERE id=? AND user_id=? AND kind='generated'").bind(id,uid)),db.prepare('DELETE FROM jobs WHERE id=? AND user_id=?').bind(b.id,uid)]);
  return json({ok:true});
 }
 if(op==='delete-reference'){
  await rateLimit(uid,'delete-reference',30);
  const ids=[...new Set(Array.isArray(b.ids)?b.ids:[b.id])];assert(ids.length>0&&ids.length<=30&&ids.every((id:any)=>typeof id==='string'&&id.length<=100),'Choose up to 30 reference files to delete.');
  const placeholders=ids.map(()=>'?').join(','),files=await db.prepare(`SELECT id,object_key FROM files WHERE user_id=? AND kind LIKE 'reference%' AND id IN (${placeholders})`).bind(uid,...ids).all<any>();
  if(!files.results.length)return json({error:'Reference file not found.'},404);
  for(const file of files.results)await bucket().delete(file.object_key);
  await db.batch(files.results.map(file=>db.prepare("DELETE FROM files WHERE id=? AND user_id=? AND kind LIKE 'reference%'").bind(file.id,uid)));
  return json({ok:true,deleted:files.results.map(file=>file.id)});
 }
 if(op==='openai-settings'){
  await rateLimit(uid,'openai-settings',10);
  if(b.disconnect){await db.prepare('UPDATE accounts SET openai_key_cipher=NULL,updated=? WHERE user_id=?').bind(Date.now(),uid).run();return json({ok:true});}
  assert(typeof b.key==='string'&&b.key.trim().startsWith('sk-')&&b.key.length<=1024,'Enter a valid OpenAI API key.');
  const encrypted=await cipher(b.key.trim(),uid+':openai');
  await db.prepare('INSERT INTO accounts(user_id,openai_key_cipher,updated) VALUES(?,?,?) ON CONFLICT(user_id) DO UPDATE SET openai_key_cipher=excluded.openai_key_cipher,updated=excluded.updated').bind(uid,encrypted,Date.now()).run();
  return json({ok:true});
 }
 if(op==='prompt'){
  await rateLimit(uid,'prompt',8);
   assert(typeof b.idea==='string'&&b.idea.trim()&&b.idea.length<=6000,'Describe your idea in 6,000 characters or fewer.');
   const promptMedia=b.media==='video'?'video':'image';assert(Object.hasOwn(CAMERAS,b.camera)&&Object.hasOwn(FILMS,b.film),'Choose valid presets.');
    if(promptMedia==='video')assert(b.model==='bytedance/seedance-2-5'&&['480p','720p','1080p'].includes(b.resolution)&&['adaptive','21:9','16:9','4:3','1:1','3:4','9:16'].includes(b.aspect),'Choose valid Seedance settings.');else assert(MODELS.some(m=>m.id===b.model)&&['1K','2K','4K'].includes(b.resolution)&&ratios(b.model,b.resolution).includes(b.aspect),'Choose valid image settings.');
  assert(Array.isArray(b.references)&&b.references.length<=4&&b.references.every((id:any)=>typeof id==='string'),'Choose up to four references.');
   assert(['openai','kie'].includes(b.provider),'Choose OpenAI or KIE as the prompt provider.');
   const account=await db.prepare('SELECT key_cipher,openai_key_cipher FROM accounts WHERE user_id=?').bind(uid).first<any>();
   assert(b.provider==='kie'?account?.key_cipher:account?.openai_key_cipher,'Connect your '+(b.provider==='kie'?'KIE':'OpenAI')+' API key in account settings to build prompts.');
   const structured=b.media!=='video'&&b.skillId===BUILTIN_SKILL_ID;
  const context=structured?builtinContext(b.idea):b.skillId?await db.prepare('SELECT name,content FROM skills WHERE id=? AND user_id=?').bind(String(b.skillId),uid).first():null;
  assert(!b.skillId||context,'The selected skill is unavailable.');
   const images:string[]=[];const referenceFiles:any[]=[];let totalImageBytes=0;
  for(const id of b.references){
    const file=await db.prepare("SELECT object_key,mime FROM files WHERE id=? AND user_id=? AND kind IN ('reference','reference-image')").bind(id,uid).first<any>();assert(file,'A reference image is unavailable.');
   const obj=await bucket().get(file.object_key);assert(obj,'A reference image is unavailable.');totalImageBytes+=obj.size;assert(totalImageBytes<=8*1024*1024,'For prompt analysis, select reference images totaling 8 MB or less. Image generation supports larger references.');
    if(b.provider==='kie')referenceFiles.push({file:{...file,id,name:id},object:obj});else{const bytes=new Uint8Array(await obj.arrayBuffer());let binary='';for(let i=0;i<bytes.length;i+=8192)binary+=String.fromCharCode(...bytes.subarray(i,i+8192));images.push('data:'+file.mime+';base64,'+btoa(binary));}
   }
   let prompt:string;
   if(b.provider==='kie'){const key=await cipher(account.key_cipher,uid,true),promptModel=(b.promptModel||'gpt-5-6-sol') as KiePromptModel;assert(KIE_PROMPT_MODELS.includes(promptModel),'Choose a supported KIE prompt model.');for(const ref of referenceFiles)images.push(await uploadReferenceToKie(ref.file,ref.object,key));const promptContext={idea:b.idea,model:b.model,camera:b.camera,film:b.film,aspect:b.aspect,resolution:b.resolution,skill:context,structured,images,media:(b.media||'image') as 'image'|'video'};if(promptModel.startsWith('gpt-5-6-')){const result=await kie('/codex/v1/responses',key,buildCodexPromptRequest(promptContext,promptModel),120000);prompt=readCodexPromptResponse(result,structured);}else{const result=await kie('/'+promptModel+'/v1/chat/completions',key,buildPromptRequest(promptContext),90000);prompt=readPromptResponse(result,structured);}}
   else{const key=await cipher(account.openai_key_cipher,uid+':openai',true);const result=await callOpenAI(key,promptRequest({idea:b.idea,model:b.model,camera:b.camera,film:b.film,aspect:b.aspect,resolution:b.resolution,skill:context,structured,images,media:promptMedia}));prompt=readOpenAIPrompt(result,structured);}
   assert(composePrompt(prompt,b.camera,b.film,b.aspect).length<=(b.model==='nano-banana-pro'?10000:18000),'The generated prompt is too long for this model. Ask for a more concise description.');
  return json({prompt,sources:structured?(context as ReturnType<typeof builtinContext>).examples.map(e=>({title:e.title,author:e.author,source:e.source})):[]});
 }
   if(op==='generate-video'){
    await rateLimit(uid,'generate-video',4);assert(typeof b.prompt==='string'&&b.prompt.trim()&&b.prompt.length<=30000,'Write a video prompt of up to 30,000 characters.');assert(['480p','720p','1080p'].includes(b.resolution),'Choose 480p, 720p or 1080p.');assert(['adaptive','21:9','16:9','4:3','1:1','3:4','9:16'].includes(b.aspect),'Choose a supported video aspect ratio.');assert(Number.isInteger(b.duration)&&b.duration>=4&&b.duration<=30,'Choose a duration from 4 to 30 seconds.');assert(typeof b.generateAudio==='boolean','Choose whether to generate audio.');assert(typeof b.requestId==='string'&&/^[0-9a-f-]{36}$/.test(b.requestId),'Invalid request ID.');const imageRefs=b.imageReferences||b.references||[],videoRefs=b.videoReferences||[],audioRefs=b.audioReferences||[];assert(Array.isArray(imageRefs)&&imageRefs.length<=30&&imageRefs.every((r:any)=>typeof r==='string'),'Choose up to 30 reference images.');assert(Array.isArray(videoRefs)&&videoRefs.length<=10&&videoRefs.every((r:any)=>typeof r==='string'),'Choose up to 10 reference videos.');assert(Array.isArray(audioRefs)&&audioRefs.length<=10&&audioRefs.every((r:any)=>typeof r==='string'),'Choose up to 10 reference audio files.');assert(new Set([...imageRefs,...videoRefs,...audioRefs]).size===imageRefs.length+videoRefs.length+audioRefs.length,'Do not select the same reference more than once.');
   const key=await userKey(uid),requestKey=uid+':'+b.requestId,claim=await db.prepare('INSERT OR IGNORE INTO requests(id,user_id,created) VALUES(?,?,?)').bind(requestKey,uid,Date.now()).run();if(!claim.meta.changes)return json({jobs:await jobList(uid)});const active:any=await db.prepare("SELECT COUNT(*) AS count FROM jobs WHERE user_id=? AND status IN ('submitting','queued','generating','saving') AND created>?").bind(uid,Date.now()-3600000).first();assert(active.count<12,'Wait for an active generation to finish before starting another.');
    async function prepareReferences(ids:string[],kinds:string[]){const urls:string[]=[];let totalDuration=0;for(const referenceId of ids){const placeholders=kinds.map(()=>'?').join(','),file:any=await db.prepare(`SELECT * FROM files WHERE id=? AND user_id=? AND kind IN (${placeholders})`).bind(referenceId,uid,...kinds).first();assert(file,'A selected reference is unavailable. Select it again.');totalDuration+=Number(file.duration_ms||0);const object=await bucket().get(file.object_key);assert(object,'A selected reference is unavailable.');urls.push(await uploadReferenceToKie(file,object,key));}return {urls,totalDuration};}
    const imageData=await prepareReferences(imageRefs,['reference','reference-image']),videoData=await prepareReferences(videoRefs,['reference-video']),audioData=await prepareReferences(audioRefs,['reference-audio']);assert(videoData.totalDuration<=30000,'Reference videos may total no more than 30 seconds.');assert(audioData.totalDuration<=30000,'Reference audio may total no more than 30 seconds.');
    const id=crypto.randomUUID(),now=Date.now(),settings=JSON.stringify({kind:'video',resolution:b.resolution,aspect:b.aspect,duration:b.duration,generateAudio:b.generateAudio,imageReferences:imageRefs,videoReferences:videoRefs,audioReferences:audioRefs});await db.prepare('INSERT INTO jobs(id,user_id,request_id,model,prompt,final_prompt,settings,status,progress,created,updated) VALUES(?,?,?,?,?,?,?,?,?,?,?)').bind(id,uid,b.requestId,'bytedance/seedance-2-5',b.prompt.trim(),b.prompt.trim(),settings,'submitting',0,now,now).run();
    try{const input:any={prompt:b.prompt.trim(),reference_image_urls:imageData.urls,reference_video_urls:videoData.urls,reference_audio_urls:audioData.urls,return_last_frame:false,generate_audio:b.generateAudio,resolution:b.resolution,aspect_ratio:b.aspect,duration:b.duration};const result=await kie('/api/v1/jobs/createTask',key,{model:'bytedance/seedance-2-5',input});assert(result.data?.taskId,'KIE did not return a task ID.');await db.prepare('UPDATE jobs SET task_id=?,status=?,progress=?,updated=? WHERE id=? AND user_id=?').bind(result.data.taskId,'queued',1,Date.now(),id,uid).run();}catch(e:any){await db.prepare('UPDATE jobs SET status=?,error=?,updated=? WHERE id=? AND user_id=?').bind(e.definite?'fail':'unknown',e.definite?e.message:'The request may have reached KIE. Check your KIE task history before generating again to avoid duplicate charges.',Date.now(),id,uid).run();}return json({jobs:await jobList(uid)});
  }
  if(op==='generate'){
  await rateLimit(uid,'generate',8);assert(MODELS.some(m=>m.id===b.model),'Choose a supported model.');assert(['1K','2K','4K'].includes(b.resolution),'Choose a supported resolution.');assert(ratios(b.model,b.resolution).includes(b.aspect),'This aspect ratio is not available for the chosen model and resolution.');assert(typeof b.prompt==='string'&&b.prompt.trim()&&b.prompt.length<=18000,'Write a prompt of up to 18,000 characters.');assert(Object.hasOwn(CAMERAS,b.camera)&&Object.hasOwn(FILMS,b.film),'Choose valid camera and film presets.');assert(Number.isInteger(b.quantity)&&b.quantity>=1&&b.quantity<=4,'Choose between 1 and 4 images.');assert(typeof b.requestId==='string'&&/^[0-9a-f-]{36}$/.test(b.requestId),'Invalid request ID.');assert(Array.isArray(b.references)&&b.references.length<=4&&b.references.every((r:any)=>typeof r==='string'),'Choose up to 4 references.');
  if(b.prompt.trim().startsWith('{')){let profile;try{profile=JSON.parse(b.prompt);}catch{return json({error:'Your JSON prompt has a syntax error. Fix it before generating.'},400);}assert(profile&&typeof profile==='object'&&!Array.isArray(profile),'Use one JSON object.');}
  assert(composePrompt(b.prompt,b.camera,b.film,b.aspect).length<=(b.model==='nano-banana-pro'?10000:20000),'The final prompt is too long for this model. Shorten it and try again.');const key=await userKey(uid);const requestKey=uid+':'+b.requestId;const claim=await db.prepare('INSERT OR IGNORE INTO requests(id,user_id,created) VALUES(?,?,?)').bind(requestKey,uid,Date.now()).run();if(!claim.meta.changes)return json({jobs:await jobList(uid)});
  const active:any=await db.prepare("SELECT COUNT(*) AS count FROM jobs WHERE user_id=? AND status IN ('submitting','queued','generating','saving') AND created>?").bind(uid,Date.now()-3600000).first();assert(active.count+b.quantity<=12,'Wait for a few active generations to finish before starting more.');
  const referenceUrls=await Promise.all(b.references.map(async(id:string)=>{const file:any=await db.prepare("SELECT * FROM files WHERE id=? AND user_id=? AND kind='reference'").bind(id,uid).first();assert(file,'A reference image is unavailable. Select it again.');const object=await bucket().get(file.object_key);assert(object,'A reference image is unavailable.');return uploadReferenceToKie(file,object,key);}));
  const now=Date.now();const finalPrompt=composePrompt(b.prompt,b.camera,b.film,b.aspect);const settings=JSON.stringify({kind:'image',resolution:b.resolution,aspect:b.aspect,camera:b.camera,film:b.film,references:b.references});const ids=Array.from({length:b.quantity},()=>crypto.randomUUID());await db.batch(ids.map(id=>db.prepare('INSERT INTO jobs(id,user_id,request_id,model,prompt,final_prompt,settings,status,created,updated) VALUES(?,?,?,?,?,?,?,?,?,?)').bind(id,uid,b.requestId,b.model,b.prompt,finalPrompt,settings,'submitting',now,now)));
  await Promise.all(ids.map(async id=>{try{const input:any={prompt:finalPrompt,aspect_ratio:b.aspect,resolution:b.resolution};let model=b.model;if(model==='nano-banana-pro'){input.image_input=referenceUrls;input.output_format='png';}else{model+=referenceUrls.length?'-image-to-image':'-text-to-image';if(referenceUrls.length)input.input_urls=referenceUrls;}
   const result=await kie('/api/v1/jobs/createTask',key,{model,input});assert(result.data?.taskId,'KIE did not return a task ID.');await db.prepare('UPDATE jobs SET task_id=?,status=?,updated=? WHERE id=? AND user_id=?').bind(result.data.taskId,'queued',Date.now(),id,uid).run();
  }catch(e:any){await db.prepare('UPDATE jobs SET status=?,error=?,updated=? WHERE id=? AND user_id=?').bind(e.definite?'fail':'unknown',e.definite?e.message:'The request may have reached KIE. Check your KIE task history before generating again to avoid duplicate charges.',Date.now(),id,uid).run();}}));return json({jobs:await jobList(uid)});
 }
 if(op==='poll'){
  await rateLimit(uid,'poll',20);const key=await userKey(uid);const active=await db.prepare("SELECT * FROM jobs WHERE user_id=? AND status IN ('queued','generating','saving') ORDER BY created DESC LIMIT 12").bind(uid).all();
  // Process a bounded batch to keep generated image buffers below Worker memory limits.
  for(let i=0;i<active.results.length;i+=2){await Promise.all(active.results.slice(i,i+2).map(async j=>{try{await refreshJob(j,key);}catch(e:any){await db.prepare('UPDATE jobs SET error=? WHERE id=? AND user_id=?').bind(String(e.message||'Could not refresh. Trying again shortly.').replaceAll(key,'[redacted]'),j.id,uid).run();}}));}
  await db.prepare("UPDATE jobs SET status='unknown',error='Submission was interrupted. Check your KIE history before retrying.' WHERE user_id=? AND status='submitting' AND created<?").bind(uid,Date.now()-120000).run();return json({jobs:await jobList(uid)});
 }
 return json({error:'Not found.'},404);
 }catch(e:any){return json({error:e instanceof SyntaxError?'Invalid request.':e.message||'The service is temporarily unavailable.'},400);}
}
export const GET=handle;export const POST=handle;
