import './sites-env.mjs';
import {existsSync,readFileSync,writeFileSync} from 'node:fs';
import {randomBytes} from 'node:crypto';
import {spawnSync} from 'node:child_process';

if(!existsSync('.env')) {
 if(existsSync('.wrangler/state'))throw Error('Falta .env pero existen datos guardados. Restaure .env desde su copia de seguridad.');
 writeFileSync('.env',`KEY_ENCRYPTION_SECRET=${randomBytes(32).toString('base64')}\n`,{flag:'wx'});
}
const secret=readFileSync('.env','utf8').match(/^KEY_ENCRYPTION_SECRET=(.+)$/m)?.[1]?.trim();
if(!secret||Buffer.from(secret,'base64').length!==32)throw Error('La clave de cifrado de .env no es valida. No la reemplace si ya tiene datos guardados.');
const result=spawnSync(process.execPath,['node_modules/wrangler/bin/wrangler.js','d1','migrations','apply','DB','--local','--config','wrangler.local.json','--persist-to','.wrangler/state'],{stdio:'inherit',env:{...process.env,CI:'true'}});
if(result.error)throw result.error;
if(result.status!==0)process.exit(result.status||1);

