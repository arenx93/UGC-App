// Workers supports only manual/follow. Inspect redirects explicitly, keep HTTPS,
// and never forward the user's KIE credential to an image host.
export async function downloadImage(source:string,fetcher:typeof fetch=fetch,timeoutMs=45000){
 let url=new URL(source);
 const signal=AbortSignal.timeout(timeoutMs);
 for(let redirects=0;redirects<=4;redirects++){
  if(url.protocol!=='https:'||url.username||url.password)throw Error('KIE returned an unsupported image URL.');
  const response=await fetcher(url,{signal,redirect:'manual'});
  if(![301,302,303,307,308].includes(response.status))return response;
  const location=response.headers.get('location');
  await response.body?.cancel();
  if(!location)throw Error('The image download redirect has no destination.');
  url=new URL(location,url);
 }
 throw Error('The image download has too many redirects.');
}
