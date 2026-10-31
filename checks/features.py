import json, urllib.request, urllib.error, subprocess, os, time, tempfile, sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
build = tempfile.TemporaryDirectory(prefix='rescueroute-check-')
build_dir = Path(build.name)
source = build_dir / 'server_check.cpp'
source.write_text((root/'main.cpp').read_text(encoding='utf-8').replace('htons(8081)', 'htons(8082)'), encoding='utf-8')
executable = build_dir / ('server_check.exe' if os.name == 'nt' else 'server_check')
command = ['g++', '-std=c++17', '-Wall', '-Wextra', '-pedantic', str(source), '-o', str(executable)]
if os.name == 'nt': command.append('-lws2_32')
subprocess.run(command, check=True)
server = subprocess.Popen([str(executable)], cwd=root, stdout=subprocess.DEVNULL)
results = []
def request(path='/api/state', body=None, expected=200):
    req = urllib.request.Request('http://127.0.0.1:8082'+path,
        data=None if body is None else json.dumps(body, ensure_ascii=False).encode(),
        headers={'Content-Type':'application/json'})
    try:
        response = urllib.request.urlopen(req, timeout=6)
    except urllib.error.HTTPError as error:
        response = error
    assert response.code == expected, (path, response.code, expected)
    return json.loads(response.read())
def check(name, test):
    try:
        test()
        results.append({'feature':name, 'result':'PASS'})
    except Exception as error:
        results.append({'feature':name, 'result':'FAIL', 'detail':str(error)})
def require(condition, detail='Unexpected result'):
    assert condition, detail
def add(name='Test caller', urgency=3, **kwargs):
    return request('/api/calls', dict(name=name,phone='9876543210',location=0,urgency=urgency,condition=['','Critical','Urgent','Stable'][urgency],**kwargs))
try:
    time.sleep(.5)
    s=request()
    initial=s
    check('Initial demo counts',lambda:require(s['counts']=={'available':6,'busy':2,'waiting':3}))
    # Independent shortest-path oracle: Floyd-Warshall, rather than the server Dijkstra.
    n=len(s['places']); distances=[[float('inf')]*n for _ in range(n)]
    edges={}
    for i in range(n): distances[i][i]=0
    for a,b,w in s['edges']:
        distances[a][b]=distances[b][a]=w;edges[a,b]=edges[b,a]=w
    for k in range(n):
        for i in range(n):
            for j in range(n): distances[i][j]=min(distances[i][j],distances[i][k]+distances[k][j])
    hospitals=[i for i,p in enumerate(s['places']) if p['type']=='hospital']
    def routes():
        calls={c['id']:c for c in s['waiting']+s['active']}
        for plan in s['routePlans']:
            call=calls[plan['callId']];unit=next(a for a in s['ambulances'] if a['id']==plan['ambulanceId'])
            for path,key,start,end in [(plan['path'],'distance',unit['at'],call['location']), (plan['hospitalPath'],'hospitalDistance',call['location'],plan['hospital'])]:
                require(path[0]==start and path[-1]==end)
                require(sum(edges[a,b] for a,b in zip(path,path[1:]))==plan[key]==distances[start][end])
            require(plan['hospitalDistance']==min(distances[call['location']][h] for h in hospitals))
    check('All demo routes and nearest hospitals',routes)
    check('Hospital suggestions for every incident location',lambda:require(all(x['distance']==distances[x['location']][x['hospital']]==min(distances[x['location']][h] for h in hospitals) for x in s['hospitalSuggestions'])))
    check('Hospital directory includes every hospital',lambda:require(sorted(h['place'] for r in s['hospitalDirectory']['children'] for h in r['children'])==hospitals))
    before=request();after=request('/api/dispatch',{})
    chosen=next(c for c in after['active'] if c['id'] not in [a['id'] for a in before['active']])
    check('Dispatch highest priority and closest available unit',lambda:require(chosen['name']==before['waiting'][0]['name'] and next(p for p in after['routePlans'] if p['callId']==chosen['id'])['distance']==min(distances[a['at']][chosen['location']] for a in before['ambulances'] if a['status']=='available')))
    undone=request('/api/undo-dispatch',{})
    check('Undo restores waiting calls and ambulance state',lambda:require(all(undone[k]==before[k] for k in ['waiting','active','ambulances','counts'])))
    request('/api/undo-dispatch',{},409)
    a=add('Stable test');b=add('Critical first',1);c=add('Critical second',1)
    check('Add calls, priority sorting and FIFO ties',lambda:require([x['name'] for x in c['waiting']][:3]==['Kavya Sharma','Critical first','Critical second'] and c['waiting'][-1]['name']=='Stable test'))
    check('Activity history newest first',lambda:require(c['activityHistory'][0].startswith('Critical second reported')))
    before=request();after=request('/api/dispatch',{});active=next(x for x in after['active'] if x['id'] not in [c['id'] for c in before['active']]);plan=next(p for p in after['routePlans'] if p['callId']==active['id'])
    done=request('/api/complete',{'id':active['id']})
    check('Complete frees ambulance at caller location',lambda:require(not any(x['id']==active['id'] for x in done['active']) and any(a['id']==plan['ambulanceId'] and a['status']=='available' and a['at']==active['location'] for a in done['ambulances']) and not done['dispatchUndoAvailable']))
    check('Unknown completion rejected',lambda:request('/api/complete',{'id':-99},404))
    def validation():
        base=dict(name='Test',phone='9876543210',location=0,urgency=1,condition='Critical')
        for key,val in [('phone','123'),('phone','5876543210'),('name',''),('location',-1),('location',999),('urgency',0),('urgency',4)]:
            request('/api/calls',dict(base,**{key:val}),400)
    check('Invalid mobile, empty name, location and urgency rejected',validation)
    def unicode():
        state=add('आरव शर्मा')
        require(any(c['name']=='आरव शर्मा' for c in state['waiting']))
    check('Unicode caller names',unicode)
    def escaped():
        state=add('Test "quoted" \\ name')
        require(any(c['name']=='Test "quoted" \\ name' for c in state['waiting']))
    check('Quoted and backslash caller names',escaped)
    def whitespace():
        request('/api/calls',dict(name='   ',phone='9876543210',location=0,urgency=1,condition='Critical'),400)
    check('Whitespace-only caller name rejected by API',whitespace)
    def exhaustion():
        for i in range(8): add('Load '+str(i))
        for i in range(request()['counts']['available']):request('/api/dispatch',{})
        state=request();require(state['counts']['available']==0)
        request('/api/dispatch',{},409)
        require(len([p for p in state['routePlans'] if not p['busy']])==0)
    check('Ambulance exhaustion handled',exhaustion)
finally:
    server.terminate();server.wait(timeout=5)
    print(json.dumps(results,indent=2,ensure_ascii=False))
    (root/'checks/results.json').write_text(json.dumps(results,indent=2,ensure_ascii=False),encoding='utf-8')

build.cleanup()
sys.exit(1 if any(result['result'] == 'FAIL' for result in results) else 0)
