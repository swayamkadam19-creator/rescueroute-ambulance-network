# Emergency Ambulance & City Network Optimization System

A second-year data structures project with a browser dashboard and a C++ backend. The dashboard shows a sample city network, patient calls, ambulance locations, priority order, and suggested routes to the closest hospital.

## C++ data structures and algorithms

- **Weighted graph and adjacency list:** locations are vertices; roads are two-way edges with distance in kilometres.
- **Dijkstra's algorithm:** a min-priority queue finds the shortest route from an ambulance to a caller and from a caller to a hospital.
- **Emergency priority queue:** Critical calls come before Urgent calls, then Stable calls. Calls with equal condition are handled in arrival order.
- **Vectors:** store map locations, connected roads, ambulances, and active and waiting calls.

The browser code in `app.js` draws the interface and sends actions to the C++ server. Route finding, dispatch choices, the priority queue, and the live demo state are managed by `main.cpp`.

## Build and run

From this project folder on macOS or Linux, compile and start the C++ server:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o ambulance_dashboard
./ambulance_dashboard
```

Then open [http://localhost:8081](http://localhost:8081). Keep the terminal running while using the dashboard. Press **Ctrl+C** in the terminal to stop it. Calls and ambulance status are sample in-memory data and return to their starting values when the server restarts.

## Try the demo

The dashboard starts with three waiting calls and two active calls so the priority queue and routes are visible immediately. Add another caller and choose a condition. Critical calls sort before Urgent and Stable calls. Select **Dispatch next** to send the closest available unit to the top-priority call. **Complete** returns that unit to service at the caller's location.

The Hospital Directory groups facilities by region as a tree-style directory and suggests the nearest hospital for the selected incident location. Road routes and distances still come from the weighted city graph. **Undo last dispatch** uses a stack to restore the most recent active dispatch to the waiting queue and return the ambulance to its starting location. The activity history lists recent events newest first.

The sample caller names and Indian-format 10-digit phone numbers are fictional demo placeholders. New calls require a 10-digit Indian mobile number beginning with 6, 7, 8, or 9. Ambulance call links open the phone dialer with the +91 country code. Replace the sample ambulance numbers in the `ambulances` setup in `main.cpp` before using them for real contacts.

## Scope

The locations, distances, calls, ambulance positions, and hospital choices are fictional. This project demonstrates data structures and routing; it is not connected to a real emergency service or live traffic feed.

## Windows

With MinGW g++ available, run these commands in PowerShell from this folder:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o ambulance_dashboard.exe -lws2_32
.\ambulance_dashboard.exe
```

Open http://localhost:8081 while the server is running. Keep your MinGW bin directory on PATH so the executable can load its runtime libraries.

## Feature checks

Install Python 3 and a C++17-compatible `g++` compiler, then run:

```sh
python checks/features.py
```

The checks compile a temporary server on port 8082, verify 15 feature groups, and stop that server afterward. They do not change a dashboard running on port 8081. On Windows, keep the MinGW bin directory on PATH for the compiler and runtime DLLs.

## Files

- `main.cpp`: graph, shortest paths, priority queue, dispatch, history, and HTTP server.
- `app.js`: dashboard rendering and API actions.
- `index.html`: responsive interface and styling.
- `checks/features.py`: independent route verification and API regression checks.
