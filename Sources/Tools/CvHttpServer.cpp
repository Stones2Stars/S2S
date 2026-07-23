#include "CvGameCoreDLL.h"
#include "Infrastructure/CvDLLEntityIFaceBase.h"
#include "CvHttpServer.h"
#include "CvBuildingInfo.h"
#include "Engine/CvPropertySource.h" // property-source completeness oracle: getSource()->getProperty()
#include "Engine/CvPropertyManipulators.h" // the property CONSTANT-source recompute (the property channel's net)
#include "Property/CvPropertyChannel.h"     // the §430 property channel's per-city sourced numbers
#include <psapi.h>                           // /computed/perf memory gauge: GetProcessMemoryInfo (the CvPlotPaging mechanism)
#include "AI/BetterBTSAI.h"                  // /computed/perf frameAccumMs: the whole-turn frame-span ms accumulators
#include "Data/CvReadJson.h"       // /state/info: rjInfoForType -- the info-object edge dump (DEC-one-reverse-view)
#include "Tally/CvTally.h"          // /computed/tally TAG_ routing -> countUnitsWithTag (the per-tag unit count)
#include "CvJsonBoolBlock.h"        // /state/info classification exposure: the loaded tags/skills held-key sets
#include "Enabler/CvBuildingEnabler.h"       // /computed/enabler/buildings: the per-city domain's oracle verification
#include "Enabler/CvUnitEnabler.h"           // /computed/enabler/units: the per-unit verdict decomposition
#include "CvBonusInfo.h" // bonus-name resolution in the /diagnostic/whyNot trace
#include "CvImprovementInfo.h" // cityInput loadout: worked-plot improvement type
#include "CvTraitInfo.h" // cityInput loadout: player trait list
#include "CvYieldInfo.h" // /extractor world.config: per-yield trade-modifier base (YieldInfo.getTradeModifier)
#include "CvGameOptionInfo.h" // /extractor world.options: active game-option type names
#include "CvProjectInfo.h"    // /extractor team.projects: completed project type names
#include "CvHeritageInfo.h"   // /extractor empire.heritages: owned heritage type names
#include "CvEraInfo.h"        // /extractor empire.era: current era type name
#include "Engine/CvCity.h"
#include "Engine/CvPlot.h" // pCity->plot()->canTrain in the /diagnostic/whyNot trace
#include "Engine/CvMap.h"  // /state/plots: the global all-plots walk (plotByIndex/numPlots)
#include "Engine/CvArea.h" // /state/plots: per-plot area id
#include "AI/CvGameAI.h"
#include "Defines/CvGlobals.h"
#include "CvInfos.h"
#include "AI/CvPlayerAI.h"
#include "Engine/CvSelectionGroup.h"
#include "AI/CvTeamAI.h"
#include "Engine/CvUnit.h"
#include "CvUnitCombatInfo.h" // /computed/cities/yields heal-per-unitcombat decomposition (getUnitCombatInfo().getType())
#include "Enabler/CvCapabilities.h" // /computed/teamFlags hasLanguage (the legacy latch is cut, #430)
#include "Enabler/CvEnablerKernel.h" // wireOperatingBuildings for the wellbeing eval ctx
// NB no Cascade headers: this surface serves RAW state (/state) and the ENGINE's own answers (/computed)
// only -- the cascade-vs-legacy shadow comparison was retired (the cutover is validated by the external
// dry-calc + logging). See docs/specs/http-endpoints.md.

// Deliberately the winsock 1.1 header, NOT winsock2.h: some unity batches pull a
// full-fat windows.h (no WIN32_LEAN_AND_MEAN) which includes winsock.h, and
// winsock2.h cannot be included after it (sockaddr redefinition in ws2def.h).
// winsock.h is immune to that under any unity reshuffle -- it is a guarded no-op
// when already present and self-sufficient when not. Everything used here is 1.1
// API, and ws2_32.dll exports all of it.
#include <winsock.h>

#ifndef SD_SEND
#define SD_SEND 1 // winsock.h omits the SD_* shutdown() constants (winsock2-only)
#endif

// Already on the linker LIBPATH (vendored Windows SDK v6.0); no fbuild.bff change needed.
#pragma comment(lib, "ws2_32.lib")

namespace
{
	const unsigned short HTTP_PORT = 7227;
	const int REQUEST_CAP = 4096;
	const int TARGET_CAP = 512;          // path + query taken from the request line
	const DWORD PUBLISH_INTERVAL_MS = 5000;

	HANDLE g_hThread = NULL;
	volatile LONG g_iStopRequested = 0;

	// --- Published header ---------------------------------------------------------
	// The server thread NEVER touches game objects, but it needs the current turn (the
	// X-S2S-Turn response header) and gameId (the /events hello) without a mailbox
	// round-trip. The game thread refreshes this tiny scalar pair every frame
	// (publishIfDue); the server thread reads its own refcounted copy. All real data
	// is served on the game thread via the mailbox -- there is no bulk snapshot anymore
	// (the old units/players/cities snapshot existed only for the retired dashboard
	// endpoints; see docs/specs/http-endpoints.md "What was dropped").
	struct GameSnapshot
	{
		GameSnapshot() : iTurn(-1) {}
		int iTurn;
		CvString szGameId; // playtest id (CvGame::getGameId; digits-only yyMMddHHmm for new games)
	};

	// Narrow a wide game string to ASCII for the snapshot; non-ASCII becomes '?'.
	// No escaping here -- free-text fields are rendered through picojson, which owns
	// JSON escaping (the documented rendering rule; FAssert's AssertsJson.log precedent).
	CvString narrowToAscii(const CvWString& wName)
	{
		CvString szOut;
		for (int i = 0; i < (int)wName.length(); ++i)
		{
			const wchar_t wc = wName[i];
			szOut += (wc < 0x20 || wc > 0x7E) ? '?' : (char)wc;
		}
		return szOut;
	}

	CRITICAL_SECTION g_snapshotLock;
	bool g_bLockInitialized = false;
	// The lock only ever guards the pointer copy/swap (nanoseconds on both sides);
	// readers render from their own refcounted reference to the immutable snapshot,
	// so a slow full-dump render can never stall the game thread's publish.
	bst::shared_ptr<const GameSnapshot> g_pSnapshot; // guarded by g_snapshotLock
	DWORD g_iLastPublishTick = 0;                    // game thread only

	// --- SSE turn-event stream (#407) -----------------------------------------------
	// The game thread enqueues pre-rendered SSE frames (CvHttpServer::publishEvent);
	// the server thread drains and broadcasts them to the connected /events clients.
	// Same contract as the snapshot: the server thread never touches game objects.
	CRITICAL_SECTION g_eventLock;          // initialized alongside g_snapshotLock
	std::vector<CvString> g_pendingEvents; // guarded by g_eventLock
	// Backstop: drop new events beyond this. BUMPED 2048 -> 65536 (owner 2026-06-18): the original small cap was sized
	// against a memory worry that proved UNFOUNDED (32-bit footprint was a non-issue). At log level 3 a single turn floods
	// ~100k frames (full BBAI firehose), overrunning the old cap and silently DROPPING frames -- making /events a LOSSY
	// sample, not the complete live record the render-from-API/narrate-the-turn goal needs. 65536 frames x ~150-250B =>
	// ~10-16MB worst-case transient, fine on the LAA process for playtesting; self-draining so it never sits full.
	const size_t EVENT_QUEUE_CAP = 65536;
	int g_iDroppedEvents = 0;              // guarded by g_eventLock -- frames lost to the cap since the last marker
	std::vector<SOCKET> g_sseClients;      // server thread only
	const size_t SSE_CLIENT_CAP = 8;

	// --- diagnostic gate-eval mailbox (#430 readJson testing) ------------------------------
	// /diagnostic/* gate queries (canConstruct/canTrain/...) read live CvPlayer/CvCity, so they run on the
	// GAME thread (the server thread NEVER touches game objects -- the hard constraint). One in-flight slot:
	// the server thread fills the request + flips to EVAL_PENDING; serviceEvalMailbox() (game thread, from
	// publishIfDue) renders the JSON answer + flips to EVAL_DONE; the server reads it back. "5s-stale is
	// sufficient" (owner) -- the answer reflects a consistent game-state read on the game thread's next tick.
	enum { EVAL_IDLE = 0, EVAL_PENDING = 1, EVAL_DONE = 2 };
	CRITICAL_SECTION g_evalLock;       // initialized alongside g_snapshotLock / g_eventLock
	volatile LONG g_evalState = EVAL_IDLE;
	char g_evalAction[40] = { 0 };     // "canConstruct" | "canTrain" | ...   (server -> game)
	char g_evalType[96]   = { 0 };     // e.g. BUILDING_FORGE
	int  g_evalPlayer     = -1;        // -1 == use the active player (resolved game-side)
	int  g_evalCity       = -1;        // -1 == the player's capital (resolved game-side); else a city id
	int  g_evalUnit       = -1;        // -1 == no unit selected; else a unit id (per-player-unique; /computed/units/heal)
	CvString g_evalResult;             // the rendered JSON answer            (game -> server), guarded by g_evalLock

	bst::shared_ptr<const GameSnapshot> grabSnapshot()
	{
		EnterCriticalSection(&g_snapshotLock);
		const bst::shared_ptr<const GameSnapshot> pSnap = g_pSnapshot;
		LeaveCriticalSection(&g_snapshotLock);
		return pSnap;
	}

	// --- Socket plumbing ----------------------------------------------------------

	const char RESPONSE_405[] =
		"HTTP/1.0 405 Method Not Allowed\r\n"
		"Allow: GET\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 9\r\n"
		"Connection: close\r\n"
		"\r\n"
		"GET only\n";

	bool sendAll(SOCKET sock, const char* szData, int iLen)
	{
		int iSent = 0;
		while (iSent < iLen)
		{
			const int iRet = send(sock, szData + iSent, iLen - iSent, 0);
			if (iRet == SOCKET_ERROR || iRet == 0)
			{
				return false;
			}
			iSent += iRet;
		}
		return true;
	}

	void sendResponse(SOCKET sock, const char* szStatus, const char* szContentType, const CvString& szBody, int iTurn)
	{
		const CvString szHead = CvString::format(
			"HTTP/1.0 %s\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %u\r\n"
			"X-S2S-Turn: %d\r\n"
			"Connection: close\r\n"
			"\r\n",
			szStatus, szContentType, (unsigned int)szBody.size(), iTurn);
		sendAll(sock, szHead.c_str(), (int)szHead.size());
		sendAll(sock, szBody.c_str(), (int)szBody.size());
	}

	// --- Request handling (server thread) -------------------------------------------
	// The server thread renders only the {turn,gameId} header here; every /state and
	// /computed document is produced ON THE GAME THREAD (evaluate(), via the mailbox).

	int snapshotTurn()
	{
		const bst::shared_ptr<const GameSnapshot> pSnap = grabSnapshot();
		return pSnap ? pSnap->iTurn : -1;
	}

	// --- SSE turn-event stream (server thread; #407) --------------------------------

	// The /events preamble: a response that never ends (no Content-Length -- the
	// stream IS the body). The hello event carries the current snapshot turn and
	// gameId so a client syncs immediately and detects reloads on reconnect.
	void beginEventStream(SOCKET sock)
	{
		const bst::shared_ptr<const GameSnapshot> pSnap = grabSnapshot();
		const CvString szHead = CvString::format(
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/event-stream\r\n"
			"Cache-Control: no-cache\r\n"
			"Connection: keep-alive\r\n"
			"Access-Control-Allow-Origin: *\r\n"
			"\r\n"
			"retry: 3000\n\n"
			"event: hello\ndata: {\"turn\":%d,\"gameId\":\"%s\"}\n\n",
			pSnap ? pSnap->iTurn : -1, pSnap ? pSnap->szGameId.c_str() : "");
		sendAll(sock, szHead.c_str(), (int)szHead.size());
	}

	// Server thread (NEVER the profiler -- it is not thread-safe).
	void broadcastPendingEvents()
	{
		if (g_sseClients.empty())
		{
			// Nobody listening -- still drain, so the queue cannot sit full while idle.
			EnterCriticalSection(&g_eventLock);
			g_pendingEvents.clear();
			LeaveCriticalSection(&g_eventLock);
			return;
		}
		std::vector<CvString> events;
		EnterCriticalSection(&g_eventLock);
		events.swap(g_pendingEvents);
		LeaveCriticalSection(&g_eventLock);

		if (events.empty())
		{
			return;
		}
		// Batched send (#419): concatenate the drained frames and write once per client
		// per drain pass -- with the log stream a heavy turn carries hundreds of frames,
		// and per-frame send() calls would multiply syscalls for no benefit.
		size_t iTotal = 0;
		for (size_t i = 0; i < events.size(); ++i)
		{
			iTotal += events[i].size();
		}
		CvString szBatch;
		szBatch.reserve(iTotal);
		for (size_t i = 0; i < events.size(); ++i)
		{
			szBatch += events[i];
		}
		for (size_t j = 0; j < g_sseClients.size(); )
		{
			if (!sendAll(g_sseClients[j], szBatch.c_str(), (int)szBatch.size()))
			{
				closesocket(g_sseClients[j]);
				g_sseClients.erase(g_sseClients.begin() + j);
			}
			else
			{
				++j;
			}
		}
	}

	// --- game-thread evaluation -------------------------------------------------------
	// ⛔ THE ENDPOINT SURFACE IS PURGED (owner). The old /state + /computed bodies had served their
	// purpose and had become a chief source of rollerskating: ~4,250 lines of hand-rolled per-feature
	// renderers, each reaching directly into engine internals, which is exactly the accumulation
	// http-endpoints.md warns against ("the server SERVES state; it does not ACCUMULATE it").
	// What survives is the SERVER itself + the event listener: sockets, the SSE /events stream, and
	// the game-thread mailbox mechanism (the one piece that is genuinely hard and proven correct --
	// the server thread must never touch a live game object). A new endpoint surface is built on the
	// mailbox when there is something to serve.
	CvString evaluateGate(const char* szAction, const char* /*szType*/, int /*iPlayer*/, int /*iCity*/, int /*iUnit*/)
	{
		picojson::value::object o;
		o["error"]  = picojson::value(std::string("no endpoint surface"));
		o["action"] = picojson::value(std::string(szAction != NULL ? szAction : ""));
		return CvString(picojson::value(o).serialize().c_str());
	}

	// GAME THREAD (publishIfDue): if a diagnostic request is pending, render its answer and mark it done.
	void serviceEvalMailbox()
	{
		if (!g_bLockInitialized || g_evalState != EVAL_PENDING)
		{
			return; // fast idle peek -- no lock taken when nothing is pending
		}
		char szAction[40]; char szType[96]; int iPlayer; int iCity; int iUnit;
		EnterCriticalSection(&g_evalLock);
		if (g_evalState != EVAL_PENDING) { LeaveCriticalSection(&g_evalLock); return; }
		strncpy(szAction, g_evalAction, sizeof(szAction)); szAction[sizeof(szAction) - 1] = '\0';
		strncpy(szType, g_evalType, sizeof(szType)); szType[sizeof(szType) - 1] = '\0';
		iPlayer = g_evalPlayer;
		iCity = g_evalCity;
		iUnit = g_evalUnit;
		LeaveCriticalSection(&g_evalLock);

		const CvString szResult = evaluateGate(szAction, szType, iPlayer, iCity, iUnit); // safe: game thread

		EnterCriticalSection(&g_evalLock);
		g_evalResult = szResult;
		g_evalState = EVAL_DONE;
		LeaveCriticalSection(&g_evalLock);
	}

	// SERVER THREAD: enqueue a request, then wait (bounded) for the game thread to render the answer.
	bool evalRequestBlocking(const char* szAction, const char* szType, int iPlayer, int iCity, int iUnit, CvString& szAnswerOut)
	{
		if (!g_bLockInitialized)
		{
			return false;
		}
		EnterCriticalSection(&g_evalLock);
		if (g_evalState == EVAL_PENDING) { LeaveCriticalSection(&g_evalLock); return false; } // another request in flight
		strncpy(g_evalAction, szAction, sizeof(g_evalAction)); g_evalAction[sizeof(g_evalAction) - 1] = '\0';
		strncpy(g_evalType, szType, sizeof(g_evalType)); g_evalType[sizeof(g_evalType) - 1] = '\0';
		g_evalPlayer = iPlayer;
		g_evalCity = iCity;
		g_evalUnit = iUnit;
		g_evalState = EVAL_PENDING;
		LeaveCriticalSection(&g_evalLock);

		// publishIfDue() ticks every frame, so this normally returns well under a second; the cap stops a
		// paused / non-ticking game thread from wedging the handler. Sized to absorb the one-time index builds
		// (obsoletion scan over tech JSONs + the upgrade scan over ~2k unit JSONs on the first gate query).
		for (int iWaited = 0; iWaited < 18000; iWaited += 10)
		{
			if (g_evalState == EVAL_DONE)
			{
				EnterCriticalSection(&g_evalLock);
				szAnswerOut = g_evalResult;
				g_evalState = EVAL_IDLE;
				LeaveCriticalSection(&g_evalLock);
				return true;
			}
			Sleep(10);
		}
		return false; // timeout: leave the slot (the game thread flips it to DONE; the next request reclaims it)
	}

	// Returns true if the socket joined the SSE client list and must stay open.
	bool handleRequest(SOCKET sock, const char* szRequest)
	{
		if (strncmp(szRequest, "GET ", 4) != 0)
		{
			sendAll(sock, RESPONSE_405, (int)strlen(RESPONSE_405));
			return false;
		}

		// Extract the request target ("/computed/canConstruct?type=BUILDING_FORGE&player=0") -- everything
		// between the method and the next space/CR, length-capped.
		char szTarget[TARGET_CAP + 1];
		int iLen = 0;
		for (const char* p = szRequest + 4; *p != '\0' && *p != ' ' && *p != '\r' && *p != '\n' && iLen < TARGET_CAP; ++p)
		{
			szTarget[iLen++] = *p;
		}
		szTarget[iLen] = '\0';

		// Split path from query string.
		char* szQuery = strchr(szTarget, '?');
		if (szQuery != NULL)
		{
			*szQuery = '\0';
			++szQuery;
		}

		// ---- the route table: the SINGLE place every endpoint is declared (path -> mailbox action + doc). ----
		// A human (and the /state and /computed index pages) reads THIS to see the whole surface. /state actions
		// emit RAW inputs; /computed actions emit the engine's own answers. Every data route is serviced on the
		// game thread via the mailbox (evalRequestBlocking). See docs/specs/http-endpoints.md.
		struct Route { const char* szPath; const char* szAction; const char* szDoc; };
		// The route table is EMPTY: the endpoint surface was purged (see evaluateGate above). The dispatch
		// machinery below is kept intact so a new surface plugs into the proven mailbox rather than being
		// re-invented -- add rows here and they route through the game thread exactly as before.
		static const Route ROUTES[1] = { { NULL, NULL, NULL } };

		const int iNumRoutes = 0;   // the surface is purged; ROUTES[0] is a placeholder, never dispatched

		// liveness + the SSE stream are served on THIS (server) thread; every data route goes through the mailbox.
		if (strcmp(szTarget, "/") == 0)
		{
			sendResponse(sock, "200 OK", "text/plain", CvString("hello world\n"), snapshotTurn());
			return false;
		}
		if (strcmp(szTarget, "/events") == 0)
		{
			if (g_sseClients.size() >= SSE_CLIENT_CAP)
			{
				sendResponse(sock, "503 Service Unavailable", "application/json",
					CvString("{\"error\":\"too many event streams\"}\n"), snapshotTurn());
				return false;
			}
			beginEventStream(sock);
			g_sseClients.push_back(sock);
			return true; // keep the socket open for the broadcast loop
		}

		// /state and /computed (bare) -> the index of that bucket's routes, generated from the table above.
		if (strcmp(szTarget, "/state") == 0 || strcmp(szTarget, "/computed") == 0)
		{
			const size_t iPrefix = strlen(szTarget);
			picojson::value::array eps;
			for (int i = 0; i < iNumRoutes; ++i)
				if (strncmp(ROUTES[i].szPath, szTarget, iPrefix) == 0 && ROUTES[i].szPath[iPrefix] == '/')
				{
					picojson::value::object e;
					e["path"] = picojson::value(std::string(ROUTES[i].szPath));
					e["doc"]  = picojson::value(std::string(ROUTES[i].szDoc));
					eps.push_back(picojson::value(e));
				}
			picojson::value::object root;
			root["endpoints"] = picojson::value(eps);
			root["note"] = picojson::value(std::string(strcmp(szTarget, "/state") == 0
				? "RAW inputs only (no drycalc target). ?player=N filters; cities also accept &city=M."
				: "the engine's computed answers (verification ground-truth). ?player=N[&city=M]; gates need type=PREFIX_NAME."));
			CvString szBody(picojson::value(root).serialize().c_str());
			szBody += "\n";
			sendResponse(sock, "200 OK", "application/json", szBody, snapshotTurn());
			return false;
		}

		// Match a data route, parse the shared query params (type/player/city), and dispatch via the mailbox.
		for (int i = 0; i < iNumRoutes; ++i)
		{
			if (strcmp(szTarget, ROUTES[i].szPath) != 0) continue;

			char szType[96]; szType[0] = '\0';
			int iPlayer = -1; // -1 == active player (/computed) or ALL players (/state), resolved game-side
			int iCity = -1;   // -1 == the player's capital (/computed) or no city filter (/state)
			int iUnit = -1;   // -1 == no unit selected (/computed/units/heal keys on ?player=N&unit=M)
			char* szTok = szQuery;
			while (szTok != NULL && *szTok != '\0')
			{
				char* szNext = strchr(szTok, '&');
				if (szNext != NULL) { *szNext = '\0'; ++szNext; }
				if (strncmp(szTok, "type=", 5) == 0) { strncpy(szType, szTok + 5, sizeof(szType)); szType[sizeof(szType) - 1] = '\0'; }
				else if (strncmp(szTok, "player=", 7) == 0) iPlayer = atoi(szTok + 7);
				else if (strncmp(szTok, "city=", 5) == 0) iCity = atoi(szTok + 5);
				else if (strncmp(szTok, "unit=", 5) == 0) iUnit = atoi(szTok + 5);
				else if (strncmp(szTok, "globalId=", 9) == 0)
				{
					// the "<PP>-<id>" snowflake selector -> (player, city). atoi stops at '-' (leading zeros fine);
					// the part after '-' is the engine city id. One param instead of player=&city=.
					const char* v = szTok + 9;
					const char* dash = strchr(v, '-');
					if (dash != NULL) { iPlayer = atoi(v); iCity = atoi(dash + 1); }
				}
				szTok = szNext;
			}

			CvString szAnswer;
			if (evalRequestBlocking(ROUTES[i].szAction, szType, iPlayer, iCity, iUnit, szAnswer))
				sendResponse(sock, "200 OK", "application/json", szAnswer, snapshotTurn());
			else
				sendResponse(sock, "503 Service Unavailable", "application/json",
					CvString("{\"error\":\"eval busy or game thread not ticking; retry\"}\n"), snapshotTurn());
			return false;
		}

		sendResponse(sock, "404 Not Found", "application/json",
			CvString("{\"error\":\"not found; see /state and /computed\"}\n"), snapshotTurn());
		return false;
	}

	void handleClient(SOCKET sock)
	{
		// Bound every socket op so a stalled client cannot wedge the server thread
		// (and with it the option-off shutdown wait) for more than ~2s.
		const int iTimeoutMs = 2000;
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&iTimeoutMs, sizeof(iTimeoutMs));
		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&iTimeoutMs, sizeof(iTimeoutMs));

		char szRequest[REQUEST_CAP + 1];
		int iReceived = 0;
		while (iReceived < REQUEST_CAP)
		{
			const int iRet = recv(sock, szRequest + iReceived, REQUEST_CAP - iReceived, 0);
			if (iRet == SOCKET_ERROR || iRet == 0)
			{
				break;
			}
			iReceived += iRet;
			szRequest[iReceived] = '\0';
			if (strstr(szRequest, "\r\n\r\n") != NULL)
			{
				break; // headers complete; the request line is all we need
			}
		}

		bool bKeepOpen = false;
		if (iReceived > 0)
		{
			szRequest[iReceived] = '\0';
			bKeepOpen = handleRequest(sock, szRequest);
			if (!bKeepOpen)
			{
				shutdown(sock, SD_SEND);
			}
		}
		if (!bKeepOpen)
		{
			closesocket(sock);
		}
	}

	void runServerLoop()
	{
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
		{
			return;
		}

		const SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listenSock == INVALID_SOCKET)
		{
			WSACleanup();
			return;
		}

		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 only -- never exposed off-machine
		addr.sin_port = htons(HTTP_PORT);

		// boost::bind is visible at global scope via the PCH's using-directives and VC7.1
		// resolves even a qualified ::bind call to it. Select winsock's bind through a
		// typed function pointer instead -- the boost template cannot match this signature,
		// so the address-of uniquely picks the winsock function.
		int (PASCAL *pfnWinsockBind)(SOCKET, const struct sockaddr*, int) = &::bind;
		if (pfnWinsockBind(listenSock, (sockaddr*)&addr, (int)sizeof(addr)) == SOCKET_ERROR
			|| listen(listenSock, SOMAXCONN) == SOCKET_ERROR)
		{
			// Most likely the port is in use (e.g. a second game instance); nothing to serve.
			closesocket(listenSock);
			WSACleanup();
			return;
		}

		DWORD iLastKeepaliveTick = GetTickCount();

		while (g_iStopRequested == 0)
		{
			// select() with a timeout so the stop flag is honoured within 250ms
			// without the game thread having to close the socket out from under us.
			// The persistent /events clients are watched too: them becoming readable
			// means stray data (drained, ignored) or a disconnect to reap.
			fd_set readSet;
			FD_ZERO(&readSet);
			FD_SET(listenSock, &readSet);
			for (size_t i = 0; i < g_sseClients.size(); ++i)
			{
				FD_SET(g_sseClients[i], &readSet);
			}
			timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 250 * 1000;

			const int iReady = select(0, &readSet, NULL, NULL, &timeout);
			if (iReady == SOCKET_ERROR)
			{
				break;
			}
			if (iReady > 0)
			{
				if (FD_ISSET(listenSock, &readSet))
				{
					const SOCKET clientSock = accept(listenSock, NULL, NULL);
					if (clientSock != INVALID_SOCKET)
					{
						handleClient(clientSock);
					}
				}
				for (size_t i = 0; i < g_sseClients.size(); )
				{
					if (FD_ISSET(g_sseClients[i], &readSet))
					{
						char szDrain[256];
						const int iRet = recv(g_sseClients[i], szDrain, sizeof(szDrain), 0);
						if (iRet == 0 || iRet == SOCKET_ERROR)
						{
							closesocket(g_sseClients[i]);
							g_sseClients.erase(g_sseClients.begin() + i);
							continue;
						}
					}
					++i;
				}
			}

			broadcastPendingEvents();

			// Comment-line keepalive: detects half-dead clients between turns and
			// keeps idle streams from being timed out by intermediaries.
			const DWORD iNow = GetTickCount();
			if (iNow - iLastKeepaliveTick >= 15000)
			{
				iLastKeepaliveTick = iNow;
				for (size_t i = 0; i < g_sseClients.size(); )
				{
					if (!sendAll(g_sseClients[i], ": keepalive\n\n", 13))
					{
						closesocket(g_sseClients[i]);
						g_sseClients.erase(g_sseClients.begin() + i);
					}
					else
					{
						++i;
					}
				}
			}
		}

		for (size_t i = 0; i < g_sseClients.size(); ++i)
		{
			closesocket(g_sseClients[i]);
		}
		g_sseClients.clear();

		closesocket(listenSock);
		WSACleanup();
	}

	DWORD WINAPI serverThreadEntry(LPVOID lpModule)
	{
		// SEH guard: a fault on this thread must never take the game down.
		__try
		{
			runServerLoop();
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
		// Releases the DLL pin taken in setEnabled and exits in one step, so the EXE
		// can never unmap the DLL while this thread is still executing inside it.
		FreeLibraryAndExitThread((HMODULE)lpModule, 0);
	}
}

void CvHttpServer::setEnabled(bool bEnable)
{
	if (bEnable == (g_hThread != NULL))
	{
		return;
	}

	if (bEnable)
	{
		if (!g_bLockInitialized)
		{
			InitializeCriticalSection(&g_snapshotLock);
			InitializeCriticalSection(&g_eventLock);
			InitializeCriticalSection(&g_evalLock);
			g_bLockInitialized = true;
		}
		g_iLastPublishTick = 0; // force a fresh snapshot on the next frame

		// Drop any events queued by a previous server incarnation.
		EnterCriticalSection(&g_eventLock);
		g_pendingEvents.clear();
		LeaveCriticalSection(&g_eventLock);

		// Pin the DLL for the thread's lifetime BEFORE starting it (the thread
		// releases the pin as it exits, via FreeLibraryAndExitThread).
		HMODULE hModule = NULL;
		if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
			(LPCSTR)&serverThreadEntry, &hModule))
		{
			return;
		}
		InterlockedExchange(&g_iStopRequested, 0);
		g_hThread = CreateThread(NULL, 0, serverThreadEntry, (LPVOID)hModule, 0, NULL);
		if (g_hThread == NULL)
		{
			FreeLibrary(hModule);
		}
	}
	else
	{
		InterlockedExchange(&g_iStopRequested, 1);
		// The loop polls the flag every 250ms and client socket ops are capped at 2s,
		// so 5s only times out if something is genuinely wedged -- in which case we
		// leak the handle rather than TerminateThread a thread that owns a socket.
		if (WaitForSingleObject(g_hThread, 5000) == WAIT_OBJECT_0)
		{
			CloseHandle(g_hThread);
		}
		g_hThread = NULL;
	}
}

bool CvHttpServer::isEnabled()
{
	return g_hThread != NULL;
}

// Game thread (#407): enqueue a turn-boundary event for the /events SSE stream. The
// frame is pre-rendered here so the server thread never touches game objects; a cheap
// no-op while the server is off (but see the header: guard payload formatting with
// isEnabled() at the call site). Events beyond the queue cap are dropped (a backstop
// against a wedged server thread -- the stream is advisory dev tooling, never truth).
void CvHttpServer::publishEvent(const char* szEvent, const char* szJsonData)
{
	if (g_hThread == NULL)
	{
		return;
	}
	const CvString szFrame = CvString::format("event: %s\ndata: %s\n\n", szEvent, szJsonData);

	EnterCriticalSection(&g_eventLock);
	if (g_pendingEvents.size() < EVENT_QUEUE_CAP)
	{
		// No silent caps (observability.md): if the cap dropped frames, the first frame that fits again says
		// how many were lost, so a gap in /events is always visible AS a gap.
		if (g_iDroppedEvents > 0)
		{
			g_pendingEvents.push_back(CvString::format("event: log\ndata: [STREAM] dropped=%d (event queue overflow)\n\n", g_iDroppedEvents));
			g_iDroppedEvents = 0;
		}
		g_pendingEvents.push_back(szFrame);
	}
	else ++g_iDroppedEvents;
	LeaveCriticalSection(&g_eventLock);
}

// Game thread, once per frame from CvGame::update. Walks live game objects HERE
// (the only thread allowed to) and swaps the result into the served snapshot.
void CvHttpServer::publishIfDue()
{
	if (g_hThread == NULL)
	{
		return; // server off -- this bool check is the entire cost
	}

	// Service any pending /diagnostic gate query first (every frame, ahead of the snapshot throttle): the
	// gates read live game objects, so the game thread -- this one -- is the only place they may run.
	serviceEvalMailbox();

	const DWORD iNow = GetTickCount();
	if (g_iLastPublishTick != 0 && iNow - g_iLastPublishTick < PUBLISH_INTERVAL_MS)
	{
		return;
	}
	g_iLastPublishTick = iNow;

	// Refresh the published header (turn + gameId) for the server thread's response metadata and the
	// /events hello. No bulk walk: /state and /computed are served on this thread via the mailbox.
	bst::shared_ptr<GameSnapshot> pNew(new GameSnapshot());
	pNew->iTurn = GC.getGame().getGameTurn();
	pNew->szGameId = GC.getGame().getGameId();

	EnterCriticalSection(&g_snapshotLock);
	g_pSnapshot = pNew;
	LeaveCriticalSection(&g_snapshotLock);
}
