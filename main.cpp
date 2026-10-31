#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
constexpr Socket invalidSocket = INVALID_SOCKET;
static void close(Socket socket) { closesocket(socket); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket = int;
constexpr Socket invalidSocket = -1;
#endif

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>
#include <stack>
#include <string>
#include <vector>

using namespace std;

// A location is a vertex in the city's road graph.
struct Location {
    string name;
    int x;
    int y;
    string type;
    string icon;
};

// A road is an undirected, weighted edge. The weight is distance in km.
struct Road {
    int destination;
    int kilometres;
};

struct Route {
    int distance;
    vector<int> places;
};

struct EmergencyCall {
    int id;
    string name;
    string phone;
    int location;
    int urgency; // 1 = critical, 2 = urgent, 3 = stable.
    string condition;
};

struct Ambulance {
    string id;
    string phone;
    int location;
    bool available;
};

// priority_queue puts the most urgent call (and then oldest call) on top.
struct LowerPriorityFirst {
    bool operator()(const EmergencyCall& a, const EmergencyCall& b) const {
        if (a.urgency != b.urgency) return a.urgency > b.urgency;
        return a.id > b.id;
    }
};

struct RoutePlan {
    EmergencyCall call;
    Ambulance ambulance;
    bool busy;
    Route toCaller;
    int hospital;
    Route toHospital;
    string colour;
};

struct DispatchRecord {
    EmergencyCall originalCall;
    int activeCallId;
    string ambulanceId;
    int ambulanceStart;
};

struct HospitalTreeNode {
    string name;
    int placeIndex;
    vector<HospitalTreeNode> children;
};

class CityNetwork {
public:
    vector<Location> places;
    vector<vector<Road>> roads;

    int addLocation(const string& name, int x, int y, const string& type, const string& icon) {
        places.push_back({name, x, y, type, icon});
        roads.emplace_back();
        return static_cast<int>(places.size()) - 1;
    }

    void addRoad(int a, int b, int km) {
        roads[a].push_back({b, km});
        roads[b].push_back({a, km});
    }

    Route shortestRoute(int start, int finish) const {
        const int infinity = numeric_limits<int>::max();
        vector<int> distance(places.size(), infinity), previous(places.size(), -1);
        using QueueItem = pair<int, int>;
        priority_queue<QueueItem, vector<QueueItem>, greater<QueueItem>> next;
        distance[start] = 0;
        next.push({0, start});

        while (!next.empty()) {
            const int currentDistance = next.top().first;
            const int current = next.top().second;
            next.pop();
            if (currentDistance != distance[current]) continue;

            for (const Road& road : roads[current]) {
                const int candidate = currentDistance + road.kilometres;
                if (candidate < distance[road.destination]) {
                    distance[road.destination] = candidate;
                    previous[road.destination] = current;
                    next.push({candidate, road.destination});
                }
            }
        }

        vector<int> path;
        for (int place = finish; place != -1; place = previous[place]) {
            path.push_back(place);
            if (place == start) break;
        }
        reverse(path.begin(), path.end());
        if (path.empty() || path.front() != start) return {infinity, {}};
        return {distance[finish], path};
    }

    pair<int, Route> closestHospital(int from) const {
        pair<int, Route> best{-1, {numeric_limits<int>::max(), {}}};
        for (int i = 0; i < static_cast<int>(places.size()); ++i) {
            if (places[i].type != "hospital") continue;
            Route route = shortestRoute(from, i);
            if (route.distance < best.second.distance) best = {i, route};
        }
        return best;
    }
};

class AmbulanceService {
public:
    CityNetwork city;
    vector<Ambulance> ambulances;
    vector<EmergencyCall> waiting;
    vector<EmergencyCall> active;
    HospitalTreeNode hospitalDirectory{"City Hospitals", -1, {}};
    vector<string> routeColours{"#176b4a", "#427cc2", "#a46ab5", "#de7540", "#258a8d", "#bc5870"};
    vector<DispatchRecord> dispatchHistory; // Last-in, first-out dispatch stack.
    stack<string> activityHistory; // LIFO activity log: the newest event is on top.
    int nextCallId = 4;

    AmbulanceService() {
        const int north = city.addLocation("North Station", 12, 13, "station", "✚");
        const int pine = city.addLocation("Pinecrest", 31, 13, "city", "⌂");
        const int university = city.addLocation("University", 52, 13, "city", "▤");
        const int hillHospital = city.addLocation("Hilltop Hospital", 74, 13, "hospital", "✚");
        const int west = city.addLocation("West End", 15, 34, "city", "⌂");
        const int oldTown = city.addLocation("Old Town", 36, 34, "city", "⌂");
        const int market = city.addLocation("Market Square", 57, 34, "city", "▦");
        const int airport = city.addLocation("Airport", 84, 34, "city", "✈");
        const int central = city.addLocation("Central Station", 12, 57, "station", "✚");
        const int river = city.addLocation("Riverside", 34, 57, "city", "≈");
        const int centralHospital = city.addLocation("Central Hospital", 58, 57, "hospital", "✚");
        const int park = city.addLocation("East Park", 81, 57, "city", "♧");
        const int south = city.addLocation("South Station", 16, 82, "station", "✚");
        const int lake = city.addLocation("Lakeview", 39, 82, "city", "≈");
        const int industry = city.addLocation("Industrial District", 63, 82, "city", "▦");
        const int harbour = city.addLocation("Harbor", 85, 82, "city", "⚓");
        const int harbourClinic = city.addLocation("Harbor Clinic", 94, 74, "hospital", "✚");

        const vector<vector<int>> edges = {
            {north,pine,5},{north,west,6},{pine,university,4},{pine,oldTown,5},
            {university,hillHospital,5},{university,market,4},{hillHospital,airport,6},
            {west,oldTown,4},{west,central,6},{oldTown,market,4},{oldTown,river,6},
            {market,airport,5},{market,centralHospital,5},{airport,park,6},
            {central,river,5},{central,south,5},{river,centralHospital,4},{river,lake,5},
            {centralHospital,park,4},{centralHospital,industry,6},{park,harbour,5},
            {south,lake,5},{lake,industry,5},{industry,harbour,4},{oldTown,centralHospital,7},
            {market,river,6},{river,industry,7},{centralHospital,lake,6},
            {park,harbourClinic,5},{harbour,harbourClinic,3},{airport,harbourClinic,8}
        };
        for (const auto& edge : edges) city.addRoad(edge[0], edge[1], edge[2]);

        hospitalDirectory.children = {
            {"North Region", -1, {{"Hilltop Hospital", hillHospital, {}}}},
            {"Central Region", -1, {{"Central Hospital", centralHospital, {}}}},
            {"Harbor Region", -1, {{"Harbor Clinic", harbourClinic, {}}}}
        };

        ambulances = {
            {"A-01", "9876501001", north, true}, {"A-02", "9876501002", university, true},
            {"A-03", "9876501003", airport, true}, {"A-04", "9876501004", central, true},
            {"A-05", "9876501005", centralHospital, true}, {"A-06", "9876501006", south, true},
            {"A-07", "9876501007", oldTown, false}, {"A-08", "9876501008", industry, false}
        };
        waiting = {
            {1, "Kavya Sharma", "9811100001", harbour, 1, "Critical · life-threatening"},
            {2, "Arjun Mehta", "9822200002", park, 2, "Urgent · needs prompt help"},
            {3, "Priya Nair", "9833300003", lake, 3, "Stable · non-urgent"}
        };
        active = {
            {101, "Rohan Verma", "9844400004", river, 1, "Critical · life-threatening"},
            {102, "Ananya Patel", "9855500005", harbour, 2, "Urgent · needs prompt help"}
        };
        activityHistory.push("Demo loaded with 3 calls waiting.");
        activityHistory.push("Demo started with 2 ambulances on active calls.");
    }

    vector<RoutePlan> makeRoutePlans() const {
        vector<Ambulance> free;
        for (const Ambulance& ambulance : ambulances) if (ambulance.available) free.push_back(ambulance);

        priority_queue<EmergencyCall, vector<EmergencyCall>, LowerPriorityFirst> queue;
        for (const auto& call : waiting) queue.push(call);

        vector<RoutePlan> plans;
        int colour = 0;
        while (!queue.empty()) {
            EmergencyCall call = queue.top();
            queue.pop();
            int bestIndex = -1;
            Route bestRoute{numeric_limits<int>::max(), {}};
            for (int i = 0; i < static_cast<int>(free.size()); ++i) {
                Route route = city.shortestRoute(free[i].location, call.location);
                if (route.distance < bestRoute.distance) { bestIndex = i; bestRoute = route; }
            }
            if (bestIndex >= 0) {
                Ambulance chosen = free[bestIndex];
                free.erase(free.begin() + bestIndex); // Keep a suggested unit for only one waiting call.
                auto hospital = city.closestHospital(call.location);
                plans.push_back({call, chosen, false, bestRoute, hospital.first, hospital.second,
                                 routeColours[colour++ % routeColours.size()]});
            }
        }

        for (size_t i=0;i<active.size();++i) {
            const EmergencyCall& call=active[i];
            auto unit=find_if(ambulances.begin(),ambulances.end(),[&](const Ambulance& a){return a.id==activeAmbulanceIds[i];});
            if (unit == ambulances.end()) continue;
            auto hospital = city.closestHospital(call.location);
            plans.push_back({call, *unit, true, city.shortestRoute(unit->location, call.location),
                             hospital.first, hospital.second, "#d29331"});
        }
        return plans;
    }

    string stateJson() const {
        ostringstream out;
        out << "{\"places\":[";
        for (size_t i=0;i<city.places.size();++i) { if(i)out<<',';const auto&p=city.places[i];out<<"{\"name\":"<<quote(p.name)<<",\"x\":"<<p.x<<",\"y\":"<<p.y<<",\"type\":"<<quote(p.type)<<",\"icon\":"<<quote(p.icon)<<'}'; }
        out << "],\"edges\":[";
        bool first=true;for(int from=0;from<static_cast<int>(city.roads.size());++from)for(const auto&r:city.roads[from])if(from<r.destination){if(!first)out<<',';first=false;out<<'['<<from<<','<<r.destination<<','<<r.kilometres<<']';}
        out << "],\"ambulances\":[";
        for(size_t i=0;i<ambulances.size();++i){if(i)out<<',';const auto&a=ambulances[i];out<<"{\"id\":"<<quote(a.id)<<",\"phone\":"<<quote(a.phone)<<",\"at\":"<<a.location<<",\"status\":"<<quote(a.available?"available":"busy")<<'}';}
        priority_queue<EmergencyCall, vector<EmergencyCall>, LowerPriorityFirst> callQueue;for(const auto& call:waiting)callQueue.push(call);vector<EmergencyCall> orderedWaiting;while(!callQueue.empty()){orderedWaiting.push_back(callQueue.top());callQueue.pop();}
        out << "],\"waiting\":[";writeCalls(out,orderedWaiting);out<<"],\"active\":[";writeCalls(out,active);out<<"],\"routePlans\":[";
        auto plans=makeRoutePlans();
        for(size_t i=0;i<plans.size();++i){if(i)out<<',';const auto&p=plans[i];out<<"{\"callId\":"<<p.call.id<<",\"busy\":"<<(p.busy?"true":"false")<<",\"ambulanceId\":"<<quote(p.ambulance.id)<<",\"color\":"<<quote(p.colour)<<",\"distance\":"<<p.toCaller.distance<<",\"path\":";writePath(out,p.toCaller.places);out<<",\"hospital\":"<<p.hospital<<",\"hospitalDistance\":"<<p.toHospital.distance<<",\"hospitalPath\":";writePath(out,p.toHospital.places);out<<'}';}
        int available=0;for(const auto&a:ambulances)if(a.available)++available;
        out << "],\"hospitalDirectory\":";writeHospitalNode(out,hospitalDirectory);
        out << ",\"hospitalSuggestions\":[";
        bool firstSuggestion=true;
        for(size_t place=0;place<city.places.size();++place){if(city.places[place].type=="hospital")continue;auto nearest=city.closestHospital(static_cast<int>(place));if(nearest.first<0)continue;if(!firstSuggestion)out<<',';firstSuggestion=false;out<<"{\"location\":"<<place<<",\"hospital\":"<<nearest.first<<",\"distance\":"<<nearest.second.distance<<"}";}
        out << "],\"activityHistory\":[";stack<string> newestEvents=activityHistory;bool firstEvent=true;while(!newestEvents.empty()){if(!firstEvent)out<<',';firstEvent=false;out<<quote(newestEvents.top());newestEvents.pop();}
        out << "],\"dispatchUndoAvailable\":"<<(!dispatchHistory.empty()?"true":"false")<<",\"counts\":{\"available\":"<<available<<",\"busy\":"<<ambulances.size()-available<<",\"waiting\":"<<waiting.size()<<"}}";
        return out.str();
    }

    void addCall(const string& name, const string& phone, int location, int urgency, const string& condition) {
        waiting.push_back({nextCallId++, name, phone, location, urgency, condition});
        recordActivity(name + " reported an emergency at " + city.places[location].name + ".");
    }

    bool dispatchNext() {
        priority_queue<EmergencyCall, vector<EmergencyCall>, LowerPriorityFirst> queue;
        for(const auto& call:waiting)queue.push(call);
        if(queue.empty())return false;
        auto unit=ambulances.end();Route best{numeric_limits<int>::max(),{}};
        for(auto it=ambulances.begin();it!=ambulances.end();++it)if(it->available){Route r=city.shortestRoute(it->location,queue.top().location);if(r.distance<best.distance){best=r;unit=it;}}
        if(unit==ambulances.end())return false;
        EmergencyCall call=queue.top();
        const EmergencyCall originalCall=call;
        const int ambulanceStart=unit->location;
        waiting.erase(find_if(waiting.begin(),waiting.end(),[&](const EmergencyCall& c){return c.id==call.id;}));
        unit->available=false;
        call.id=1000+call.id;
        dispatchHistory.push_back({originalCall,call.id,unit->id,ambulanceStart});
        active.push_back(call);
        // Active calls retain their own ambulance ID in the id-to-unit lookup below.
        activeAmbulanceIds.push_back(unit->id);
        recordActivity("Ambulance " + unit->id + " dispatched to " + city.places[call.location].name + " for " + call.name + ".");
        return true;
    }

    bool undoLastDispatch() {
        if(dispatchHistory.empty())return false;
        DispatchRecord record=dispatchHistory.back();dispatchHistory.pop_back();
        auto activeIt=find_if(active.begin(),active.end(),[&](const EmergencyCall& call){return call.id==record.activeCallId;});
        if(activeIt==active.end())return false; // A completed dispatch cannot be undone.
        const size_t index=static_cast<size_t>(activeIt-active.begin());
        auto ambulance=find_if(ambulances.begin(),ambulances.end(),[&](const Ambulance& unit){return unit.id==record.ambulanceId;});
        if(ambulance!=ambulances.end()){ambulance->available=true;ambulance->location=record.ambulanceStart;}
        active.erase(activeIt);activeAmbulanceIds.erase(activeAmbulanceIds.begin()+index);
        waiting.push_back(record.originalCall);
        recordActivity("Undo: " + record.ambulanceId + " returned to " + record.originalCall.name + "'s waiting call.");
        return true;
    }

    bool completeCall(int callId) {
        auto it=find_if(active.begin(),active.end(),[&](const EmergencyCall& c){return c.id==callId;});
        if(it==active.end())return false;
        const int destination=it->location;
        auto index=static_cast<size_t>(it-active.begin());
        const int completedId=it->id;
        auto unit=find_if(ambulances.begin(),ambulances.end(),[&](const Ambulance&a){return a.id==activeAmbulanceIds[index];});
        if(unit!=ambulances.end()){unit->available=true;unit->location=destination;}
        recordActivity("Call completed for " + it->name + ".");
        dispatchHistory.erase(remove_if(dispatchHistory.begin(),dispatchHistory.end(),[&](const DispatchRecord& record){return record.activeCallId==completedId;}),dispatchHistory.end());
        active.erase(it);activeAmbulanceIds.erase(activeAmbulanceIds.begin()+index);return true;
    }

private:
    vector<string> activeAmbulanceIds{"A-07","A-08"};

    void recordActivity(const string& message){activityHistory.push(message);}

    static string quote(const string& value) {
        string result="\"";for(char c:value){if(c=='\\'||c=='\"')result+='\\';if(c=='\n')result+="\\n";else result+=c;}return result+'\"';
    }
    static void writePath(ostringstream& out,const vector<int>& path){out<<'[';for(size_t i=0;i<path.size();++i){if(i)out<<',';out<<path[i];}out<<']';}
    static void writeHospitalNode(ostringstream& out,const HospitalTreeNode& node){out<<"{\"name\":"<<quote(node.name)<<",\"place\":"<<node.placeIndex<<",\"children\":[";for(size_t i=0;i<node.children.size();++i){if(i)out<<',';writeHospitalNode(out,node.children[i]);}out<<"]}";}
    static void writeCalls(ostringstream& out,const vector<EmergencyCall>& calls){for(size_t i=0;i<calls.size();++i){if(i)out<<',';const auto&c=calls[i];out<<"{\"id\":"<<c.id<<",\"name\":"<<quote(c.name)<<",\"phone\":"<<quote(c.phone)<<",\"location\":"<<c.location<<",\"urgency\":"<<c.urgency<<",\"condition\":"<<quote(c.condition)<<'}';}}
};

static string jsonString(const string& body,const string& key){string needle="\""+key+"\"";auto at=body.find(needle);if(at==string::npos)return {};at=body.find(':',at+needle.size());if(at==string::npos)return {};at=body.find('\"',at+1);if(at==string::npos)return {};string value;for(++at;at<body.size();++at){if(body[at]=='\\'&&at+1<body.size()){value+=body[++at];continue;}if(body[at]=='\"')break;value+=body[at];}return value;}
static bool isIndianMobile(const string& phone){return phone.size()==10&&phone[0]>='6'&&phone[0]<='9'&&all_of(phone.begin(),phone.end(),[](unsigned char c){return isdigit(c)!=0;});}
static int jsonInt(const string& body,const string& key,int fallback=0){string needle="\""+key+"\"";auto at=body.find(needle);if(at==string::npos)return fallback;at=body.find(':',at+needle.size());if(at==string::npos)return fallback;return atoi(body.c_str()+at+1);}

static string readFile(const string& path){ifstream file(path);if(!file)return {};ostringstream data;data<<file.rdbuf();return data.str();}
static void respond(Socket client,int status,const string& type,const string& body){string phrase=status==200?"OK":status==400?"Bad Request":status==404?"Not Found":"Conflict";ostringstream header;header<<"HTTP/1.1 "<<status<<' '<<phrase<<"\r\nContent-Type: "<<type<<"\r\nContent-Length: "<<body.size()<<"\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n";string response=header.str()+body;send(client,response.data(),response.size(),0);}

int main(){
#ifdef _WIN32
    WSADATA winsockData;
    if (WSAStartup(MAKEWORD(2, 2), &winsockData) != 0) {
        std::cerr << "Could not initialize Windows sockets.\n";
        return 1;
    }
#endif
    AmbulanceService service;
    const Socket server=socket(AF_INET,SOCK_STREAM,0);if(server==invalidSocket){cerr<<"Could not create the local web server socket.\n";return 1;}
    int reuse=1;setsockopt(server,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&reuse),sizeof(reuse));
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons(8081);
    if(::bind(server,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0){cerr<<"Port 8081 is busy. Close the other server or change the port.\n";close(server);return 1;}
    if(listen(server,12)<0){cerr<<"Could not listen on port 8081.\n";close(server);return 1;}
    cout<<"C++ ambulance dashboard running at http://localhost:8081\nPress Ctrl+C to stop.\n"<<flush;

    while(true){
        Socket client=accept(server,nullptr,nullptr);if(client==invalidSocket)continue;
        // Browsers may open speculative connections without sending a request.
        // Do not let one idle connection block this single-threaded server.
#ifdef _WIN32
        DWORD receiveTimeout = 1000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&receiveTimeout), sizeof(receiveTimeout));
#else
        timeval receiveTimeout{1, 0};
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &receiveTimeout, sizeof(receiveTimeout));
#endif

        string request;char buffer[4096];int received;
        while((received=recv(client,buffer,sizeof(buffer),0))>0){request.append(buffer,static_cast<size_t>(received));auto headerEnd=request.find("\r\n\r\n");if(headerEnd!=string::npos){size_t contentLength=0;auto marker=request.find("Content-Length:");if(marker!=string::npos)contentLength=static_cast<size_t>(atoi(request.c_str()+marker+15));if(request.size()>=headerEnd+4+contentLength)break;}}
        auto lineEnd=request.find("\r\n"),headerEnd=request.find("\r\n\r\n");if(lineEnd==string::npos||headerEnd==string::npos){respond(client,400,"text/plain","Bad request");close(client);continue;}
        istringstream firstLine(request.substr(0,lineEnd));string method,path,version;firstLine>>method>>path>>version;string body=request.substr(headerEnd+4);
        if(method=="GET"&&path=="/"){string html=readFile("index.html");if(html.empty())respond(client,404,"text/plain","index.html not found. Run the program from the project folder.");else respond(client,200,"text/html; charset=utf-8",html);}
        else if(method=="GET"&&path=="/app.js"){string script=readFile("app.js");if(script.empty())respond(client,404,"text/plain","app.js not found. Run the program from the project folder.");else respond(client,200,"text/javascript; charset=utf-8",script);}
        else if(method=="GET"&&path=="/api/state")respond(client,200,"application/json; charset=utf-8",service.stateJson());
        else if(method=="POST"&&path=="/api/calls"){string name=jsonString(body,"name"),phone=jsonString(body,"phone"),condition=jsonString(body,"condition");int location=jsonInt(body,"location",-1),urgency=jsonInt(body,"urgency",3);if(all_of(name.begin(),name.end(),[](unsigned char c){return isspace(c)!=0;})||!isIndianMobile(phone)||location<0||location>=static_cast<int>(service.city.places.size())||urgency<1||urgency>3)respond(client,400,"application/json","{\"error\":\"Enter a caller name, a valid 10-digit Indian mobile number, location, and condition.\"}");else{service.addCall(name,phone,location,urgency,condition);respond(client,200,"application/json",service.stateJson());}}
        else if(method=="POST"&&path=="/api/dispatch"){if(!service.dispatchNext())respond(client,409,"application/json","{\"error\":\"No call or available ambulance to dispatch.\"}");else respond(client,200,"application/json",service.stateJson());}
        else if(method=="POST"&&path=="/api/undo-dispatch"){if(!service.undoLastDispatch())respond(client,409,"application/json","{\"error\":\"There is no active dispatch available to undo.\"}");else respond(client,200,"application/json",service.stateJson());}
        else if(method=="POST"&&path=="/api/complete"){if(!service.completeCall(jsonInt(body,"id")))respond(client,404,"application/json","{\"error\":\"That active call was not found.\"}");else respond(client,200,"application/json",service.stateJson());}
        else respond(client,404,"text/plain","Not found");
        close(client);
    }
    close(server);
    return 0;
}
