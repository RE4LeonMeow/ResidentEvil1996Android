const {test}=require('node:test');
const assert=require('node:assert/strict');
const {indexFiles}=require('../../android/app/src/main/assets/web/browser-storage.js');
const names=['fontus.tim','font03t.tim','item_all.pix','optkey03.tim','status.tim','statface.tim','blue.tim','staitem.tim','kage.tim'];
function fixture(prefix='GOG/Resident Evil/USA/'){
  return [...names.map(n=>({path:prefix+'Data/'+n,file:new File([new Uint8Array(8)],n)})),{path:prefix+'Stage1/ROOM1000.RDT',file:new File(['room'],'ROOM1000.RDT')}];
}
test('folder indexing strips the installation wrapper, keeps files and validates the complete boot set',()=>{
  const selected=indexFiles(fixture());assert.equal(selected.files.length,10);assert.equal(selected.name,'USA');assert.equal(selected.total,76);
  assert.equal(selected.files[0].path,'Data/fontus.tim');assert.equal(selected.files[9].path,'Stage1/ROOM1000.RDT');
  assert.equal(indexFiles(selected.files).files.length,10);
});
test('selection requires one installation and rejects ambiguous case aliases and unsafe paths',()=>{
  assert.throws(()=>indexFiles([]),/fontus/);
  assert.throws(()=>indexFiles([...fixture('one/'),...fixture('two/')]),/Multiple/);
  assert.throws(()=>indexFiles([...fixture(),{path:'GOG/Resident Evil/USA/data/FONTUS.tim',file:new File(['x'],'alias')}]),/Duplicate/);
  for(const path of ['/absolute','root/../escape','root/./escape','root//escape','bad\0path'])assert.throws(()=>indexFiles([{path,file:new File(['x'],'bad')}]),/Invalid/);
});
test('empty boot resources and missing room trees fail before the engine is loaded',()=>{
  const files=fixture();assert.throws(()=>indexFiles(files.slice(0,-1)),/room files/);
  files[1].file=new File([],'empty');assert.throws(()=>indexFiles(files),/missing or empty/);
});
