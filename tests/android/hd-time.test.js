const test=require('node:test'),assert=require('node:assert/strict');
const {BuildClock,duration}=require('../../android/app/src/main/assets/web/hd-time.js');
test('HD elapsed time and ETA use measured pixel work, including partial-image waits',()=>{
  const clock=new BuildClock(0);
  assert.equal(clock.text(2250),'Elapsed 00:03 · Estimating time remaining…');
  clock.update({phase:'build',doneWork:0,totalWork:1000},3000);
  clock.update({phase:'build',doneWork:250,totalWork:1000},8000);
  assert.equal(clock.text(9000),'Elapsed 00:09 · About 00:14 remaining');
  clock.update({phase:'build',doneWork:500,totalWork:1000},10000);
  assert.equal(clock.text(11000),'Elapsed 00:11 · About 00:06 remaining');
  assert.match(clock.text(20000),/Updating estimate/);
  clock.update({phase:'finish',doneWork:1000,totalWork:1000},21000);
  assert.equal(clock.text(22000),'Elapsed 00:22 · Saving pack…');
});
test('HD estimates remain unknown before enough work and support long phone builds',()=>{
  const clock=new BuildClock(0);
  clock.update({phase:'build',doneWork:0,totalWork:100},1000);
  assert.match(clock.text(10000),/Estimating/);
  clock.update({phase:'build',doneWork:1,totalWork:100},1500);
  assert.match(clock.text(2000),/Estimating/);
  assert.equal(duration(3661000),'1:01:01');assert.equal(duration(-5),'00:00');
});
