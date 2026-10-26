// Port-owned stable release comparison, shared with launcher tests.
(function(root){
  'use strict';
  const CURRENT='1.1';
  function version(value){
    if(typeof value!=='string')return null;
    const match=/^v?(\d{1,4})\.(\d{1,4})(?:\.(\d{1,4}))?$/.exec(value.trim());
    if(!match)return null;
    // Early public versions used 1.00/1.01/1.02. They precede 1.1.
    if(!match[3]&&match[1]==='1'&&/^0\d$/.test(match[2]))return [1,0,Number(match[2])];
    return [Number(match[1]),Number(match[2]),Number(match[3]||0)];
  }
  function newer(candidate,current=CURRENT){
    const a=version(candidate),b=version(current);
    if(!a||!b)return null;
    for(let i=0;i<3;i++)if(a[i]!==b[i])return a[i]>b[i];
    return false;
  }
  function result(reply){
    if(reply?.state==='offline')return {state:'offline',text:'Offline mode'};
    if(reply?.state==='no-release')return {state:'no-release',text:'No release published yet'};
    if(reply?.state!=='ok')return {state:'unavailable',text:'Couldn’t check for updates'};
    const release=reply.release;
    if(!release||release.draft!==false||release.prerelease!==false||!version(release.tag_name))
      return {state:'unavailable',text:'Couldn’t check for updates'};
    return newer(release.tag_name)?{state:'available',text:'Update available · '+release.tag_name.trim()}
      :{state:'current',text:'Up to date · '+CURRENT};
  }
  const api={CURRENT,version,newer,result};
  if(typeof module==='object'&&module.exports)module.exports=api;
  else root.RE1UpdateCore=api;
})(typeof globalThis==='object'?globalThis:this);
