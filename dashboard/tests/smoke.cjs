// Headless behavior checks: no visible browser or third-party DOM dependency.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const elements = new Map();
const dashedPoints = [];
let dashed = false;
const canvasContext = new Proxy({}, {get(_target, name) {
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
