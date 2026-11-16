"use strict";
const $ = id => document.getElementById(id);
const canvas = $("world"), ctx = canvas.getContext("2d");
let events = [], metadata, cursor = 0, playing = false, previous = 0, lastDraw = 0;
let state;
let followLive=true, dropped=0;
function reset() { state = {observation:null, lease:null, mode:"UNKNOWN", epoch:0, decisions:0, rejected:0, reasons:[], path:[]}; }
reset();
function apply(event) {
  const p = event.payload;
  if (event.kind === "observation") { state.observation = p; state.path.push([p.x,p.y]); }
  if (event.kind === "gate_transition") { state.mode = p.to; state.epoch = p.epoch ?? state.epoch; if(p.to!=="MOVING")state.lease=null; }
  if (event.kind === "lease" && Number.isFinite(p.expires_ns) && Number.isFinite(p.issued_ns)) state.lease=p;
  if (event.kind === "revoke")state.lease=null;
  if (event.kind === "decision") { state.decisions++; state.rejected += p.verdict !== "AUTHORIZED" ? 1 : 0; }
  const reason = p.primary_reason || p.reason || (event.kind === "fault" ? `${p.fault} ${p.phase}` : null);
  if (reason && reason !== "OK_AUTHORIZED" && reason !== "GATE_LEASE_ACCEPTED") {
    const words={GATE_SUPERVISOR_LOST:"Supervisor connection lost",GATE_STANDSTILL:"Robot standstill confirmed",GATE_LEASE_EXPIRED:"Motion lease expired",GATE_REVOKED:"Supervisor withdrew motion permission",GATE_EPOCH_CHANGED:"Replacement supervisor registered",GATE_EPOCH_STALE:"Old supervisor lease rejected",REJ_STALE_OBSERVATION:"Planner used an old observation",REJ_STOPPING_DISTANCE:"Insufficient modeled stopping distance",REJ_RESTRICTED_ZONE:"Path enters a restricted zone",GATE_RECOVERED:"Recovery conditions met",UNV_SENSOR_STALE:"Fresh sensor data unavailable"};
    state.reasons.unshift({time:event.t_ns, text:words[reason] || reason.replaceAll("_", " ").toLowerCase()});
    state.reasons.length = Math.min(state.reasons.length,40);
  }
}
function seek(time) {
  cursor = Math.max(0, Math.min(time, events.at(-1)?.t_ns || 0));
  reset();
  for (const event of events) { if (event.t_ns > cursor) break; apply(event); }
  $("seek").value = cursor;
  $("time").textContent = ((cursor-(events[0]?.t_ns || 0))/1e9).toFixed(2)+" s";
  $("mode").textContent = state.mode;
  $("mode").dataset.mode=state.mode;
  $("epoch").textContent = state.epoch || "—";
  $("velocity").textContent = state.observation ? state.observation.v_meas.toFixed(2)+" m/s" : "—";
  $("decisions").textContent = state.decisions;
  $("rejected").textContent = state.rejected;
  const lease=state.lease, remaining=lease ? Math.max(0,lease.expires_ns-cursor):0;
  $("lease").value=lease ? remaining/Math.max(1,lease.expires_ns-lease.issued_ns):0;
  $("lease-detail").textContent=lease ? `${(remaining/1e6).toFixed(0)} ms · lease ${lease.lease_id}`:"No active lease data";
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
  const lease=state.lease;
  if(lease && state.mode==="MOVING" && [lease.x,lease.y,lease.theta,lease.d_req,lease.v_cmd,lease.w_cmd].every(Number.isFinite) && lease.d_req>=0 && lease.d_req<=10){
    const curvature=lease.v_cmd>0 ? lease.w_cmd/lease.v_cmd:0;
    ctx.strokeStyle="#dfb367";ctx.setLineDash([6,4]);ctx.lineWidth=2;ctx.beginPath();
    for(let i=0;i<=40;i++){const distance=lease.d_req*i/40,turn=distance*curvature;
      const x=lease.x+(Math.abs(curvature)<1e-10 ? distance*Math.cos(lease.theta):(Math.sin(lease.theta+turn)-Math.sin(lease.theta))/curvature);
      const y=lease.y+(Math.abs(curvature)<1e-10 ? distance*Math.sin(lease.theta):-(Math.cos(lease.theta+turn)-Math.cos(lease.theta))/curvature);
      i ? ctx.lineTo(...point(x,y)):ctx.moveTo(...point(x,y));}
    ctx.stroke();ctx.setLineDash([]);ctx.fillStyle="#dfb367";ctx.fillText(`Recorded d_req ${lease.d_req.toFixed(2)} m`,...point(lease.x,lease.y+0.8));
  }
  if(o){for(const obstacle of o.obstacles || []){ctx.fillStyle="#ecad78";ctx.beginPath();ctx.arc(...point(obstacle.x,obstacle.y),obstacle.r*scale,0,Math.PI*2);ctx.fill();}
    const [x,y]=point(o.x,o.y);ctx.strokeStyle="#8adccc";ctx.fillStyle="#47b7a64d";ctx.lineWidth=2;ctx.beginPath();ctx.arc(x,y,0.35*scale,0,Math.PI*2);ctx.fill();ctx.stroke();ctx.beginPath();ctx.moveTo(x,y);ctx.lineTo(x+Math.cos(o.theta)*0.65*scale,y-Math.sin(o.theta)*0.65*scale);ctx.stroke();}
  const goal=point(map.goal.x,map.goal.y);ctx.strokeStyle="#91a5b1";ctx.strokeRect(goal[0]-6,goal[1]-6,12,12);
}
$("play").onclick=()=>{if(metadata?.live){followLive=!followLive;$("play").textContent=followLive ? "Pause":"Follow live";if(followLive)seek(events.at(-1)?.t_ns || 0);return;}if(cursor>=Number($("seek").max))seek(0);playing=!playing;$("play").textContent=playing ? "Pause":"Play";};
$("seek").oninput=event=>{if(metadata?.live){followLive=false;$("play").textContent="Follow live";}seek(Number(event.target.value));};
function frame(now){const dt=previous ? now-previous:0;previous=now;if(playing){cursor+=dt*1e6*Number($("speed").value);if(now-lastDraw>33){seek(cursor);lastDraw=now;if(cursor>=Number($("seek").max)){playing=false;$("play").textContent="Play";}}}requestAnimationFrame(frame);}
async function start(){metadata=await (await fetch("/meta.json")).json();$("map-title").textContent=metadata.map.title;$("source").textContent=metadata.source;const socket=new WebSocket(`ws://${location.hostname}:${metadata.websocket_port}/events`);
  socket.onmessage=event=>{const packet=JSON.parse(event.data);if(packet.type==="error")$("connection").textContent=packet.message;if(packet.type==="drop"){dropped+=packet.count || 1;$("drops").textContent=`Dropped malformed records: ${dropped}`;}if(packet.type==="events"){events.push(...packet.events);if(metadata.live){events.sort((a,b)=>a.t_ns-b.t_ns);if(events.length>20000)events.splice(0,events.length-20000);$("seek").min=events[0]?.t_ns || 0;$("seek").max=events.at(-1)?.t_ns || 0;$("connection").textContent="Live · read-only";if(followLive)seek(Number($("seek").max));}}if(packet.type==="end"){events.sort((a,b)=>a.t_ns-b.t_ns);$("seek").max=events.at(-1)?.t_ns || 0;$("connection").textContent=metadata.live ? "Live feed ended":"Replay ready";if(!metadata.live)seek(events[0]?.t_ns || 0);}};
  if(metadata.live){$("play").textContent="Pause";$("speed").disabled=true;}
  socket.onerror=()=>{$("connection").textContent="Relay unavailable";};
  requestAnimationFrame(frame);
}
start().catch(error=>{$("connection").textContent=error.message;});
