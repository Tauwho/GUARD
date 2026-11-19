// Headless behavior checks: no visible browser or third-party DOM dependency.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const elements = new Map();
const dashedPoints = [], fills = [];
let dashed = false, draws = 0;
const canvasContext = new Proxy({}, {get(target, name) {
  if (name === 'clearRect') return () => {draws++;};
  if (name === 'fillRect') return () => {fills.push(target.fillStyle);};
  if (name === 'setLineDash') return values => {dashed = values.length > 0;};
  if (name === 'moveTo') return (...point) => {if (dashed) dashedPoints.push(point);};
  return () => {};
}});
function element(id) {
  if (!elements.has(id)) elements.set(id, {value: id === 'speed' ? 1 : 0, width: 1200,
    height: 430, dataset: {}, children: [], textContent: '',
    getContext: () => canvasContext, append(...children) {this.children.push(...children);},
    replaceChildren(...children) {this.children = children;}});
  return elements.get(id);
}
const context = vm.createContext({document: {getElementById: element,
  createElement: () => element(Symbol()), createTextNode: text => ({textContent: text})},
  requestAnimationFrame: () => {}, fetch: () => new Promise(() => {}), console});
vm.runInContext(fs.readFileSync(path.join(__dirname, '../app.js'), 'utf8'), context);
vm.runInContext(`
metadata = {map:{size:[20,10],walls:[],zones:[],goal:{x:19,y:9}}};
events = [
 {t_ns:0,kind:'gate_transition',payload:{to:'MOVING',epoch:2}},
 {t_ns:0,kind:'observation',payload:{x:10,y:5,theta:0,v_meas:0.4}},
 {t_ns:0,kind:'lease',payload:{lease_id:1,issued_ns:0,expires_ns:100000000,
   x:1,y:2,theta:0,d_req:1,v_cmd:0.4,w_cmd:0}},
 {t_ns:100000000,kind:'gate_transition',payload:{to:'LATCHED',reason:'GATE_SUPERVISOR_LOST'}}
];
seek(50000000);
`, context);
assert.equal(element('lease').value, 0.5);
assert.match(element('lease-detail').textContent, /50 ms/);
assert.deepEqual(dashedPoints[0], [294, 317]); // Source pose, not current observation.
vm.runInContext('seek(100000000);', context);
assert.equal(element('mode').textContent, 'LATCHED');
assert.equal(element('lease').value, 0);
assert.equal(element('journal').children.length, 1);
assert.match(element('journal').children[0].children[1].textContent, /Supervisor connection lost/);
vm.runInContext('seek(0);', context);
assert.equal(element('mode').textContent, 'MOVING');
console.log('Dashboard headless smoke: PASS (seek, lease, source arc, latch, reason feed)');

// Target traces stamp CLOCK_MONOTONIC since boot: scenario time starts at run_start.
vm.runInContext(`
metadata = {map:{size:[20,10],walls:[],goal:{x:19,y:9},
  zones:[{id:'mri',kind:'restricted',rect:{min:[1,1],max:[2,2]},active:[{from_s:1,to_s:null}]}]}};
events = [
 {t_ns:5000000000,kind:'run_start',payload:{}},
 {t_ns:5500000000,kind:'gate_transition',payload:{to:'LATCHED',reason:'GATE_SUPERVISOR_LOST'}},
 {t_ns:6500000000,kind:'observation',payload:{x:10,y:5,theta:0,v_meas:0}}
];
`, context);
fills.length = 0;
vm.runInContext('seek(0);', context);
assert.equal(vm.runInContext('cursor', context), 5000000000);
assert.equal(element('time').textContent, '0.00 s');
assert.equal(fills.at(-1), '#31434366'); // Zone scheduled from 1 s is not active yet.
vm.runInContext('seek(6500000000);', context);
assert.equal(element('time').textContent, '1.50 s');
assert.equal(fills.at(-1), '#cb9b4133');
assert.equal(element('journal').children[0].children[0].textContent, '0.50 s');
console.log('Boot-clock trace smoke: PASS (run_start origin for clock, zones and journal)');

// A flood is ingested immediately but drawn once per animation frame.
draws = 0;
vm.runInContext(`
metadata.live=true; events=[]; livePending=[];
for(let i=0;i<1000;i++) receiveLive([{t_ns:i,kind:'status',payload:{}}]);
`, context);
assert.equal(draws, 0);
vm.runInContext('frame(1000);', context);
assert.equal(draws, 1);
assert.equal(vm.runInContext('events.length', context), 1000);
vm.runInContext(`receiveLive(Array.from({length:25000},(_,i)=>({t_ns:i,kind:'status',payload:{}})));frame(1100);`, context);
assert.equal(vm.runInContext('events.length', context), 20000);
assert.equal(Number(element('seek').max), 24999);
vm.runInContext(`followLive=false;receiveLive([{t_ns:25000,kind:'status',payload:{}}]);`, context);
const pausedDraws = draws;
vm.runInContext('frame(1200);', context);
assert.equal(draws, pausedDraws);
assert.equal(vm.runInContext('events.at(-1).t_ns', context), 25000);
console.log('Live burst smoke: PASS (bounded ingestion, one redraw/frame, pause preserved)');

// A queued frame must not conceal a newer relay/socket error.
vm.runInContext(`followLive=true;receiveLive([{t_ns:25001,kind:'status',payload:{}}]);`, context);
element('connection').textContent = 'Relay unavailable';
vm.runInContext('frame(1300);', context);
assert.equal(element('connection').textContent, 'Relay unavailable');
console.log('Live error ordering: PASS');
