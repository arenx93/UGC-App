import examples from './skill-data/examples.json';
import profile from './skill-data/json-profile.json';

export const BUILTIN_SKILL_ID='builtin-json-image';
export const BUILTIN_SKILL_NAME='JSON visual profile + GPT Image 2 examples';
export function relevantExamples(idea:string){
 const words=[...new Set(idea.toLowerCase().match(/[\p{L}\p{N}]{3,}/gu)||[])].filter(w=>!['the','and','with','for','image','photo','make','create'].includes(w));
 const seen=new Set<string>();
 return examples.filter(e=>{if(seen.has(e.title))return false;seen.add(e.title);return true;}).map(e=>({e,score:words.reduce((s,w)=>s+(e.title.toLowerCase().includes(w)?5:0)+(e.prompt.toLowerCase().includes(w)?1:0),0)})).filter(e=>e.score>0).sort((a,b)=>b.score-a.score).slice(0,3).map(({e})=>({...e,prompt:e.prompt.slice(0,6500)}));
}
export function builtinContext(idea:string){return {profile,examples:relevantExamples(idea),attribution:'Examples adapted from YouMind OpenLab, Awesome GPT Image 2, CC BY 4.0. Original authors and sources accompany selected examples.',source:'https://github.com/YouMind-OpenLab/awesome-gpt-image-2',license:'https://creativecommons.org/licenses/by/4.0/'};}
