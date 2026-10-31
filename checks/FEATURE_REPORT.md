# Feature verification — 31 October 2026

Tested an isolated copy on port 8082; the user's dashboard on port 8081 was not modified by test actions.

## Passed

- Initial demo counts: 17 locations, 6 available ambulances, 2 active calls, 3 waiting calls.
- Demo response paths and hospital paths matched an independent Floyd-Warshall shortest-path calculation.
- Nearest-hospital suggestions and regional hospital directory.
- Emergency creation, priority ordering, FIFO ordering of equal priorities.
- Closest available ambulance selection for dispatch.
- Undo restored the original calls, ambulance positions, and counts.
- Completion released the ambulance at the caller location and removed the completed dispatch from undo history.
- Activity history in newest-first order.
- Rejection of invalid phone numbers, empty names, out-of-range locations and urgency, and unknown completion IDs.
- Unicode names, quotes, and backslashes.
- No available ambulance: dispatch returned a conflict without crashing.
- Browser form submission, dispatch, undo, completion, invalid-phone form validation, expandable hospital directory, and correct +91 dialer link target.
- No browser warnings/errors during the tested normal workflows.

## Confirmed issues

1. **Stale hospital suggestion after adding a call:** choose Harbor and submit a call. The incident selector resets to North Station, while the suggestion still describes Harbor. `app.js` calls `event.target.reset()` after rendering the suggestion and does not refresh it after resetting. A subsequent action that renders the page corrects the text.
2. **Whitespace-only name accepted by the API:** POST `/api/calls` with `name` containing only spaces returns 200. The browser trims names, but `main.cpp` checks only `name.empty()`, so direct API requests can add blank-looking callers.

The automated API checks passed 14 of 15 groups; the whitespace-name rejection group failed as expected. The stale-suggestion issue was reproduced separately in the browser.

## Limits

Verified dialer link URLs without placing calls. Did not exhaustively test every input, browser, screen size, or long-running workload. This is a fictional demo, not a live ambulance service. No application behavior was changed during this review.

## Fix verification

Both confirmed issues have been fixed. All 15 automated groups now pass. A browser test selected Harbor, added a call, and confirmed both the reset selector and hospital suggestion describe North Station. The main dashboard was rebuilt and restarted on port 8081.
