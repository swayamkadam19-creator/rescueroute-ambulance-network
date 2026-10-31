// The browser only draws the interface. The C++ server owns the graph,
// priority queue, ambulance state, and route calculations.
let state = null;
const map = document.getElementById('map');
const svg = document.getElementById('roads');
const locationSelect = document.getElementById('location');
const urgencySelect = document.getElementById('urgency');

function escapeHtml(text) {
  const div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}

function phoneLabel(phone) {
  return phone;
}

function phoneLink(unit) {
  return `<a href="tel:+91${unit.phone}">${phoneLabel(unit.phone)}</a>`;
}

function conditionLabel(call) {
  return call.condition ? call.condition.split(' ·')[0] : ['', 'Critical', 'Urgent', 'Stable'][call.urgency];
}

function showToast(text) {
  const toast = document.getElementById('toast');
  toast.textContent = text;
  toast.classList.add('show');
  setTimeout(() => toast.classList.remove('show'), 2400);
}

function drawMap(plans) {
  svg.innerHTML = '';
  const element = (tag, attrs) => {
    const node = document.createElementNS('http://www.w3.org/2000/svg', tag);
    Object.entries(attrs).forEach(([key, value]) => node.setAttribute(key, value));
    return node;
  };

  state.edges.forEach(([a, b]) => {
    const p = state.places[a], q = state.places[b];
    svg.appendChild(element('line', { x1:p.x*8, y1:p.y*5.6, x2:q.x*8, y2:q.y*5.6, class:'road' }));
  });
  plans.forEach(plan => {
    for (let i=1; i<plan.path.length; i++) {
      const p=state.places[plan.path[i-1]], q=state.places[plan.path[i]];
      svg.appendChild(element('line', { x1:p.x*8, y1:p.y*5.6, x2:q.x*8, y2:q.y*5.6, class:'road-highlight', stroke:plan.color }));
    }
    for (let i=1; i<plan.hospitalPath.length; i++) {
      const p=state.places[plan.hospitalPath[i-1]], q=state.places[plan.hospitalPath[i]];
      svg.appendChild(element('line', { x1:p.x*8, y1:p.y*5.6, x2:q.x*8, y2:q.y*5.6, class:'hospital-highlight' }));
    }
  });
  state.edges.forEach(([a,b,distance],index) => {
    const p=state.places[a], q=state.places[b];
    const x1=p.x*8,y1=p.y*5.6,x2=q.x*8,y2=q.y*5.6,dx=x2-x1,dy=y2-y1;
    const length=Math.hypot(dx,dy)||1,side=index%2?1:-1;
    const mx=(x1+x2)/2+(dy/length)*10*side,my=(y1+y2)/2-(dx/length)*10*side;
    svg.appendChild(element('rect',{x:mx-16,y:my-8,width:32,height:16,rx:6,class:'road-label-bg'}));
    const label=element('text',{x:mx,y:my,class:'road-label'});label.textContent=`${distance}km`;svg.appendChild(label);
  });

  map.querySelectorAll('.place,.ambulance-pin,.call-dot').forEach(node=>node.remove());
  const incidents=new Set([...state.waiting,...state.active].map(call=>call.location));
  state.places.forEach((place,index)=>{
    const node=document.createElement('div');
    node.className=`place ${place.type}${incidents.has(index)?' incident':''}`;
    node.style.left=place.x+'%';node.style.top=place.y+'%';
    node.innerHTML=`<div class="marker">${place.icon}</div><span>${escapeHtml(place.name)}</span>`;
    map.appendChild(node);
  });
  const offsets=[[13,-10],[-13,-10],[13,11],[-13,11],[0,-18],[0,18]],groups=new Map();
  state.ambulances.forEach(unit=>{if(!groups.has(unit.at))groups.set(unit.at,[]);groups.get(unit.at).push(unit)});
  groups.forEach((units,placeIndex)=>units.forEach((unit,index)=>{
    const p=state.places[placeIndex],off=offsets[index%offsets.length],pin=document.createElement('div');
    pin.className=`ambulance-pin ${unit.status==='busy'?'busy':''}`;
    pin.style.left=`calc(${p.x}% + ${off[0]}px)`;pin.style.top=`calc(${p.y}% + ${off[1]}px)`;
    pin.title=`${unit.id} · ${unit.status} · ${p.name} · ${phoneLabel(unit.phone)}`;
    pin.innerHTML=`🚑<b>${unit.id}</b>`;map.appendChild(pin);
  }));
  [...state.waiting,...state.active].forEach(call=>{
    const p=state.places[call.location],dot=document.createElement('div');dot.className='call-dot';
    dot.style.left=`calc(${p.x}% + 16px)`;dot.style.top=`calc(${p.y}% - 15px)`;
    dot.textContent=call.id>=100?'●':`#${call.id}`;
    dot.title=`${call.name} · ${call.id>=100?'On call':'Waiting'} · ${p.name}`;map.appendChild(dot);
  });
}

function render() {
  const selectedLocation=locationSelect.value;
  document.getElementById('available').textContent=state.counts.available;
  document.getElementById('busy').textContent=state.counts.busy;
  document.getElementById('waiting').textContent=state.counts.waiting;
  document.getElementById('locationCount').textContent=state.places.length;
  locationSelect.innerHTML=state.places.map((p,i)=>`<option value="${i}">${escapeHtml(p.name)}</option>`).join('');
  if(selectedLocation!==''&&Number(selectedLocation)<state.places.length)locationSelect.value=selectedLocation;

  const plans=state.routePlans;
  const byCall=new Map(plans.filter(p=>!p.busy).map(p=>[p.callId,p]));
  const callById=new Map([...state.waiting,...state.active].map(call=>[call.id,call]));
  const queue=document.getElementById('queue');
  queue.innerHTML=state.waiting.length?state.waiting.map(call=>{
    const plan=byCall.get(call.id);
    const unit=plan&&state.ambulances.find(a=>a.id===plan.ambulanceId);
    const suggested=plan?` · Best unit: ${unit.id} at ${escapeHtml(state.places[unit.at].name)} (${plan.distance} km) · Nearest hospital: ${escapeHtml(state.places[plan.hospital].name)} (${plan.hospitalDistance} km) · ${phoneLink(unit)}`:' · No unit available yet';
    return `<div class="call"><div class="call-num">#${call.id}</div><div><strong>${escapeHtml(call.name)} · ${conditionLabel(call)}</strong><small>Caller: ${escapeHtml(state.places[call.location].name)} · ${phoneLink(call)}${suggested}</small></div><span class="urgency ${['','critical','urgent','normal'][call.urgency]}">${conditionLabel(call)}</span></div>`;
  }).join(''):'<div class="empty">No calls waiting. The city is all clear.</div>';

  const free=state.ambulances.filter(unit=>unit.status==='available');
  const available=document.getElementById('availableList');
  available.innerHTML=free.length?free.map(unit=>`<div class="call"><div class="call-num">🚑</div><div><strong>${unit.id} · Available</strong><small>Location: ${escapeHtml(state.places[unit.at].name)} · Call ${phoneLink(unit)}</small></div></div>`).join(''):'<div class="empty">No ambulances currently available.</div>';

  const busy=document.getElementById('busyList'),activePlans=new Map(plans.filter(p=>p.busy).map(p=>[p.callId,p]));
  busy.innerHTML=state.active.length?state.active.map(call=>{
    const plan=activePlans.get(call.id),unit=state.ambulances.find(a=>a.id===plan.ambulanceId);
    return `<div class="call busy-call"><div class="call-num">🚑</div><div><strong>${unit.id} · ${escapeHtml(call.name)} · ${conditionLabel(call)}</strong><small>At: ${escapeHtml(state.places[unit.at].name)} → Caller: ${escapeHtml(state.places[call.location].name)} · Caller ${phoneLink(call)} · Hospital: ${escapeHtml(state.places[plan.hospital].name)} (${plan.hospitalDistance} km) · Ambulance ${phoneLink(unit)}</small></div><button class="complete-btn" data-id="${call.id}">Complete</button></div>`;
  }).join(''):'<div class="empty">No ambulances currently busy.</div>';
  busy.querySelectorAll('.complete-btn').forEach(button=>button.addEventListener('click',()=>completeCall(Number(button.dataset.id))));

  const list=document.getElementById('routeList');
  list.innerHTML=plans.map(plan=>{
    const call=callById.get(plan.callId),unit=state.ambulances.find(a=>a.id===plan.ambulanceId);
    return `<div class="route-plan ${plan.busy?'busy-plan':'wait-plan'}"><b style="color:${plan.color}">${plan.busy?'IN PROGRESS':'WAITING'} · ${escapeHtml(call.name)} · ${conditionLabel(call)}</b><span>${unit.id} at ${escapeHtml(state.places[unit.at].name)} → Caller: ${escapeHtml(state.places[call.location].name)} · ${phoneLink(call)}<br>Ambulance route: ${plan.path.map(i=>state.places[i].name).join(' → ')} · ${plan.distance} km<br>To hospital: ${plan.hospitalPath.map(i=>state.places[i].name).join(' → ')} · ${plan.hospitalDistance} km (${escapeHtml(state.places[plan.hospital].name)})<br>Ambulance ${phoneLink(unit)}</span></div>`;
  }).join('')||'<div class="empty">No current route plans.</div>';
  document.getElementById('hospitalDirectory').innerHTML=state.hospitalDirectory.children.map((region,index)=>`<details ${index===0?'open':''}><summary>${escapeHtml(region.name)}</summary><ul>${region.children.map(hospital=>`<li>${escapeHtml(hospital.name)}</li>`).join('')}</ul></details>`).join('');
  document.getElementById('activityHistory').innerHTML=state.activityHistory.map(event=>`<li>${escapeHtml(event)}</li>`).join('');
  document.getElementById('undoDispatch').disabled=!state.dispatchUndoAvailable;
  renderHospitalSuggestion();
  drawMap(plans);
}

function renderHospitalSuggestion(){
  if(!state)return;
  const location=Number(locationSelect.value),suggestion=state.hospitalSuggestions.find(item=>item.location===location);
  const panel=document.getElementById('hospitalSuggestion');
  panel.textContent=suggestion?`Nearest hospital from ${state.places[location].name}: ${state.places[suggestion.hospital].name} · ${suggestion.distance} km by the road network. The displayed route is calculated on the city graph.`:'Select a non-hospital incident location to see the nearest hospital.';
}

async function request(path,body) {
  const response=await fetch(path,{method:body?'POST':'GET',headers:body?{'Content-Type':'application/json'}:{},body:body?JSON.stringify(body):undefined});
  const result=await response.json();
  if(!response.ok)throw new Error(result.error||'The C++ service could not complete that action.');
  state=result;render();return result;
}

async function completeCall(id) {
  try{await request('/api/complete',{id});showToast('Call completed. The ambulance is available at the destination.');}
  catch(error){showToast(error.message);}
}

document.getElementById('callForm').addEventListener('submit',async event=>{
  event.preventDefault();
  const name=document.getElementById('caller').value.trim(),phone=document.getElementById('callerPhone').value.trim(),location=Number(locationSelect.value);
  const urgency=Number(urgencySelect.value),condition=urgencySelect.selectedOptions[0].text;
  try{await request('/api/calls',{name,phone,location,urgency,condition});showToast(`${name}'s ${condition.split(' ·')[0].toLowerCase()} call added to the priority queue.`);event.target.reset();renderHospitalSuggestion();}
  catch(error){showToast(error.message);}
});

document.getElementById('dispatch').addEventListener('click',async()=>{
  try{await request('/api/dispatch',{});showToast('Highest-priority call dispatched to the closest available ambulance.');}
  catch(error){showToast(error.message);}
});

document.getElementById('undoDispatch').addEventListener('click',async()=>{
  try{await request('/api/undo-dispatch',{});showToast('Last dispatch undone. The call returned to the priority queue.');}
  catch(error){showToast(error.message);}
});

locationSelect.addEventListener('change',renderHospitalSuggestion);

request('/api/state').catch(error=>{showToast('Start the C++ dashboard application to connect.');console.error(error);});
