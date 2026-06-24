#pragma once

#ifndef CV_HTTP_SERVER_H
#define CV_HTTP_SERVER_H

// Minimal GET-only HTTP/1.0 dev server -- the live observability surface (#387).
// Gated by the BUG option Autolog__HttpServer (Logging tab, off by default);
// binds 127.0.0.1:7227 only, so it is never reachable from off-machine.
//
// The surface has exactly two data buckets, split on the verification axis (full
// route map + rationale: docs/specs/http-endpoints.md):
//   GET /              -> "hello world" (the smoke test)
//   GET /events        -> Server-Sent Events stream (#407/#419): "hello" with the
//                         current turn + gameId on connect; turnStart/turnEnd and
//                         playerTurnStart/playerTurnEnd phase signals; "log" frames
//                         carrying RAW gated log lines at Autolog__LogLevelStream and
//                         below; ": keepalive" every ~15s. text/event-stream, never
//                         ends; <=8 concurrent streams (503 beyond).
//   GET /state/*       -> the RAW, uncalculated game state (techs known, buildings
//                         present, plots + contents/state, specialists, ...). NO yields,
//                         NO computed verdicts -- the INPUT to a calculation.
//   GET /computed/*    -> the engine's OWN computed answers (yield rates + per-source
//                         decomposition, gate verdicts, availability oracles, counts,
//                         victory state) -- the verification ground-truth the external
//                         dry-calc checks itself against.
// Anything but GET gets 405 (Allow: GET); unknown paths get 404. /state and /computed
// list their slices when fetched bare. Every response carries X-S2S-Turn.
//
// City ids are NOT unique across empires -- every city object carries {owner,id,name,
// x,y}; single-city fetches key on ?player=N&city=M (with ?name= as a convenience
// lookup). See docs/specs/http-endpoints.md ("City identity").
//
// gameId is CvGame::getGameId(): the persistent playtest identity stamped at game
// creation (digits-only yyMMddHHmm for new games; older saves carry "DD-MM-YYYY HH:MM:SS").
//
// Runs on its own Win32 thread. HARD CONSTRAINT: that thread NEVER touches live game
// objects. /state and /computed read live state, so they are serviced ON THE GAME
// THREAD via a single-slot mailbox (publishIfDue drains it); the server thread only
// renders the answer the game thread produced and reads a tiny published {turn,gameId}
// header for response metadata.
namespace CvHttpServer
{
	// Start or stop the server thread to match the BUG option. Idempotent.
	// Call from the game thread only (wired into cvInternalGlobals::refreshOptionsBUG,
	// so toggling the option in the BUG screen takes effect on closing it).
	void setEnabled(bool bEnable);

	// Game-thread publish hook (wired into CvGame::update, i.e. once per frame):
	// snapshots queryable state into the server's buffer. No-op when the server is
	// off; internally throttled to one snapshot every few seconds when it is on.
	void publishIfDue();

	// Game-thread event publish (#407): enqueues one SSE frame ("event: <szEvent>",
	// "data: <szJsonData>") for the /events stream. Pre-rendered on this side so the
	// server thread never touches game objects. publishEvent itself no-ops when the
	// server is off, but C++ evaluates call arguments regardless -- so guard the
	// payload FORMATTING with isEnabled() at the call site, or the off-state cost is
	// a wasted format instead of one bool check.
	bool isEnabled();
	void publishEvent(const char* szEvent, const char* szJsonData);
}

#endif
