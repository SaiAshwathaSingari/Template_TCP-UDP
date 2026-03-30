const crypto = require('crypto');
const k = 'dGhlIHNhbXBsZSBub25jZQ==';
const m = '258EAFA5-E914-47DA-95CA-5AB4AA29BE5E';
const combined = k + m;
console.log('Input:', JSON.stringify(combined));
console.log('Length:', combined.length);
console.log('SHA1 hex:', crypto.createHash('sha1').update(combined).digest('hex'));
console.log('SHA1 b64:', crypto.createHash('sha1').update(combined).digest('base64'));
console.log('Expected: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=');
