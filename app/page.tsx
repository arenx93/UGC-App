import Studio from './studio';
import {getChatGPTUser} from './chatgpt-auth';
import {headers} from 'next/headers';
export const dynamic='force-dynamic';
export default async function Home(){const user=await getChatGPTUser(),requestHeaders=await headers(),desktop=requestHeaders.get('x-framecraft-desktop')==='1',local=requestHeaders.get('x-framecraft-local-browser')==='1';return <Studio user={user?{name:user.displayName,email:user.email}:null} desktop={desktop} local={local}/>;}
