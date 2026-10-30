// 用原生 EqBand 导出的整套边界组合逐点对照网页即时响应。
const fs=require('node:fs'),assert=require('node:assert/strict');
const {pathToFileURL}=require('node:url');
(async()=>{
 const {responseCurve}=await import(pathToFileURL(__dirname+'/../ui/response.js'));
 const fixture=JSON.parse(fs.readFileSync(process.argv[2]));let worst=0,caseInfo='';
 for(const sample of fixture){const computed=responseCurve(sample.type,2310,sample.gain,sample.q,sample.slope);for(let i=0;i<computed.length;i++){
  const delta=Math.abs(computed[i]-sample.response[i]);if(delta>worst){worst=delta;caseInfo=JSON.stringify({type:sample.type,q:sample.q,gain:sample.gain,slope:sample.slope,index:i});}
  assert(Number.isFinite(delta),'曲线存在非有限数值');
 }}
 assert(worst<.013,`网页响应与 DSP 解析曲线不一致，最大 ${worst}：${caseInfo}`);
 console.log(`通过：${fixture.length} 组、${fixture.length*320} 点响应比对，最大偏差 ${worst.toFixed(5)} dB`);
})().catch(error=>{console.error(error);process.exit(1)});
