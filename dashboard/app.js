"use strict";
const $ = id => document.getElementById(id);
const canvas = $("world"), ctx = canvas.getContext("2d");
let events = [], metadata, cursor = 0, playing = false, previous = 0, lastDraw = 0;
let state;
function reset() { state = {observation:null, mode:"LATCHED", epoch:0, decisions:0, rejected:0, reasons:[], path:[]}; }
reset();
function apply(event) {
  const p = event.payload;
  if (event.kind === "observation") { state.observation = p; state.path.push([p.x,p.y]); }
  if (event.kind === "gate_transition") { state.mode = p.to; state.epoch = p.epoch ?? state.epoch; }
  if (event.kind === "decision") { state.decisions++; state.rejected += p.verdict !== "AUTHORIZED" ? 1 : 0; }
  const reason = p.primary_reason || p.reason || (event.kind === "fault" ? `${p.fault} ${p.phase}` : null);
  if (reason && reason !== "OK_AUTHORIZED" && reason !== "GATE_LEASE_ACCEPTED") {
    state.reasons.unshift({time:event.t_ns, text:reason.replaceAll("_", " ").toLowerCase()});
    state.reasons.length = Math.min(state.reasons.length,40);
  }
}
function seek(time) {
  cursor = Math.max(0, Math.min(time, events.at(-1)?.t_ns || 0));
  reset();
  for (const event of events) { if (event.t_ns > cursor) break; apply(event); }
  $("seek").value = cursor;
  $("time").textContent = (cursor/1e9).toFixed(2)+" s";
  $("mode").textContent = state.mode;
  $("epoch").textContent = state.epoch || "—";
  $("velocity").textContent = state.observation ? state.observation.v_meas.toFixed(2)+" m/s" : "—";
  $("decisions").textContent = state.decisions;
  $("rejected").textContent = state.rejected;
  $("journal").replaceChildren(...state.reasons.map(reason => {
    const item = document.createElement("li"), stamp = document.createElement("span");
    stamp.textContent = (reason.time/1e9).toFixed(2)+" s";
    item.append(stamp, document.createTextNode(reason.text)); return item;
  }));
  draw();
}
function draw() {
  if (!metadata) return;
  const map = metadata.map, pad = 45;
  const scale = Math.min((canvas.width-2*pad)/map.size[0],(canvas.height-2*pad)/map.size[1]);
  const left=(canvas.width-map.size[0]*scale)/2, bottom=(canvas.height+map.size[1]*scale)/2;
  const point = (x,y) => [left+x*scale, bottom-y*scale];
  ctx.clearRect(0,0,canvas.width,canvas.height);
  ctx.strokeStyle="#1d333e"; ctx.lineWidth=1;
  for(let x=0;x<=map.size[0];x+=2){ctx.beginPath();ctx.moveTo(...point(x,0));ctx.lineTo(...point(x,map.size[1]));ctx.stroke();}
  for(let y=0;y<=map.size[1];y+=2){ctx.beginPath();ctx.moveTo(...point(0,y));ctx.lineTo(...point(map.size[0],y));ctx.stroke();}
  const rectangle = (r,color) => {const p=point(r.min[0],r.max[1]);ctx.fillStyle=color;ctx.fillRect(...p,(r.max[0]-r.min[0])*scale,(r.max[1]-r.min[1])*scale);};
  for(const wall of map.walls) rectangle(wall,"#536b79");
  for(const zone of map.zones){let active=Array.isArray(zone.active) ? zone.active.some(window=>cursor/1e9>=window.from_s && (window.to_s==null || cursor/1e9<window.to_s)):zone.active;
    for(const event of metadata.zone_events || [])if(event.zone===zone.id && event.t_s<=cursor/1e9)active=event.active;
    rectangle(zone.rect,!active ? "#31434366":zone.kind==="restricted" ? "#cb9b4133":"#31564f44");const p=point(zone.rect.min[0],zone.rect.max[1]);ctx.fillStyle="#d7bd7e";ctx.font="12px system-ui";ctx.fillText(zone.id,...[p[0]+5,p[1]+16]);}
  ctx.strokeStyle="#35776f";ctx.lineWidth=2;ctx.beginPath();state.path.forEach((p,i)=>i ? ctx.lineTo(...point(...p)):ctx.moveTo(...point(...p)));ctx.stroke();
  const o=state.observation;
  if(o){for(const obstacle of o.obstacles || []){ctx.fillStyle="#ecad78";ctx.beginPath();ctx.arc(...point(obstacle.x,obstacle.y),obstacle.r*scale,0,Math.PI*2);ctx.fill();}
    const [x,y]=point(o.x,o.y);ctx.strokeStyle="#8adccc";ctx.fillStyle="#47b7a64d";ctx.lineWidth=2;ctx.beginPath();ctx.arc(x,y,0.35*scale,0,Math.PI*2);ctx.fill();ctx.stroke();ctx.beginPath();ctx.moveTo(x,y);ctx.lineTo(x+Math.cos(o.theta)*0.65*scale,y-Math.sin(o.theta)*0.65*scale);ctx.stroke();}
  const goal=point(map.goal.x,map.goal.y);ctx.strokeStyle="#91a5b1";ctx.strokeRect(goal[0]-6,goal[1]-6,12,12);
}
$("play").onclick=()=>{if(cursor>=Number($("seek").max))seek(0);playing=!playing;$("play").textContent=playing ? "Pause":"Play";};
$("seek").oninput=event=>seek(Number(event.target.value));
function frame(now){const dt=previous ? now-previous:0;previous=now;if(playing){cursor+=dt*1e6*Number($("speed").value);if(now-lastDraw>33){seek(cursor);lastDraw=now;if(cursor>=Number($("seek").max)){playing=false;$("play").textContent="Play";}}}requestAnimationFrame(frame);}
async function start(){metadata=await (await fetch("/meta.json")).json();$("map-title").textContent=metadata.map.title;$("source").textContent=metadata.source;const socket=new WebSocket(`ws://${location.hostname}:${metadata.websocket_port}/events`);
  socket.onmessage=event=>{const packet=JSON.parse(event.data);if(packet.type==="events")events.push(...packet.events);if(packet.type==="end"){events.sort((a,b)=>a.t_ns-b.t_ns);$("seek").max=events.at(-1)?.t_ns || 0;$("connection").textContent="Replay ready";seek(0);}};
  socket.onerror=()=>{$("connection").textContent="Relay unavailable";};
  requestAnimationFrame(frame);
}
start().catch(error=>{$("connection").textContent=error.message;});
