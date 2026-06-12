#pragma once

#ifndef CV_HTTP_SERVER_H
#define CV_HTTP_SERVER_H

// Minimal GET-only HTTP/1.0 dev server -- the live game-state endpoint (#387).
// Gated by the BUG option Autolog__HttpServer (Logging tab, off by default);
// binds 127.0.0.1:7227 only, so it is never reachable from off-machine.
// Endpoints (anything but GET gets 405, Allow: GET):
//   GET /                           -> "hello world" (the smoke test)
//   GET /units                      -> JSON array of every unit in the game
//   GET /units?id=N                 -> filtered to one unit id
//   GET /units?playerNumber=N       -> filtered to one player's units (combinable with id)
//   GET /players                    -> JSON array of every alive player: score, era, tech
//                                      count, current research, cities, population, units,
//                                      gold(+rate), science rate, production, handicap
//   GET /players?playerNumber=N     -> filtered to one player
//   GET /cities                     -> JSON array of every city: position, name, population,
//                                      yield rates, production head (+turns left), building
//                                      count, culture level, capital flag, and the live
//                                      property values: crime, education, disease
//   GET /cities?id=N                -> filtered to one city id (combinable with playerNumber)
//   GET /cities?playerNumber=N      -> filtered to one player's cities
//   GET /events                     -> Server-Sent Events stream (#407): "hello" with the
//                                      current turn + gameId on connect; "turnEnd" (old
//                                      turn) / "turnStart" (new turn) bracketing every
//                                      game-turn increment; "playerTurnStart" /
//                                      "playerTurnEnd" for HUMAN players only (the live
//                                      "human thinking vs AI processing" phase signal --
//                                      turn DURATION analytics belong to the [PERF]
//                                      logs); ": keepalive" comments every ~15s.
//                                      text/event-stream; the response never ends. At
//                                      most 8 concurrent streams (503 beyond).
// The /units and /players wrappers carry "gameId" (JSON string) -- CvGame::getGameId(),
// the persistent playtest identity stamped at game creation (digits-only yyMMddHHmm local
// time for new games; saves predating the format change carry "DD-MM-YYYY HH:MM:SS") --
// so tooling can tell playtests apart and detect reloads/new games mid-session.
// Responses carry X-S2S-Turn (the game turn the snapshot was taken on). Data is
// "as of the last publish" -- the game thread refreshes it every few seconds via
// publishIfDue(); unknown query parameters are ignored.
//
// Runs on its own Win32 thread. HARD CONSTRAINT: that thread NEVER touches live
// game objects -- it only reads the immutable snapshot the game thread published
// (the same publish-and-serve contract the derived-data repositories will use;
// see docs/plans/derived-data-repository.md section 8b).
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
	// server thread never touches game objects; cheap no-op when the server is off.
	void publishEvent(const char* szEvent, const char* szJsonData);
}

#endif
