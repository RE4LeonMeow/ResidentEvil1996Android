const test=require('node:test');
const assert=require('node:assert/strict');
const {version,newer,result}=require('../../android/app/src/main/assets/web/updates-core.js');

test('stable releases compare numerically, including early Android labels',()=>{
  for(const old of ['v1.00','1.01','v1.02','1.0.9','v1.1','1.1.0','v1.1.1','v1.2','v1.10','v1.11'])assert.equal(newer(old),false,old);
  for(const next of ['v1.11.1','v1.12','2.0'])assert.equal(newer(next),true,next);
  assert.equal(newer('v1.1','1.01'),true);
  assert.equal(newer('v1.02','1.01'),true);
  assert.deepEqual(version(' v1.1.2 '),[1,1,2]);
});
test('draft, prerelease and unrecognized tags never advertise an update',()=>{
  const release={tag_name:'v1.12',draft:false,prerelease:false};
  assert.equal(result({state:'ok',release}).state,'available');
  for(const invalid of ['v1.2-beta','1','latest','v1.2.3.4','1e2.0','<script>']) {
    assert.equal(newer(invalid),null);
    assert.equal(result({state:'ok',release:{...release,tag_name:invalid}}).state,'unavailable');
  }
  for(const change of [{draft:true},{prerelease:true},{draft:undefined},{prerelease:undefined}])
    assert.equal(result({state:'ok',release:{...release,...change}}).state,'unavailable');
});
test('offline, errors and empty repositories do not claim to be up to date',()=>{
  assert.equal(result({state:'offline'}).text,'Offline mode');
  assert.equal(result({state:'no-release'}).state,'no-release');
  for(const value of [null,{}, {state:'unavailable'}, {state:'ok'}])assert.equal(result(value).state,'unavailable');
  assert.equal(result({state:'ok',release:{tag_name:'v1.01',draft:false,prerelease:false}}).state,'current');
});
