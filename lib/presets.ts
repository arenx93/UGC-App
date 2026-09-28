export const MODELS = [
  {id:'gpt-image-2', name:'GPT Image 2', note:'Versatile image creation'},
  {id:'gpt-image-2-5-flare', name:'GPT Image 2.5 · Flare', note:'Creative image generation'},
  {id:'gpt-image-2-5-sunburst', name:'GPT Image 2.5 · Sunburst', note:'Detailed image generation'},
  {id:'nano-banana-pro', name:'Nano Banana Pro', note:'Reference-driven creation'},
];
export const CAMERAS: Record<string,string> = {
 'N/A':'',
 'Android Camera':'Low-quality Android phone snapshot: slightly soft focus, subtle motion blur, visible digital noise, compressed detail and imperfect exposure. Natural, unpolished everyday photography.',
 'iPhone Camera':'Candid iPhone snapshot with natural smartphone processing, realistic skin texture, available light and spontaneous everyday framing. Avoid a polished studio look.',
 'TV / News Camera':'Local television news camera footage captured as a still: documentary framing, on-location broadcast lighting, slightly compressed video detail and realistic local-news color. No station logo, lower third or text unless requested.',
};
export const FILMS: Record<string,string> = {
 'N/A':'',
 'POV':'First-person POV through a handheld phone at 1× magnification. Exactly one hand is visible; the other hand is holding the phone outside the frame. Slight handheld motion blur, eye-level perspective and natural framing. No extra hands or fingers.',
 'Selfie':'Front-facing phone selfie, candid expression, imperfect slightly off-center framing, natural arm-length perspective and spontaneous composition. Unretouched everyday appearance.',
};
export function composePrompt(prompt:string,camera:string,film:string,aspect?:string){
 const text=prompt.trim();
 if(!text.startsWith('{'))return [text,CAMERAS[camera],FILMS[film]].filter(Boolean).join('\n\n');
 let p:any;try{p=JSON.parse(text);}catch{return text;}
 if(!p||Array.isArray(p))return text;
 const object=(v:any)=>v&&typeof v==='object'&&!Array.isArray(v)?v:{};
 if(aspect&&aspect!=='auto')p.composition={...object(p.composition),aspect_ratio:aspect};
 if(!CAMERAS[camera]&&!FILMS[film])return JSON.stringify(p);
 p.technical_specs={...object(p.technical_specs)};
 if(CAMERAS[camera]){
  p.technical_specs.camera_style=CAMERAS[camera];
  if(camera==='Android Camera')Object.assign(p.technical_specs,{sharpness:'Slightly soft focus and subtle motion blur',grain:'Visible digital noise',texture:'Compressed detail, imperfect exposure'});
  if(camera==='iPhone Camera')Object.assign(p.technical_specs,{sharpness:'Natural smartphone detail, realistic skin texture',grain:'Subtle natural phone noise',texture:'Unretouched candid detail'});
  if(camera==='TV / News Camera')Object.assign(p.technical_specs,{sharpness:'Slightly compressed broadcast video detail',texture:'Local-news documentary footage'});
 }
 if(FILMS[film]){
  p.composition={...object(p.composition),framing:FILMS[film]};
  p.technical_specs.perspective=film==='POV'?'First-person eye-level handheld phone, 1x magnification':'Front-facing phone selfie at arm’s length, imperfect off-center framing';
  if(film==='POV'){
   p.technical_specs.motion_blur='Slight handheld motion blur';
   p.subject_analysis={...object(p.subject_analysis),hands_and_gestures:{left_hand:'Outside the frame, holding the phone',right_hand:'The only visible hand; natural gesture consistent with the scene',finger_positions:'Natural anatomy, no extra fingers',interaction:'Exactly one visible hand; phone-holding hand stays out of frame',visible_hand_count:1}};
  }
 }
 p.framecraft_overrides={priority:'These selected camera and framing constraints override conflicting descriptions elsewhere in this profile, including generation_parameters.prompts.',...(CAMERAS[camera]?{camera:CAMERAS[camera]}:{}),...(FILMS[film]?{film:FILMS[film]}:{})};
 return JSON.stringify(p);
}
export function ratios(model:string,resolution:string){
 if(model.startsWith('gpt-image-2-5'))return ['auto','1:1','3:2','2:3','4:3','3:4','16:9','9:16','21:9',...(resolution==='1K'?['27:16','16:27','9:8','8:9']:[])];
 const all=model==='nano-banana-pro'?['1:1','2:3','3:2','3:4','4:3','4:5','5:4','9:16','16:9','21:9']:['auto','1:1','3:2','2:3','4:3','3:4','5:4','4:5','16:9','9:16','2:1','1:2','3:1','1:3','21:9','9:21'];
 return all.filter(r=> !(model==='gpt-image-2'&&((resolution!=='1K'&&r==='auto')||(resolution==='4K'&&r==='1:1')))).filter(r=>model==='nano-banana-pro'|| !(resolution==='2K'?['5:4','4:5','3:1','1:3','9:21']:resolution==='4K'?['3:1','1:3','9:21']:[]).includes(r));
}
