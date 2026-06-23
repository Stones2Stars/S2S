#include "CvGameCoreDLL.h"
#include "CvHttpServer.h"
#include "CvBuildingInfo.h"
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
#include "AI/CvGameAI.h"
#include "Defines/CvGlobals.h"
#include "CvInfos.h"
#include "AI/CvPlayerAI.h"
#include "Engine/CvSelectionGroup.h"
#include "AI/CvTeamAI.h"
#include "Engine/CvUnit.h"
#include "CvCascadeReadJson.h" // cascadeReadJsonBuildingAvailability -- the /diagnostic/canConstruct cascade verdict
#include "CvCascadeTally.h"    // cascadeTally / CountDomain / CountScope (CvEntityAvailability + cascadeBuildable via the above)
#include "CvCascadeMovement.h" // cascadeResolveMoveCost -- the movementSweep cascade-vs-legacy shadow column
#include "CvUnitCombatInfo.h"  // GC.getUnitCombatInfo().getType() -- the unit-plane per-source attribution

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

	// --- Published snapshot -------------------------------------------------------
	// Written by the game thread (publishIfDue), read by the server thread. The lock
	// is initialized before the server thread can exist (setEnabled) and publish
	// early-outs while the server is off, so neither side touches it uninitialized.
	struct UnitSnap
	{
		int iID;
		int iOwner;
		int iX;
		int iY;
		int iGroup;
		int iMissionAI;
		int iActivity;
		int iDamage;
		int iLevel;
		// Movement/range observability (#430 movement model -- the "observe" half of observe-then-shadow,
		// modifier.md 6.6). All O(1) const reads off CvUnit; the data is landed-but-unconsumed, so these are
		// the LEGACY effective values the movement/range converter is diffed against.
		int iBaseMoves;    // baseMoves()           -- iMoves + extraMoves + team domain moves (small int)
		int iMaxMoves;     // maxMoves()            -- baseMoves * MOVE_DENOMINATOR (the x100 per-turn budget)
		int iMovesLeft;    // movesLeft()           -- remaining budget this turn (max(0, maxMoves - getMoves))
		int iMoveDiscount; // getExtraMoveDiscount()-- unit-side -cost discount (commander/commodore-borrowed)
		int iRange;        // airRange()            -- the unified range value (air-only today; 0 for ground)
		int iDomain;       // getDomainType()       -- DOMAIN_* (explains the flat/air moveCost early-returns)
		CvString szType; // XML key, e.g. UNIT_WARDOG
		CvString szAI;   // XML key, e.g. UNITAI_HUNTER
	};

	struct PlayerSnap
	{
		int iID;
		int iTeam;
		int iHuman;
		int iNPC;
		int iScore;
		int iEra;
		int iTechs;
		int iCities;
		int iPopulation;
		int iUnits;
		int64_t iGold; // CvPlayer::getGold() is 64-bit
		int iGoldRate;
		int iScienceRate;
		int iProduction;
		CvString szCiv;      // XML key, e.g. CIVILIZATION_ENGLAND
		CvString szName;     // sanitized to JSON-safe ASCII
		CvString szResearch; // XML key of the current research, or NONE
		CvString szHandicap; // XML key, e.g. HANDICAP_EMPEROR -- the per-player difficulty
	};

	struct CitySnap
	{
		int iID;
		int iOwner;
		int iX;
		int iY;
		int iPopulation;
		int iFood;            // YIELD_FOOD rate
		int iProduction;      // YIELD_PRODUCTION rate
		int iCommerce;        // YIELD_COMMERCE rate
		int iProducingTurns;  // turns left on the current production (0 when idle)
		int iNumBuildings;
		int iCultureLevel;
		int iCapital;
		// The property values worth tracking (owner ruling 2026-06-11: crime, education
		// and disease carry real gameplay; flammability and the pollutions are dormant).
		int iCrime;
		int iEducation;
		int iDisease;
		CvString szName;      // sanitized to ASCII; escaped by picojson
		CvString szProducing; // XML key of the production head, or NONE
	};

	struct GameSnapshot
	{
		GameSnapshot() : iTurn(-1) {}
		int iTurn;
		CvString szGameId; // playtest id (CvGame::getGameId; digits-only yyMMddHHmm for new games)
		std::vector<UnitSnap> units;
		std::vector<PlayerSnap> players;
		std::vector<CitySnap> cities;
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

	// --- Request handling (server thread; snapshot reads only) ---------------------

	// Renders the /units document from this thread's own snapshot reference.
	// IMPORTANT: the document is assembled by CONCATENATION -- CvString::format's
	// growth loop caps out around 82KB (40 x 2KB attempts in formatv) and silently
	// returns an EMPTY string beyond that, and a full dump of a mature game is
	// megabytes. format() is only safe here for the small per-unit pieces.
	CvString renderUnits(bool bFilterId, int iId, bool bFilterOwner, int iOwner)
	{
		const bst::shared_ptr<const GameSnapshot> pSnap = grabSnapshot();
		const int iTurn = pSnap ? pSnap->iTurn : -1;

		CvString szItems;
		int iCount = 0;
		if (pSnap)
		{
			szItems.reserve(pSnap->units.size() * 160);
			for (size_t i = 0; i < pSnap->units.size(); ++i)
			{
				const UnitSnap& u = pSnap->units[i];
				if (bFilterId && u.iID != iId)
				{
					continue;
				}
				if (bFilterOwner && u.iOwner != iOwner)
				{
					continue;
				}
				if (iCount > 0)
				{
					szItems += ",";
				}
				szItems += CvString::format(
					"\n{\"id\":%d,\"owner\":%d,\"x\":%d,\"y\":%d,\"type\":\"%s\",\"ai\":\"%s\","
					"\"group\":%d,\"missionAI\":%d,\"activity\":%d,\"damage\":%d,\"level\":%d,"
					"\"baseMoves\":%d,\"maxMoves\":%d,\"movesLeft\":%d,\"moveDiscount\":%d,\"range\":%d,\"domain\":%d}",
					u.iID, u.iOwner, u.iX, u.iY, u.szType.c_str(), u.szAI.c_str(),
					u.iGroup, u.iMissionAI, u.iActivity, u.iDamage, u.iLevel,
					u.iBaseMoves, u.iMaxMoves, u.iMovesLeft, u.iMoveDiscount, u.iRange, u.iDomain);
				iCount++;
			}
		}

		// gameId is emitted as a JSON string: legacy saves carry the old timestamp format
		// (digits, dashes, spaces, colons -- all JSON-string-safe without escaping).
		CvString szBody = CvString::format(
			"{\"turn\":%d,\"gameId\":\"%s\",\"count\":%d,\"units\":[",
			iTurn, pSnap ? pSnap->szGameId.c_str() : "", iCount);
		szBody += szItems;
		szBody += "\n]}\n";
		return szBody;
	}

	// Rendered through picojson, unlike /units: the name field is free text, and the
	// documented rendering rule reserves hand-built JSON for flat ints + XML keys.
	// The document is small (<= MAX_PLAYERS objects), so DOM + serialize costs nothing
	// and is immune to CvString::format's ~82KB cap. picojson objects are std::maps,
	// so fields serialize in alphabetical key order.
	CvString renderPlayers(bool bFilterOwner, int iOwner)
	{
		const bst::shared_ptr<const GameSnapshot> pSnap = grabSnapshot();
		const int iTurn = pSnap ? pSnap->iTurn : -1;

		picojson::value::array players;
		if (pSnap)
		{
			for (size_t i = 0; i < pSnap->players.size(); ++i)
			{
				const PlayerSnap& p = pSnap->players[i];
				if (bFilterOwner && p.iID != iOwner)
				{
					continue;
				}
				picojson::value::object o;
				o["id"] = picojson::value((double)p.iID);
				o["team"] = picojson::value((double)p.iTeam);
				o["civ"] = picojson::value(p.szCiv);
				o["name"] = picojson::value(p.szName);
				o["human"] = picojson::value(p.iHuman != 0);
				o["npc"] = picojson::value(p.iNPC != 0);
				o["score"] = picojson::value((double)p.iScore);
				o["era"] = picojson::value((double)p.iEra);
				o["techs"] = picojson::value((double)p.iTechs);
				o["research"] = picojson::value(p.szResearch);
				o["handicap"] = picojson::value(p.szHandicap);
				o["cities"] = picojson::value((double)p.iCities);
				o["population"] = picojson::value((double)p.iPopulation);
				o["units"] = picojson::value((double)p.iUnits);
				o["gold"] = picojson::value((double)p.iGold);
				o["goldRate"] = picojson::value((double)p.iGoldRate);
				o["scienceRate"] = picojson::value((double)p.iScienceRate);
				o["production"] = picojson::value((double)p.iProduction);
				players.push_back(picojson::value(o));
			}
		}

		picojson::value::object root;
		root["turn"] = picojson::value((double)iTurn);
		root["gameId"] = picojson::value(pSnap ? pSnap->szGameId : CvString(""));
		root["count"] = picojson::value((double)players.size());
		root["players"] = picojson::value(players);

		CvString szBody(picojson::value(root).serialize().c_str());
		szBody += "\n";
		return szBody;
	}

	// picojson-rendered like /players: the name field is free text.
	CvString renderCities(bool bFilterId, int iId, bool bFilterOwner, int iOwner)
	{
		const bst::shared_ptr<const GameSnapshot> pSnap = grabSnapshot();
		const int iTurn = pSnap ? pSnap->iTurn : -1;

		picojson::value::array cities;
		if (pSnap)
		{
			for (size_t i = 0; i < pSnap->cities.size(); ++i)
			{
				const CitySnap& c = pSnap->cities[i];
				if (bFilterId && c.iID != iId)
				{
					continue;
				}
				if (bFilterOwner && c.iOwner != iOwner)
				{
					continue;
				}
				picojson::value::object o;
				o["id"] = picojson::value((double)c.iID);
				o["owner"] = picojson::value((double)c.iOwner);
				o["x"] = picojson::value((double)c.iX);
				o["y"] = picojson::value((double)c.iY);
				o["name"] = picojson::value(c.szName);
				o["population"] = picojson::value((double)c.iPopulation);
				o["food"] = picojson::value((double)c.iFood);
				o["production"] = picojson::value((double)c.iProduction);
				o["commerce"] = picojson::value((double)c.iCommerce);
				o["producing"] = picojson::value(c.szProducing);
				o["producingTurns"] = picojson::value((double)c.iProducingTurns);
				o["buildings"] = picojson::value((double)c.iNumBuildings);
				o["cultureLevel"] = picojson::value((double)c.iCultureLevel);
				o["capital"] = picojson::value(c.iCapital != 0);
				o["crime"] = picojson::value((double)c.iCrime);
				o["education"] = picojson::value((double)c.iEducation);
				o["disease"] = picojson::value((double)c.iDisease);
				cities.push_back(picojson::value(o));
			}
		}

		picojson::value::object root;
		root["turn"] = picojson::value((double)iTurn);
		root["gameId"] = picojson::value(pSnap ? pSnap->szGameId : CvString(""));
		root["count"] = picojson::value((double)cities.size());
		root["cities"] = picojson::value(cities);

		CvString szBody(picojson::value(root).serialize().c_str());
		szBody += "\n";
		return szBody;
	}

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

	// --- diagnostic gate-eval (#430 readJson testing) -------------------------------------
	// GAME-THREAD ONLY: evaluate one engine gate (+ the cascade equivalent where wired) and render the JSON
	// answer. Called from serviceEvalMailbox -- it reads live CvPlayer state, which the server thread may not.
	// Add the building/unit cap shadow (parsed allowed vs the engine getMax*Instances vs the live tally).
	void rjAddCapShadow(picojson::value::object& o, const CvEntityAvailability& kAvail, CountDomain eDomain,
		int iIdx, const CvCascadeContext& kCtx)
	{
		if (kAvail.allowedCap < 0) return;
		int iLegacyCap = -1; const char* szScope = "?";
		// legacy cap accessor (building has all three; unit exposes global/player)
		if (eDomain == COUNTDOMAIN_BUILDING)
		{
			const CvBuildingInfo& kInfo = GC.getBuildingInfo((BuildingTypes)iIdx);
			switch (kAvail.allowedScope)
			{
			case COUNTSCOPE_WORLD:  iLegacyCap = kInfo.getMaxGlobalInstances(); szScope = "world";  break;
			case COUNTSCOPE_TEAM:   iLegacyCap = kInfo.getMaxTeamInstances();   szScope = "team";   break;
			case COUNTSCOPE_EMPIRE: iLegacyCap = kInfo.getMaxPlayerInstances(); szScope = "empire"; break;
			default: break;
			}
		}
		picojson::value::object cap;
		cap["scope"] = picojson::value(std::string(szScope));
		cap["json"] = picojson::value((double)kAvail.allowedCap);
		cap["legacy"] = picojson::value((double)iLegacyCap);
		cap["tally"] = picojson::value((double)cascadeTally().count(eDomain, iIdx, kAvail.allowedScope, kCtx.contextFor(kAvail.allowedScope)));
		o["cap"] = picojson::value(cap);
	}

	// Diagnostic: WHY does legacy canConstruct block eBuilding in pCity? Returns the FIRST failing legacy gate by
	// CALLING the same legacy accessors canConstruct uses (no logic duplication) -- so cluster diagnosis stops
	// reverse-engineering the failing clause from the data. Order roughly follows CvPlayer/CvCity::canConstructInternal.
	const char* legacyBlockReason(const CvCity* pCity, BuildingTypes eBuilding)
	{
		if (pCity == NULL) return "noCity";
		if (pCity->canConstruct(eBuilding)) return "(buildable)";
		const CvPlayer& kP = GET_PLAYER(pCity->getOwner());
		const CvTeam& kT = GET_TEAM(pCity->getTeam());
		const CvBuildingInfo& kB = GC.getBuildingInfo(eBuilding);
		const SpecialBuildingTypes eSB = kB.getSpecialBuilding();

		if (kB.getExtendsBuilding() > NO_BUILDING && !pCity->hasBuilding(kB.getExtendsBuilding())) return "extends";
		if (kB.getPrereqAndTech() != NO_TECH && !kT.isHasTech((TechTypes)kB.getPrereqAndTech())) return "tech";
		foreach_(const TechTypes eT, kB.getPrereqAndTechs()) { if (!kT.isHasTech(eT)) return "techAnd"; }
		if (eSB != NO_SPECIALBUILDING)
		{
			const TechTypes eRT = GC.getSpecialBuildingInfo(eSB).getTechPrereq();
			if (eRT != NO_TECH && !kT.isHasTech(eRT)) return "specialBuildingTech";
		}
		if (kT.isObsoleteBuilding(eBuilding)) return "obsolete";
		if (kP.isBuildingMaxedOut(eBuilding) || kT.isBuildingMaxedOut(eBuilding) || GC.getGame().isBuildingMaxedOut(eBuilding)) return "cap";
		if (eSB != NO_SPECIALBUILDING && kP.isBuildingGroupMaxedOut(eSB)) return "groupCap";
		if (!kP.hasValidCivics(eBuilding)) return "civics";
		if (kB.isPrereqWar() && !kT.isAtWar()) return "war";
		if (kB.getProductionCost() == -1) return "notConstructible";
		{
			const ReligionTypes ePSR = (ReligionTypes)kB.getPrereqStateReligion();
			if (ePSR != NO_RELIGION && ePSR != kP.getStateReligion()) return "prereqStateReligion";
		}
		if (pCity->hasBuilding(eBuilding)) return "alreadyBuilt";
		if (pCity->isDisabledBuilding(eBuilding)) return "disabled";
		if (kB.needStateReligionInCity())
		{
			const ReligionTypes eSR = kP.getStateReligion();
			if (eSR == NO_RELIGION || !pCity->isHasReligion(eSR)) return "stateReligionInCity";
		}
		{
			const ReligionTypes ePR = (ReligionTypes)kB.getPrereqReligion();
			if (ePR != NO_RELIGION && !pCity->isHasReligion(ePR)) return "prereqReligion";
		}
		{
			const CorporationTypes ePC = (CorporationTypes)kB.getPrereqCorporation();
			if (ePC != NO_CORPORATION && !pCity->isHasCorporation(ePC)) return "prereqCorp";
		}
		if (!pCity->isValidBuildingLocation(eBuilding)) return "location";
		for (int i = 0; i < kB.getNumPrereqInCityBuildings(); i++)
		{
			const BuildingTypes eP = (BuildingTypes)kB.getPrereqInCityBuilding(i);
			if (eP != NO_BUILDING && !kT.isObsoleteBuilding(eP) && !pCity->isActiveBuilding(eP)) return "prereqInCity";
		}
		// --- expanded gates (whittling down the "other" bucket; order approximate, not canConstruct-exact) ---
		if (!GC.getGame().canEverConstruct(eBuilding)) return "canEverConstruct";
		if (GC.getGame().countCivTeamsEverAlive() < kB.getNumTeamsPrereq()) return "teamsPrereq";
		if (kB.getMaxStartEra() != NO_ERA && GC.getGame().getStartEra() > kB.getMaxStartEra()) return "maxStartEra";
		if (kB.getVictoryPrereq() != NO_VICTORY
		&& (kP.isMinorCiv() || !GC.getGame().isVictoryValid((VictoryTypes)kB.getVictoryPrereq())
		    || kT.getVictoryCountdown((VictoryTypes)kB.getVictoryPrereq()) >= 0)) return "victory";
		if (kB.getFoundsCorporation() != NO_CORPORATION
		&& GC.getGame().isCorporationFounded((CorporationTypes)kB.getFoundsCorporation())) return "foundsCorp";
		if (kB.isNoHolyCity() && pCity->isHolyCity()) return "noHolyCity";
		{
			bool bReqH = false, bValidH = false;
			foreach_(const HeritageTypes eH, kB.getPrereqOrHeritage())
			{ bReqH = true; if (kP.hasHeritage(eH)) { bValidH = true; break; } }
			if (bReqH && !bValidH) return "heritage";
		}
		if (!kB.getPrereqNumOfBuildings().empty())
		{
			for (int iI = 0; iI < GC.getNumBuildingInfos(); iI++)
			{
				const BuildingTypes eX = (BuildingTypes)iI;
				if (!kT.isObsoleteBuilding(eX) && kP.getBuildingCount(eX) < kP.getBuildingPrereqBuilding(eBuilding, eX, 0))
					return "prereqNumBuildings";
			}
		}
		// --- city-side gates (CvCity::canConstructInternal 2664-2792) ---
		if (pCity->getFirstBuildingOrder(eBuilding) != -1) return "alreadyQueued";
		if (isLimitedWonder(eBuilding) && !kB.isNoLimit()
		&& ((isWorldWonder(eBuilding)    && pCity->isWorldWondersMaxed())
		||  (isTeamWonder(eBuilding)     && pCity->isTeamWondersMaxed())
		||  (isNationalWonder(eBuilding) && pCity->isNationalWondersMaxed()))) return "wonderCategoryMaxed";
		if (kB.getHolyCity() != NO_RELIGION && !pCity->isHolyCity((ReligionTypes)kB.getHolyCity())) return "holyCity";
		if (kB.getPrereqAndBonus() != NO_BONUS && !pCity->hasBonus((BonusTypes)kB.getPrereqAndBonus())) return "bonus";
		if (!(*pCity->getPropertiesConst() <= *(kB.getPrereqMaxProperties()))
		||  !(*pCity->getPropertiesConst() >= *(kB.getPrereqMinProperties()))) return "propertyThreshold";
		if (pCity->plot()->getLatitude() > kB.getMaxLatitude() || pCity->plot()->getLatitude() < kB.getMinLatitude()) return "latitude";
		{
			const int iReqPop = std::max(kB.getPrereqPopulation(), 1 + pCity->getNumPopulationEmployed() + kB.getNumPopulationEmployed());
			if (iReqPop > 1 && pCity->getPopulation() < iReqPop) return "population";
		}
		if (kB.getPrereqCultureLevel() != NO_CULTURELEVEL && pCity->getCultureLevel() < kB.getPrereqCultureLevel()) return "cultureLevel";
		{
			const BuildingTypes eAny = kB.getPrereqAnyoneBuilding();
			if (eAny != NO_BUILDING && GC.getGame().getBuildingCreatedCount(eAny) == 0) return "prereqAnyone";
		}
		{
			bool bReqOB = false, bHasOB = false;
			foreach_(const BonusTypes eB, kB.getPrereqOrBonuses()) { bReqOB = true; if (pCity->hasBonus(eB)) { bHasOB = true; break; } }
			if (bReqOB && !bHasOB) return "prereqOrBonus";
		}
		if (kB.getPrereqVicinityBonus() != NO_BONUS && !pCity->hasVicinityBonus((BonusTypes)kB.getPrereqVicinityBonus())) return "vicinityBonus";
		if (kB.getPrereqRawVicinityBonus() != NO_BONUS && !pCity->hasRawVicinityBonus((BonusTypes)kB.getPrereqRawVicinityBonus())) return "rawVicinityBonus";
		{
			bool bReqOV = false, bHasOV = false;
			foreach_(const BonusTypes eB, kB.getPrereqOrVicinityBonuses()) { bReqOV = true; if (pCity->hasVicinityBonus(eB)) { bHasOV = true; break; } }
			if (bReqOV && !bHasOV) return "prereqOrVicinityBonus";
		}
		{
			bool bReqORV = false, bHasORV = false;
			foreach_(const BonusTypes eB, kB.getPrereqOrRawVicinityBonuses()) { bReqORV = true; if (pCity->hasRawVicinityBonus(eB)) { bHasORV = true; break; } }
			if (bReqORV && !bHasORV) return "prereqOrRawVicinityBonus";
		}
		for (int iI = 0; iI < kB.getNumPrereqNotInCityBuildings(); ++iI)
			if (pCity->hasBuilding((BuildingTypes)kB.getPrereqNotInCityBuilding(iI))) return "prereqNotInCity";
		{
			bool bReqB = false, bValidB = false;
			for (int iI = 0; iI < kB.getNumPrereqOrBuilding(); ++iI)
			{
				const BuildingTypes eP = (BuildingTypes)kB.getPrereqOrBuilding(iI);
				if (!kT.isObsoleteBuilding(eP)) { bReqB = true; if (pCity->isActiveBuilding(eP)) { bValidB = true; break; } }
			}
			if (bReqB && !bValidB) return "prereqOrBuildings";
		}
		for (int iI = 0; iI < kB.getNumReplacementBuilding(); ++iI)
		{
			const BuildingTypes eRepl = (BuildingTypes)kB.getReplacementBuilding(iI);
			if (pCity->isActiveBuilding(eRepl)
			|| (kP.isModderOption(MODDEROPTION_HIDE_REPLACED_BUILDINGS) && pCity->canConstruct(eRepl, true, false, false, true)))
				return "replaced";
		}
		return "other";
	}

	// Symmetric diagnostic: WHY does the CASCADE block eBuilding (the under-offer side -- legacy allows, cascade hides)?
	// Mirrors the sweep's cascade verdict, broken into its clauses. On-demand only; holds no state.
	const char* cascadeBlockReason(const CvEntityAvailability& kA, BuildingTypes iIdx, const CvCity* pCity, int iTeam, const CvCascadeContext& kCtx)
	{
		if (kA.notConstructible) return "notConstructible";
		if (pCity != NULL && pCity->hasBuilding(iIdx)) return "alreadyBuilt";
		if (cascadeIsObsoleteForTeam(COUNTDOMAIN_BUILDING, iIdx, iTeam)) return "obsolete";
		if (cascadeIsReplacedInCity(iIdx, kCtx)) return "replaced";
		if (!cascadeBuildingGroupAllows(iIdx, kCtx)) return "groupCap";
		if (!cascadeEvalCondition(kA.requiresBuild, kCtx)) return "requiresBuild";
		if (!cascadeEvalCondition(kA.requiresOperate, kCtx)) return "requiresOperate";
		if (!cascadeWithinAllowed(COUNTDOMAIN_BUILDING, iIdx, kA.allowedScope, kA.allowedCap, kCtx)) return "allowedCap";
		return "(buildable)";
	}

	// ---- movement-cost decomposition (the /diagnostic/movementSweep "observe" surface) ----------------------
	// A FAITHFUL re-decomposition of CvPlot::movementCost (Sources/Engine/CvPlot.cpp::movementCost, verified
	// 2026-06-20) that records every named component, so a future converter divergence is attributed to a SOURCE
	// WITH NUMBERS rather than guessed (DEC-no-guessing). It is a MIRROR, not the authority: the sweep cross-checks
	// `iFinal` against the engine's own movementCost() per edge and flags any drift -- the mirror exists only to
	// explain the engine's number, never to replace it. Reproduces every branch incl. the route min-override
	// (routeCost vs routeFlatCost), the additive terrain stack, the unit -cost discount, the /2|/4 double-move, and
	// the hard floor of 90 (NOT the denominator) -- the facts modifier.md 6.6 under-specified.
	struct MoveCostParts
	{
		int  iDenominator;     // GC.getMOVE_DENOMINATOR()
		bool bEarlyFlat;       // flatMovementCost() || DOMAIN_AIR -> denominator (the one true early return)
		bool bRouteBranch;     // both plots routed & (no river-cross OR bridge-building tech)
		int  iRouteCost;       // route.getMovementCost() + team.getRouteChange() (max of from/to routes)
		int  iRouteFlatCost;   // max(fromFlat,toFlat) * baseMoves -- the second min() term
		bool bIgnoreTerrain;   // ignoreTerrainCost(), or reduced-to-<=1 by the discount
		int  iTerrain, iFeature, iHills, iRiver, iPeak; // additive regular-branch adders (pre-denominator)
		int  iDiscount;        // getExtraMoveDiscount()
		int  iRegularPreDenom; // max(1, sum - discount), before the * denominator
		int  iDoubleDiv;       // 1 | 2 | 4 (terrain/feature/hills double-move divisor)
		int  iFinal;           // the recomposed final cost (floor 90 on the regular branch, then max(1))
	};

	void decomposeMoveCost(const CvPlot* pTo, const CvUnit* pUnit, const CvPlot* pFrom, MoveCostParts& r)
	{
		const int iDenom = GC.getMOVE_DENOMINATOR();
		r.iDenominator = iDenom;
		r.bEarlyFlat = false; r.bRouteBranch = false; r.bIgnoreTerrain = false;
		r.iRouteCost = 0; r.iRouteFlatCost = 0;
		r.iTerrain = 0; r.iFeature = 0; r.iHills = 0; r.iRiver = 0; r.iPeak = 0;
		r.iDiscount = pUnit->getExtraMoveDiscount();
		r.iRegularPreDenom = 0; r.iDoubleDiv = 1; r.iFinal = 0;

		// 1) flat-cost / air units: the ONLY branch that returns without the trailing max(1).
		if (pUnit->flatMovementCost() || pUnit->getDomainType() == DOMAIN_AIR)
		{
			r.bEarlyFlat = true; r.iFinal = iDenom; return;
		}
		// 2) human stepping into unrevealed, or invalid-domain-for-location: maxMoves(). 3) invalid-for-action: denom.
		if (pUnit->isHuman() && !pTo->isRevealed(pUnit->getTeam(), false)) { r.iFinal = std::max(1, pUnit->maxMoves()); return; }
		if (!pFrom->isValidDomainForLocation(*pUnit))                      { r.iFinal = std::max(1, pUnit->maxMoves()); return; }
		if (!pTo->isValidDomainForAction(*pUnit))                          { r.iFinal = std::max(1, iDenom); return; }

		const bool bRiverCross = pFrom->isRiverCrossing(directionXY(pFrom, pTo));

		// 4) ROUTE OVERRIDE branch: cost = min(denom, min(routeCost, routeFlatCost)).
		if (pFrom->isValidRoute(pUnit) && pTo->isValidRoute(pUnit)
			&& (!bRiverCross || GET_TEAM(pUnit->getTeam()).isBridgeBuilding()))
		{
			r.bRouteBranch = true;
			const RouteTypes eFrom = pFrom->getRouteType();
			const RouteTypes eTo = pTo->getRouteType();
			const CvRouteInfo& kFrom = GC.getRouteInfo(eFrom);
			const CvRouteInfo& kTo = GC.getRouteInfo(eTo);
			int iRoute = kFrom.getMovementCost() + GET_TEAM(pUnit->getTeam()).getRouteChange(eFrom);
			if (eTo != eFrom)
			{
				const int iToCost = kTo.getMovementCost() + GET_TEAM(pUnit->getTeam()).getRouteChange(eTo);
				if (iToCost > iRoute) iRoute = iToCost;
			}
			r.iRouteCost = iRoute;
			r.iRouteFlatCost = std::max(kFrom.getFlatMovementCost(), kTo.getFlatMovementCost()) * pUnit->baseMoves();
			r.iFinal = std::max(1, std::min(iDenom, std::min(r.iRouteCost, r.iRouteFlatCost)));
			return;
		}

		// 5) REGULAR (terrain) branch: additive stack, -discount, *denom, double-move /2|/4, floor 90.
		int iRegular;
		bool bIgnore = pUnit->ignoreTerrainCost();
		if (bIgnore)
		{
			iRegular = 1;
		}
		else
		{
			r.iTerrain = GC.getTerrainInfo(pTo->getTerrainType()).getMovementCost();
			iRegular = r.iTerrain;
			if (pTo->getFeatureType() != NO_FEATURE)
			{
				r.iFeature = GC.getFeatureInfo(pTo->getFeatureType()).getMovementCost();
				iRegular += r.iFeature;
			}
			if (pTo->isHills()) { r.iHills = GC.getHILLS_EXTRA_MOVEMENT(); iRegular += r.iHills; }
			if (bRiverCross)    { r.iRiver = GC.getRIVER_EXTRA_MOVEMENT(); iRegular += r.iRiver; }
			if (pTo->isAsPeak())
			{
				if (!GET_TEAM(pUnit->getTeam()).isMoveFastPeaks()) { r.iPeak = GC.getPEAK_EXTRA_MOVEMENT(); iRegular += r.iPeak; }
				r.iPeak += 3; iRegular += 3; // the literal "+3" the engine adds for peaks unconditionally
			}
		}
		if (iRegular > 0) iRegular = std::max(1, iRegular - r.iDiscount);
		if (iRegular <= 1) bIgnore = true; // discount drove it to the flat case
		r.bIgnoreTerrain = bIgnore;
		r.iRegularPreDenom = iRegular;
		iRegular *= iDenom;
		const bool bFeatDouble = ((pTo->getFeatureType() != NO_FEATURE && pUnit->isFeatureDoubleMove(pTo->getFeatureType()))
			|| (pTo->isHills() && pUnit->isHillsDoubleMove()));
		const bool bTerrDouble = pUnit->isTerrainDoubleMove(pTo->getTerrainType());
		if (!bIgnore && bFeatDouble)                  { iRegular /= 4; r.iDoubleDiv = 4; }
		else if (bTerrDouble || (bIgnore && bFeatDouble)) { iRegular /= 2; r.iDoubleDiv = 2; }
		r.iFinal = std::max(1, std::max(90, iRegular));
	}

	// ====================================================================================================
	// /extractor -- the RAW game-state dump (world -> teams -> empires -> areas -> cities -> plots).
	// CLEAN, raw-FACTS-ONLY extraction: NO calculated value ever appears here (DEC-calc-zero-ride-in). The
	// only map-derived number is distanceFromCapital. This is the dedicated extraction surface -- read it
	// directly, feed it to Tools/ModifierCalc/dry_calc.py, or build features on it. Spec:
	// Tools/ModifierCalc/README.md. Runs on the game thread (mailbox), where every fact is readable.
	// ====================================================================================================
	picojson::value::object extractCity(CvPlayer& kPlayer, CvCity* pCity, int iTeam)
	{
		picojson::value::object c;
		c["id"]          = picojson::value((double)pCity->getID());
		c["name"]        = picojson::value(std::string(narrowToAscii(pCity->getName()).GetCString()));
		c["population"]  = picojson::value((double)pCity->getPopulation());
		c["isCapital"]   = picojson::value(pCity->isCapital());
		c["isPowered"]   = picojson::value(pCity->isPower());
		c["isGoldenAge"] = picojson::value(kPlayer.isGoldenAge());           // golden age is a player-level boolean
		const CvCity* pCap = kPlayer.getCapitalCity();
		c["distanceFromCapital"] = picojson::value((double)(pCap != NULL ?
			plotDistance(pCity->getX(), pCity->getY(), pCap->getX(), pCap->getY()) : 0)); // the lone map-number
		c["x"] = picojson::value((double)pCity->getX());                   // map coords -- for plotDistance (trade)
		c["y"] = picojson::value((double)pCity->getY());
		c["connectedToCapital"] = picojson::value(pCity->isConnectedToCapital()); // CAPITAL_TRADE_MODIFIER gate (raw fact)

		// TRADE ROUTES -- the city's PICKED partners (m_paTradeCities), a per-city stored DECISION, exactly
		// like worked-plots: we EXTRACT the decision and let the calc compute the profit/yield from raw inputs
		// (partner pop/coords + deposit-modeled trade modifiers + the area/team/peace facts). We do NOT re-derive
		// the selection (greedy best-N + plot-group connectivity + diplo) -- observing the result makes all that
		// machinery irrelevant. Partner ref = (owner, id) since city ids are per-player.
		picojson::value::array routes;
		for (int ti = 0; ti < pCity->getMaxTradeRoutes(); ++ti)
		{
			const CvCity* pPartner = pCity->getTradeCity(ti);
			if (pPartner == NULL) continue;
			picojson::value::object rt;
			rt["owner"] = picojson::value((double)pPartner->getOwner());
			rt["id"]    = picojson::value((double)pPartner->getID());
			rt["distance"] = picojson::value((double)plotDistance(pCity->getX(), pCity->getY(),
				pPartner->getX(), pPartner->getY())); // map geometry (wrap-correct) -- the getBaseTradeProfit input
			routes.push_back(picojson::value(rt));
		}
		c["tradeRoutePartners"] = picojson::value(routes);

		// buildings PRESENT (hasBuilding) + the DISABLED subset = the exact gate getBuildingCommerceByBuilding uses
		// to yield 0 (CvCity.cpp:12148 !isActiveBuilding  OR  12157 isReligiouslyLimitedBuilding). NB this is LOOSER
		// than hasFullyActiveBuilding -- a resource-disabled building (e.g. a library missing its bonus) is NOT
		// isActiveBuilding-disabled and STILL yields commerce, so it must stay IN. The value-calc consumes this
		// engine ACTIVE set as state (like hasBuilding itself); the pure dormancy compute is the enabler's shadow.
		picojson::value::array bldgs, dormant;
		for (int b = 0; b < GC.getNumBuildingInfos(); ++b)
			if (pCity->hasBuilding((BuildingTypes)b))
			{
				const char* szTb = GC.getBuildingInfo((BuildingTypes)b).getType();
				bldgs.push_back(picojson::value(std::string(szTb)));
				if (!pCity->isActiveBuilding((BuildingTypes)b) || pCity->isReligiouslyLimitedBuilding((BuildingTypes)b))
					dormant.push_back(picojson::value(std::string(szTb)));
			}
		c["buildings"] = picojson::value(bldgs);
		c["dormantBuildings"] = picojson::value(dormant);

		// per-building AGE in game-years (getGameTurnYear - iTimeBuilt) -- the `existedFor` predicate input
		// (CommerceChangeDoubleTimes -> 2nd age-gated deposit, CvCity.cpp:12207-12213). Legacy doubles only when
		// iTimeBuilt != MIN_INT; emit age for those buildings (the calc compares age >= existedFor.min).
		{
			picojson::value::object bldgAges;
			const int iYearNow = GC.getGame().getGameTurnYear();
			for (int b = 0; b < GC.getNumBuildingInfos(); ++b)
				if (pCity->hasBuilding((BuildingTypes)b))
				{
					const int iTB = pCity->getBuildingData((BuildingTypes)b).iTimeBuilt;
					if (iTB != MIN_INT)
						bldgAges[GC.getBuildingInfo((BuildingTypes)b).getType()] = picojson::value((double)(iYearNow - iTB));
				}
			c["buildingAges"] = picojson::value(bldgAges);
		}

		// specialist ASSIGNMENT counts (manual + free)
		picojson::value::object specs;
		for (int s = 0; s < GC.getNumSpecialistInfos(); ++s)
		{
			const int n = pCity->getSpecialistCount((SpecialistTypes)s) + pCity->getFreeSpecialistCount((SpecialistTypes)s);
			if (n > 0) specs[GC.getSpecialistInfo((SpecialistTypes)s).getType()] = picojson::value((double)n);
		}
		c["specialists"] = picojson::value(specs);

		// available bonuses = TRADE-connected/reachable (hasBonus) -- raw fact
		picojson::value::array bonuses;
		for (int b = 0; b < GC.getNumBonusInfos(); ++b)
			if (pCity->hasBonus((BonusTypes)b))
				bonuses.push_back(picojson::value(std::string(GC.getBonusInfo((BonusTypes)b).getType())));
		c["bonuses"] = picojson::value(bonuses);

		// VICINITY bonuses (hasVicinityBonus) -- physically in the city's workable radius, DISTINCT from the
		// trade-connected `bonuses` (hasBonus). The cascade's connection:vicinity atoms read THIS set
		// (CvCascadeCondition.cpp:114 CONN_VICINITY -> hasVicinityBonus); without it the calc cannot tell a
		// vicinity bonus from a trade-connected one (the gatherer requires.operate bonus gate -- the 964-miss bug).
		picojson::value::array vicinityBonuses;
		for (int b = 0; b < GC.getNumBonusInfos(); ++b)
			if (pCity->hasVicinityBonus((BonusTypes)b))
				vicinityBonuses.push_back(picojson::value(std::string(GC.getBonusInfo((BonusTypes)b).getType())));
		c["vicinityBonuses"] = picojson::value(vicinityBonuses);

		// bonus COUNTS (getNumBonuses) -- the corp-output scaler (CommercesProduced x Sum getNumBonuses(prereqBonus))
		picojson::value::object bonusCounts;
		for (int b = 0; b < GC.getNumBonusInfos(); ++b)
		{
			const int n = pCity->getNumBonuses((BonusTypes)b);
			if (n > 0) bonusCounts[GC.getBonusInfo((BonusTypes)b).getType()] = picojson::value((double)n);
		}
		c["bonusCounts"] = picojson::value(bonusCounts);

		// /extractor city.cultureLevel: culture level TYPE -- drives the per-city wonder-category cap allowance
		// (worldWonders/teamWonders/nationalWonders authored on the CultureLevel entity, data-model.md S3.4).
		if (pCity->getCultureLevel() >= 0)
			c["cultureLevel"] = picojson::value(std::string(GC.getCultureLevelInfo(pCity->getCultureLevel()).getType()));

		// /extractor city.queuedBuildings / queuedUnits: the production order queue -- the engine drops a queued
		// item from canConstruct/canTrain (build-queue exclusion), so the cascade must subtract these.
		picojson::value::array queuedB, queuedU;
		for (int iQ = 0; iQ < pCity->getOrderQueueLength(); ++iQ)
		{
			const OrderData od = pCity->getOrderAt(iQ);
			if (od.getOrderType() == ORDER_CONSTRUCT)
				queuedB.push_back(picojson::value(std::string(GC.getBuildingInfo(od.getBuildingType()).getType())));
			else if (od.getOrderType() == ORDER_TRAIN)
				queuedU.push_back(picojson::value(std::string(GC.getUnitInfo(od.getUnitType()).getType())));
		}
		c["queuedBuildings"] = picojson::value(queuedB);
		c["queuedUnits"] = picojson::value(queuedU);

		// /extractor city.canConstruct / canTrain: the engine's per-city buildability verdict -- the COMPARISON
		// oracle for the cascade's isolated per-city buildable frontier (the TRUE set only).
		picojson::value::array canCon;
		for (int iB = 0; iB < GC.getNumBuildingInfos(); ++iB)
			if (pCity->canConstruct((BuildingTypes)iB))
				canCon.push_back(picojson::value(std::string(GC.getBuildingInfo((BuildingTypes)iB).getType())));
		c["canConstruct"] = picojson::value(canCon);
		picojson::value::array canTrn;
		for (int iU = 0; iU < GC.getNumUnitInfos(); ++iU)
			if (pCity->canTrain((UnitTypes)iU))
				canTrn.push_back(picojson::value(std::string(GC.getUnitInfo((UnitTypes)iU).getType())));
		c["canTrain"] = picojson::value(canTrn);

		// ACTIVE corporations in the city (isActiveCorporation) -- per-city corp output/commerce applies only where active
		picojson::value::array corps;
		for (int cp = 0; cp < GC.getNumCorporationInfos(); ++cp)
			if (pCity->isActiveCorporation((CorporationTypes)cp))
				corps.push_back(picojson::value(std::string(GC.getCorporationInfo((CorporationTypes)cp).getType())));
		c["corporations"] = picojson::value(corps);

		// PRESENT corporations (isHasCorporation) -- a branch office can exist but be INACTIVE (e.g. lost its prereq
		// bonus). The building corp-PREREQ dormancy (applyCorporationModifiers off setHasCorporation, CvCity.cpp:15193/
		// 15226) gates on PRESENT, not active; so the enabler/active-set must read this set, NOT `corporations`.
		picojson::value::array pcorps;
		for (int cp = 0; cp < GC.getNumCorporationInfos(); ++cp)
			if (pCity->isHasCorporation((CorporationTypes)cp))
				pcorps.push_back(picojson::value(std::string(GC.getCorporationInfo((CorporationTypes)cp).getType())));
		c["presentCorporations"] = picojson::value(pcorps);

		// religions present in the city
		picojson::value::array rels;
		for (int r = 0; r < GC.getNumReligionInfos(); ++r)
			if (pCity->isHasReligion((ReligionTypes)r))
				rels.push_back(picojson::value(std::string(GC.getReligionInfo((ReligionTypes)r).getType())));
		c["religions"] = picojson::value(rels);
		// religions whose HOLY CITY is this city (gates HOLY_CITY religion deposits -- a real per-city fact,
		// not "religion present"; the calc reads religion-entity deposits gated by HOLY_CITY/STATE_RELIGION).
		picojson::value::array holy;
		for (int r = 0; r < GC.getNumReligionInfos(); ++r)
			if (pCity->isHolyCity((ReligionTypes)r))
				holy.push_back(picojson::value(std::string(GC.getReligionInfo((ReligionTypes)r).getType())));
		c["holyCity"] = picojson::value(holy);

		// property CURRENT VALUES (crime/education/disease/pollution/...) -- the value, not its rate
		picojson::value::object props;
		const CvProperties* pProps = pCity->getPropertiesConst();
		if (pProps != NULL)
			for (int pp = 0; pp < pProps->getNumProperties(); ++pp)
			{
				const PropertyTypes eP = pProps->getProperty(pp);
				props[GC.getPropertyInfo(eP).getType()] = picojson::value((double)pProps->getValueByProperty(eP));
			}
		c["properties"] = picojson::value(props);

		// plots -- the worked-tile substrate (raw contents + worked flag; no yields)
		picojson::value::array plots;
		for (int pi = 0; pi < pCity->getNumCityPlots(); ++pi)
		{
			const CvPlot* pPlot = pCity->getCityIndexPlot(pi);
			if (pPlot == NULL) continue;
			picojson::value::object pl;
			pl["worked"] = picojson::value(pCity->isWorkingPlot(pi));
			const TerrainTypes eT = pPlot->getTerrainType();
			if (eT != NO_TERRAIN) pl["terrain"] = picojson::value(std::string(GC.getTerrainInfo(eT).getType()));
			const FeatureTypes eF = pPlot->getFeatureType();
			if (eF != NO_FEATURE) pl["feature"] = picojson::value(std::string(GC.getFeatureInfo(eF).getType()));
			const ImprovementTypes eI = pPlot->getImprovementType();
			if (eI != NO_IMPROVEMENT) pl["improvement"] = picojson::value(std::string(GC.getImprovementInfo(eI).getType()));
			const RouteTypes eR = pPlot->getRouteType();
			if (eR != NO_ROUTE) pl["route"] = picojson::value(std::string(GC.getRouteInfo(eR).getType()));
			const BonusTypes eB = pPlot->getBonusType((TeamTypes)iTeam);
			if (eB != NO_BONUS) pl["bonus"] = picojson::value(std::string(GC.getBonusInfo(eB).getType()));
			if (pPlot->isRiver())               pl["river"] = picojson::value(true);
			if (pPlot->isIrrigationAvailable()) pl["irrig"] = picojson::value(true);
			if (pPlot->isHills())               pl["hills"] = picojson::value(true);
			if (pPlot->isPeak() || pPlot->isAsPeak()) pl["peak"] = picojson::value(true);
			if (pPlot->isWater())               pl["water"] = picojson::value(true);
			if (pPlot->isCoastalLand())         pl["coast"] = picojson::value(true);  // coastal land (HAS_COAST predicate)
			if (pPlot->isCity())                pl["isCity"] = picojson::value(true);  // city-center plot (gets getCityChange)
			// per-plot stored EXTRA yield (game event/effect state; a calculateYield addend not derivable from JSON)
			const int exF = pPlot->getExtraYield(YIELD_FOOD), exP = pPlot->getExtraYield(YIELD_PRODUCTION), exC = pPlot->getExtraYield(YIELD_COMMERCE);
			if (exF || exP || exC)
			{
				picojson::value::object ex;
				if (exF) ex["food"] = picojson::value((double)exF);
				if (exP) ex["production"] = picojson::value((double)exP);
				if (exC) ex["commerce"] = picojson::value((double)exC);
				pl["extraYield"] = picojson::value(ex);
			}
			plots.push_back(picojson::value(pl));
		}
		c["plots"] = picojson::value(plots);
		return c;
	}

	// iPlayerFilter < 0 == ALL players; otherwise restrict to that one player (smaller dump).
	CvString extractGameState(int iPlayerFilter)
	{
		CvGame& kGame = GC.getGame();
		picojson::value::object world;

		// world.religionLevels -- game-wide religion-level counts (the shrine WORLD-scope scaler)
		picojson::value::object relLevels;
		for (int r = 0; r < GC.getNumReligionInfos(); ++r)
		{
			const int n = kGame.countReligionLevels((ReligionTypes)r);
			if (n > 0) relLevels[GC.getReligionInfo((ReligionTypes)r).getType()] = picojson::value((double)n);
		}
		world["religionLevels"] = picojson::value(relLevels);

		// /extractor world.unitCreatedCounts: getUnitCreatedCount per unit type -- the lifetime-created HISTORICAL
		// (increment-only, never decremented) the unit world-cap reads (tally.md S3.3). World scope.
		picojson::value::object unitCreated;
		for (int iU = 0; iU < GC.getNumUnitInfos(); ++iU)
		{
			const int n = GC.getGame().getUnitCreatedCount((UnitTypes)iU);
			if (n > 0) unitCreated[GC.getUnitInfo((UnitTypes)iU).getType()] = picojson::value((double)n);
		}
		world["unitCreatedCounts"] = picojson::value(unitCreated);

		// world.corporationLevels -- game-wide corporation-level counts (the HQ WORLD-scope scaler):
		// corp HQ commerce = corp.<c>.empire.headquarters.perCorporationLevel x countCorporationLevels (CvCity.cpp:12200-12205).
		picojson::value::object corpLevels;
		for (int cp = 0; cp < GC.getNumCorporationInfos(); ++cp)
		{
			const int n = kGame.countCorporationLevels((CorporationTypes)cp);
			if (n > 0) corpLevels[GC.getCorporationInfo((CorporationTypes)cp).getType()] = picojson::value((double)n);
		}
		world["corporationLevels"] = picojson::value(corpLevels);
		// world CorporationMaintenancePercent -- the corp-OUTPUT scaler (CommercesProduced x bonusCount x maintPct/100,
		// CvCity.cpp:12625, the WORLD one -- NOT the handicap maint% used by the corp-MAINTENANCE calc).
		world["corporationMaintenancePercent"] = picojson::value((double)GC.getWorldInfo(GC.getMap().getWorldSize()).getCorporationMaintenancePercent());

		// world teamsEverAlive -- countCivTeamsEverAlive(), the exact source of the TEAM count atom (CvBuildingInfo
		// getNumTeamsPrereq gate, CvPlayer.cpp:6688). Emitted explicitly so the offline calc reads the ever-alive
		// count, NOT an alive-only approximation (dead teams still count toward the prereq).
		world["teamsEverAlive"] = picojson::value((double)GC.getGame().countCivTeamsEverAlive());

		// world.config -- RAW game-define scalars the calc needs that are NOT entity data. Resolved game-side
		// (authoritative; e.g. the world-size trade-profit % needs no world-size guess offline). The TRADE block
		// feeds the trade-route profit/yield port (calc-map 9.5): the profit defines + the per-yield YieldInfo
		// trade-modifier BASE that seeds getTradeYieldModifier (CvPlayer.cpp:397). The deposit-modeled trade
		// modifiers (building/civic/trait/tech) are NOT here -- the cascade computes those from Assets/Data.
		picojson::value::object config;
		config["theirPopulationTradePercent"]   = picojson::value((double)GC.getTHEIR_POPULATION_TRADE_PERCENT());
		config["tradeProfitPercent"]            = picojson::value((double)GC.getTRADE_PROFIT_PERCENT());
		config["worldTradeProfitPercent"]       = picojson::value((double)GC.getWorldInfo(GC.getMap().getWorldSize()).getTradeProfitPercent());
		config["capitalTradeModifier"]          = picojson::value((double)GC.getCAPITAL_TRADE_MODIFIER());
		config["overseasTradeModifier"]         = picojson::value((double)GC.getOVERSEAS_TRADE_MODIFIER());
		config["foreignTradeModifier"]          = picojson::value((double)GC.getFOREIGN_TRADE_MODIFIER());
		config["foreignTradeFullCreditPeaceTurns"] = picojson::value((double)GC.getFOREIGN_TRADE_FULL_CREDIT_PEACE_TURNS());
		config["ourPopulationTradeModifier"]    = picojson::value((double)GC.getOUR_POPULATION_TRADE_MODIFIER());
		config["ourPopulationTradeModifierOffset"] = picojson::value((double)GC.getOUR_POPULATION_TRADE_MODIFIER_OFFSET());
		picojson::value::object yieldTradeBase;
		for (int y = 0; y < NUM_YIELD_TYPES; ++y)
			yieldTradeBase[GC.getYieldInfo((YieldTypes)y).getType()] = picojson::value((double)GC.getYieldInfo((YieldTypes)y).getTradeModifier());
		config["yieldTradeModifierBase"] = picojson::value(yieldTradeBase);
		// city-CENTER plot yield: getCityChange (flat) + population/PopulationChangeDivisor (CvPlot::calculateYield)
		picojson::value::object yCityChange, yPopDiv, yGAYield, yGAThresh;
		for (int y = 0; y < NUM_YIELD_TYPES; ++y)
		{
			const char* yn = GC.getYieldInfo((YieldTypes)y).getType();
			yCityChange[yn] = picojson::value((double)GC.getYieldInfo((YieldTypes)y).getCityChange());
			yPopDiv[yn]     = picojson::value((double)GC.getYieldInfo((YieldTypes)y).getPopulationChangeDivisor());
			// per-plot GOLDEN-AGE yield: +getGoldenAgeYield on each worked plot whose yield >= threshold (calculateYield)
			yGAYield[yn]    = picojson::value((double)GC.getYieldInfo((YieldTypes)y).getGoldenAgeYield());
			yGAThresh[yn]   = picojson::value((double)GC.getYieldInfo((YieldTypes)y).getGoldenAgeYieldThreshold());
		}
		config["yieldCityChange"] = picojson::value(yCityChange);
		config["yieldPopulationChangeDivisor"] = picojson::value(yPopDiv);
		config["yieldGoldenAgeYield"] = picojson::value(yGAYield);
		config["yieldGoldenAgeThreshold"] = picojson::value(yGAThresh);
		config["elapsedGameTurns"] = picojson::value((double)kGame.getElapsedGameTurns()); // getPeaceTradeModifier clause
		world["config"] = picojson::value(config);

		// world.options -- the ACTIVE game options (world-scope state). Option-gated behaviour (complex/pure
		// traits, no-espionage, etc.) changes which deposits/definitions apply, so the calc must know them; e.g.
		// GAMEOPTION_LEADER_COMPLEX_TRAITS selects the complex/pure trait definitions over the simple ones.
		picojson::value::array options;
		for (int oi = 0; oi < GC.getNumGameOptionInfos(); ++oi)
			if (kGame.isOption((GameOptionTypes)oi))
				options.push_back(picojson::value(std::string(GC.getGameOptionInfo((GameOptionTypes)oi).getType())));
		world["options"] = picojson::value(options);

		// world.eras -- the era types in canonical EraTypes order (heritage byEra is a CUMULATIVE threshold:
		// a band applies for every era where currentEra >= band, so the calc needs the ordering to compare).
		picojson::value::array eras;
		for (int ei = 0; ei < GC.getNumEraInfos(); ++ei)
			eras.push_back(picojson::value(std::string(GC.getEraInfo((EraTypes)ei).getType())));
		world["eras"] = picojson::value(eras);

		// world.diplomacy -- per team-pair diplomatic STATE the trade calc needs: the foreign-route peace
		// modifier (getPeaceTradeModifier, CvCity.cpp) ramps with GET_TEAM(my).AI_getAtPeaceCounter(their) and
		// zeroes at war. Not a deposit -- live state, extracted as raw facts (per-ordered-pair atWar + counter).
		picojson::value::array diplomacy;
		for (int da = 0; da < MAX_PC_TEAMS; ++da)
		{
			if (!GET_TEAM((TeamTypes)da).isAlive()) continue;
			for (int db = 0; db < MAX_PC_TEAMS; ++db)
			{
				if (db == da || !GET_TEAM((TeamTypes)db).isAlive()) continue;
				picojson::value::object dp;
				dp["team"]           = picojson::value((double)da);
				dp["otherTeam"]      = picojson::value((double)db);
				dp["atWar"]          = picojson::value(GET_TEAM((TeamTypes)da).isAtWar((TeamTypes)db));
				dp["atPeaceCounter"] = picojson::value((double)GET_TEAM((TeamTypes)da).AI_getAtPeaceCounter((TeamTypes)db));
				diplomacy.push_back(picojson::value(dp));
			}
		}
		world["diplomacy"] = picojson::value(diplomacy);

		static const char* aCommName[4] = { "gold", "research", "culture", "espionage" };
		const int aCommType[4] = { COMMERCE_GOLD, COMMERCE_RESEARCH, COMMERCE_CULTURE, COMMERCE_ESPIONAGE };

		picojson::value::array teams;
		for (int t = 0; t < MAX_PC_TEAMS; ++t)
		{
			CvTeam& kTeam = GET_TEAM((TeamTypes)t);
			if (!kTeam.isAlive()) continue;

			picojson::value::array empires;
			for (int p = 0; p < MAX_PLAYERS; ++p)
			{
				CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)p);
				if (!kPlayer.isAlive() || (int)kPlayer.getTeam() != t) continue;
				if (iPlayerFilter >= 0 && p != iPlayerFilter) continue;

				picojson::value::object emp;
				emp["id"] = picojson::value((double)p);

				picojson::value::array civics;
				for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
				{
					const CivicTypes eC = kPlayer.getCivics((CivicOptionTypes)co);
					if (eC != NO_CIVIC) civics.push_back(picojson::value(std::string(GC.getCivicInfo(eC).getType())));
				}
				emp["civics"] = picojson::value(civics);

				picojson::value::array traits;
				for (int tr = 0; tr < GC.getNumTraitInfos(); ++tr)
					if (kPlayer.hasTrait((TraitTypes)tr))
						traits.push_back(picojson::value(std::string(GC.getTraitInfo((TraitTypes)tr).getType())));
				emp["traits"] = picojson::value(traits);

				// current ERA (heritage byEra deposits resolve against it) + owned HERITAGES (their
				// EraCommerceChanges feed the player's getExtraCommerce100 -- culture + research commerce).
				emp["era"] = picojson::value(std::string(GC.getEraInfo(kPlayer.getCurrentEra()).getType()));
				{
					picojson::value::array herit;
					const std::vector<HeritageTypes> vH = kPlayer.getHeritage();
					for (std::vector<HeritageTypes>::const_iterator it = vH.begin(); it != vH.end(); ++it)
						herit.push_back(picojson::value(std::string(GC.getHeritageInfo(*it).getType())));
					emp["heritages"] = picojson::value(herit);
				}

				const ReligionTypes eState = kPlayer.getStateReligion();
				if (eState != NO_RELIGION)
					emp["stateReligion"] = picojson::value(std::string(GC.getReligionInfo(eState).getType()));
				// Free-Church-style: EVERY present religion's stateReligionCommerce applies, not just the state one
				// (getReligionCommerceByReligion: state==X || no-state || isNonStateReligionCommerce).
				emp["nonStateReligionCommerce"] = picojson::value(kPlayer.isNonStateReligionCommerce());

				// /extractor empire.availableTechs: the engine's canResearch verdict per player -- the COMPARISON
				// oracle for the cascade's ISOLATED tech-availability set, NOT an input to that computation.
				picojson::value::array availTechs;
				for (int iT = 0; iT < GC.getNumTechInfos(); ++iT)
					if (kPlayer.canResearch((TechTypes)iT))
						availTechs.push_back(picojson::value(std::string(GC.getTechInfo((TechTypes)iT).getType())));
				emp["availableTechs"] = picojson::value(availTechs);

				// /extractor empire.availableCivics: the engine's canDoCivics verdict per player -- the COMPARISON
				// oracle for the cascade's ISOLATED civic-availability set, NOT an input to that computation.
				picojson::value::array availCivics;
				for (int iC = 0; iC < GC.getNumCivicInfos(); ++iC)
					if (kPlayer.canDoCivics((CivicTypes)iC))
						availCivics.push_back(picojson::value(std::string(GC.getCivicInfo((CivicTypes)iC).getType())));
				emp["availableCivics"] = picojson::value(availCivics);

				// /extractor empire.availableBuilds: the BUILD-UNLOCK verdict per player -- the build's tech prereq
				// is held (NOT per-plot canBuild; placement is out of scope -- "available", not "can do it here").
				// COMPARISON oracle only.
				picojson::value::array availBuilds;
				for (int iB = 0; iB < GC.getNumBuildInfos(); ++iB)
				{
					const TechTypes ePrereq = (TechTypes)GC.getBuildInfo((BuildTypes)iB).getTechPrereq();
					if (ePrereq == NO_TECH || GET_TEAM(kPlayer.getTeam()).isHasTech(ePrereq))
						availBuilds.push_back(picojson::value(std::string(GC.getBuildInfo((BuildTypes)iB).getType())));
				}
				emp["availableBuilds"] = picojson::value(availBuilds);

				picojson::value::object sliders;
				for (int cc = 0; cc < 4; ++cc)
					sliders[aCommName[cc]] = picojson::value((double)kPlayer.getCommercePercent((CommerceTypes)aCommType[cc]));
				emp["sliders"] = picojson::value(sliders);

				emp["cityCount"] = picojson::value((double)kPlayer.getNumCities());

				// empire-wide TALLY -- the engine's own live per-player counters (CvPlayer::changeBuildingCount /
				// changeUnitCount, maintained in handleBuildingCounts). This is the aggregate count for empire-scope
				// `requires.build` count atoms (min(BUILDING_X,N) / min(UNIT_X,N)) the offline emulator cannot roll up
				// without seeing every city; we emit the counter directly (no re-derivation).
				picojson::value::object bldgCounts;
				for (int bc = 0; bc < GC.getNumBuildingInfos(); ++bc)
				{
					const int n = kPlayer.getBuildingCount((BuildingTypes)bc);
					if (n > 0) bldgCounts[GC.getBuildingInfo((BuildingTypes)bc).getType()] = picojson::value((double)n);
				}
				emp["buildingCounts"] = picojson::value(bldgCounts);
				picojson::value::object unitCounts;
				for (int uc = 0; uc < GC.getNumUnitInfos(); ++uc)
				{
					const int n = kPlayer.getUnitCount((UnitTypes)uc);
					if (n > 0) unitCounts[GC.getUnitInfo((UnitTypes)uc).getType()] = picojson::value((double)n);
				}
				emp["unitCounts"] = picojson::value(unitCounts);

				// areas: the empire's cities grouped by their area id (no std::map -- distinct-id, two pass)
				std::vector<int> areaIds;
				int iLoop;
				for (CvCity* pCity = kPlayer.firstCity(&iLoop); pCity != NULL; pCity = kPlayer.nextCity(&iLoop))
				{
					const int iA = (pCity->area() != NULL) ? pCity->area()->getID() : -1;
					bool bSeen = false;
					for (size_t k = 0; k < areaIds.size(); ++k) if (areaIds[k] == iA) { bSeen = true; break; }
					if (!bSeen) areaIds.push_back(iA);
				}
				picojson::value::array areas;
				for (size_t ai = 0; ai < areaIds.size(); ++ai)
				{
					const int iA = areaIds[ai];
					picojson::value::array cityArr;
					int iSize = 0;
					for (CvCity* pCity = kPlayer.firstCity(&iLoop); pCity != NULL; pCity = kPlayer.nextCity(&iLoop))
					{
						const int ca = (pCity->area() != NULL) ? pCity->area()->getID() : -1;
						if (ca != iA) continue;
						if (pCity->area() != NULL) iSize = pCity->area()->getNumTiles();
						cityArr.push_back(picojson::value(extractCity(kPlayer, pCity, t)));
					}
					picojson::value::object area;
					area["id"]       = picojson::value((double)iA);
					area["areaSize"] = picojson::value((double)iSize);
					area["cities"]   = picojson::value(cityArr);
					areas.push_back(picojson::value(area));
				}
				emp["areas"] = picojson::value(areas);
				empires.push_back(picojson::value(emp));
			}
			if (empires.empty()) continue;

			picojson::value::object teamO;
			teamO["id"] = picojson::value((double)t);
			picojson::value::array techs;
			for (int tc = 0; tc < GC.getNumTechInfos(); ++tc)
				if (kTeam.isHasTech((TechTypes)tc))
					techs.push_back(picojson::value(std::string(GC.getTechInfo((TechTypes)tc).getType())));
			teamO["techs"]   = picojson::value(techs);
			// team's OBSOLETE buildings (isObsoleteBuilding == tech-obsoleted team-wide) -- observability for the
			// enabler: a building obsolete here is REMOVED from hasBuilding (CvCity.cpp:14838/14852), so this set
			// lets the harness confirm hasBuilding excludes them, and is the input a v3-pure obsolescence compute
			// would read (the engine has no getNumRealBuilding, so the raw built-incl-obsolete set is not available).
			picojson::value::array obsoleteBldgs;
			for (int ob = 0; ob < GC.getNumBuildingInfos(); ++ob)
				if (kTeam.isObsoleteBuilding((BuildingTypes)ob))
					obsoleteBldgs.push_back(picojson::value(std::string(GC.getBuildingInfo((BuildingTypes)ob).getType())));
			teamO["obsoleteBuildings"] = picojson::value(obsoleteBldgs);
			// completed PROJECTS (team-scope): their CommerceModifiers feed the player commerce rate
			// (CvTeam::processProject -> changeCommerceRateModifierfromBuildings) -- a real deposit source.
			picojson::value::array projects;
			for (int pj = 0; pj < GC.getNumProjectInfos(); ++pj)
				if (kTeam.getProjectCount((ProjectTypes)pj) > 0)
					projects.push_back(picojson::value(std::string(GC.getProjectInfo((ProjectTypes)pj).getType())));
			teamO["projects"] = picojson::value(projects);
			// team CorporationRevenueModifier -- the corp-output revenue modifier (CvCity.cpp:12630,
			// getModifiedIntValue(iCommerce, getCorporationRevenueModifier)).
			teamO["corporationRevenueModifier"] = picojson::value((double)kTeam.getCorporationRevenueModifier());
			teamO["empires"] = picojson::value(empires);
			teams.push_back(picojson::value(teamO));
		}
		world["teams"] = picojson::value(teams);

		picojson::value::object root;
		root["schema"] = picojson::value(std::string("gamestate/1"));
		root["turn"]   = picojson::value((double)kGame.getGameTurn());
		root["world"]  = picojson::value(world);
		return CvString(picojson::value(root).serialize().c_str());
	}

	CvString evaluateGate(const char* szAction, const char* szType, int iPlayer, int iCityReq)
	{
		// /extractor/gamestate -- the raw game-state dump; iPlayer < 0 == ALL players (runs before the
		// single-player resolution below, so the "all players" walk is reachable).
		if (strcmp(szAction, "gamestate") == 0)
			return extractGameState(iPlayer);

		picojson::value::object o;
		o["action"] = picojson::value(std::string(szAction));
		o["type"] = picojson::value(std::string(szType));

		if (iPlayer < 0)
		{
			iPlayer = (int)GC.getGame().getActivePlayer();
		}
		o["player"] = picojson::value((double)iPlayer);

		if (iPlayer < 0 || iPlayer >= MAX_PLAYERS || !GET_PLAYER((PlayerTypes)iPlayer).isAlive())
		{
			o["error"] = picojson::value(std::string("player not alive"));
			return CvString(picojson::value(o).serialize().c_str());
		}
		CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)iPlayer);

		// City for the city-relative gates (canConstruct/canTrain/sweep): the requested city, else the capital.
		CvCity* pCity = (iCityReq >= 0) ? kPlayer.getCity(iCityReq) : kPlayer.getCapitalCity();
		const int iCityId = (pCity != NULL) ? pCity->getID() : -1;
		const int iTeam = (int)kPlayer.getTeam();

		// SWEEP: iterate a whole domain (szType = "units" | "buildings") and report cascade-vs-legacy
		// divergences in one pass -- the large-surface lens. Runs on the game thread; the parsed-availability
		// cache keeps a full-roster scan affordable.
		if (strcmp(szAction, "sweep") == 0)
		{
			CvCascadeContext kCtx(iPlayer, iCityId);
			const bool bUnits = (strcmp(szType, "units") == 0);
			const int iN = bUnits ? GC.getNumUnitInfos() : GC.getNumBuildingInfos();
			int iTotal = 0, iEval = 0, iNoJson = 0, iAgree = 0, iDiv = 0;
			picojson::value::array kDiv;
			for (int i = 0; i < iN; ++i)
			{
				++iTotal;
				const char* szT = bUnits ? GC.getUnitInfo((UnitTypes)i).getType()
				                         : GC.getBuildingInfo((BuildingTypes)i).getType();
				CvEntityAvailability kA;
				std::string sN;
				if (!cascadeReadJsonAvailability(szT, kA, sN)) { ++iNoJson; continue; }
				++iEval;
				bool bC, bL;
				if (bUnits)
				{
					bC = cascadeUnitTrainable(i, kCtx);
					bL = (pCity != NULL) && pCity->canTrain((UnitTypes)i);
				}
				else
				{
					bC = cascadeBuildable(kA, COUNTDOMAIN_BUILDING, i, kCtx)
					     && !kA.notConstructible                                                  // cost==-1: never player-constructible
					     && (pCity == NULL || !pCity->hasBuilding((BuildingTypes)i))
					     && !cascadeIsObsoleteForTeam(COUNTDOMAIN_BUILDING, i, iTeam)
					     && !cascadeIsReplacedInCity(i, kCtx)                                      // a successor is active in the city
					     && cascadeBuildingGroupAllows(i, kCtx);
					bL = (pCity != NULL) && pCity->canConstruct((BuildingTypes)i);
				}
				if (bC == bL) { ++iAgree; }
				else
				{
					++iDiv;
					if (kDiv.size() < 250)
					{
						picojson::value::object e;
						e["type"] = picojson::value(std::string(szT));
						e["cascade"] = picojson::value(bC);
						e["legacy"] = picojson::value(bL);
						if (!bUnits && pCity != NULL)
						{
							e["reason"] = picojson::value(std::string(legacyBlockReason(pCity, (BuildingTypes)i)));
							e["cascadeReason"] = picojson::value(std::string(cascadeBlockReason(kA, (BuildingTypes)i, pCity, iTeam, kCtx)));
						}
						kDiv.push_back(picojson::value(e));
					}
				}
			}
			o["domain"] = picojson::value(std::string(bUnits ? "units" : "buildings"));
			o["city"] = picojson::value((double)iCityId);
			o["total"] = picojson::value((double)iTotal);
			o["evaluated"] = picojson::value((double)iEval);
			o["noJson"] = picojson::value((double)iNoJson);
			o["agree"] = picojson::value((double)iAgree);
			o["diverge"] = picojson::value((double)iDiv);
			o["divergences"] = picojson::value(kDiv);
			return CvString(picojson::value(o).serialize().c_str());
		}

		// PLACEMENT SWEEP (§14 H auto-placement shadow, B-i): per (auto-placed building x the player's cities),
		// cascade-would-place vs the legacy maintainers' realized presence. The full per-cell dump (type=full)
		// reconstructs the ENTIRE auto-placement picture from the API alone (the render-from-API / total-observability
		// bar); default returns the divergence triage list (cap 250) + summary. Game thread; parsed-availability cached.
		if (strcmp(szAction, "placementSweep") == 0)
		{
			const bool bFull = (strcmp(szType, "full") == 0);
			std::vector<int> aRoster, aKind;
			cascadeAutoPlacedRoster(aRoster, aKind);

			// Parse each roster building's availability ONCE, reuse across cities.
			std::vector<CvEntityAvailability> aAvail(aRoster.size());
			std::vector<char> aParsed(aRoster.size(), 0);
			for (size_t i = 0; i < aRoster.size(); ++i)
			{
				std::string sN;
				aParsed[i] = cascadeReadJsonAvailability(GC.getBuildingInfo((BuildingTypes)aRoster[i]).getType(), aAvail[i], sN) ? 1 : 0;
			}

			int iCities = 0, iCells = 0, iAgree = 0, iDiv = 0, iNoJson = 0;
			picojson::value::array kDiv;   // divergence triage list (cap 250)
			picojson::value::array kCells; // full per-cell state (type=full only; cap 4000)
			std::map<std::string, int> kHist; // UNCAPPED divergence histogram by "kind:reason" (engine-computed; the trust + completeness fix)
			int iCityIter = 0;
			for (CvCity* pCity = kPlayer.firstCity(&iCityIter); pCity != NULL; pCity = kPlayer.nextCity(&iCityIter))
			{
				++iCities;
				CvCascadeContext kCtx(iPlayer, pCity->getID());
				for (size_t i = 0; i < aRoster.size(); ++i)
				{
					const int b = aRoster[i];
					if (!aParsed[i]) { ++iNoJson; continue; }
					++iCells;
					bool bC = false;
					const char* szReason = cascadePlacementReason(b, aAvail[i], kCtx, iTeam, bC);
					const bool bL = pCity->hasBuilding((BuildingTypes)b);
					const char* szT = GC.getBuildingInfo((BuildingTypes)b).getType();
					if (bC == bL) ++iAgree; else { ++iDiv; kHist[CvString::format("%d:%s", aKind[i], szReason).c_str()]++; }

					if (bFull && kCells.size() < 4000)
					{
						picojson::value::object c;
						c["city"] = picojson::value((double)pCity->getID());
						c["type"] = picojson::value(std::string(szT));
						c["kind"] = picojson::value((double)aKind[i]); // bit0 bAutoBuild, bit1 property-band
						c["cascade"] = picojson::value(bC);
						c["legacy"] = picojson::value(bL);
						c["reason"] = picojson::value(std::string(szReason));
						kCells.push_back(picojson::value(c));
					}
					if (bC != bL && kDiv.size() < 250)
					{
						picojson::value::object e;
						e["city"] = picojson::value((double)pCity->getID());
						e["type"] = picojson::value(std::string(szT));
						e["kind"] = picojson::value((double)aKind[i]);
						e["cascade"] = picojson::value(bC);
						e["legacy"] = picojson::value(bL);
						e["reason"] = picojson::value(std::string(szReason));
						kDiv.push_back(picojson::value(e));
					}
				}
			}
			o["roster"] = picojson::value((double)aRoster.size());
			o["cities"] = picojson::value((double)iCities);
			o["cells"] = picojson::value((double)iCells);
			o["noJson"] = picojson::value((double)iNoJson);
			o["agree"] = picojson::value((double)iAgree);
			o["diverge"] = picojson::value((double)iDiv);
			picojson::value::object kHistO;
			for (std::map<std::string,int>::const_iterator it = kHist.begin(); it != kHist.end(); ++it)
				kHistO[it->first] = picojson::value((double)it->second);
			o["reasonHistogram"] = picojson::value(kHistO); // UNCAPPED: every divergence counted (not just the 250 sample)
			o["divergences"] = picojson::value(kDiv);
			if (bFull) o["all"] = picojson::value(kCells);
			return CvString(picojson::value(o).serialize().c_str());
		}

		// DORMANCY SWEEP (§14 H B-ii): for each BUILT building x the player's cities, cascade-active (requires.operate)
		// vs legacy hasFullyActiveBuilding (resource/replacement disabling + religious limit). `type=full` adds the full
		// per-cell `all[]`; default = divergence triage (cap 250) + summary. Game thread; parsed-availability cached.
		if (strcmp(szAction, "dormancySweep") == 0)
		{
			const bool bFull = (strcmp(szType, "full") == 0);
			const int iNum = GC.getNumBuildingInfos();
			std::vector<CvEntityAvailability> aAvail(iNum);
			std::vector<char> aState(iNum, 0); // 0 not tried, 1 parsed, 2 no JSON

			int iCities = 0, iCells = 0, iAgree = 0, iDiv = 0;
			picojson::value::array kDiv, kCells;
			std::map<std::string, int> kHist; // UNCAPPED divergence histogram by "cascade|legacy" reason pair
			int iCityIter = 0;
			for (CvCity* pCity = kPlayer.firstCity(&iCityIter); pCity != NULL; pCity = kPlayer.nextCity(&iCityIter))
			{
				++iCities;
				CvCascadeContext kCtx(iPlayer, pCity->getID());
				for (int b = 0; b < iNum; ++b)
				{
					if (!pCity->hasBuilding((BuildingTypes)b)) continue;
					if (aState[b] == 0)
					{
						std::string sN;
						aState[b] = cascadeReadJsonAvailability(GC.getBuildingInfo((BuildingTypes)b).getType(), aAvail[b], sN) ? 1 : 2;
					}
					if (aState[b] != 1) continue;
					++iCells;
					bool bCA = false;
					const char* szCascade = cascadeDormancyReason(aAvail[b], kCtx, bCA);
					const bool bLA = pCity->hasFullyActiveBuilding((BuildingTypes)b);
					const char* szLegacy = cascadeDormancyLegacyReason(pCity, b);
					const char* szT = GC.getBuildingInfo((BuildingTypes)b).getType();
					if (bCA == bLA) ++iAgree; else { ++iDiv; kHist[std::string("cascade=") + szCascade + "|legacy=" + szLegacy]++; }

					if (bFull && kCells.size() < 4000)
					{
						picojson::value::object c;
						c["city"] = picojson::value((double)pCity->getID());
						c["type"] = picojson::value(std::string(szT));
						c["cascadeActive"] = picojson::value(bCA);
						c["legacyActive"] = picojson::value(bLA);
						c["cascadeReason"] = picojson::value(std::string(szCascade));
						c["legacyReason"] = picojson::value(std::string(szLegacy));
						kCells.push_back(picojson::value(c));
					}
					if (bCA != bLA && kDiv.size() < 250)
					{
						picojson::value::object e;
						e["city"] = picojson::value((double)pCity->getID());
						e["type"] = picojson::value(std::string(szT));
						e["cascadeActive"] = picojson::value(bCA);
						e["legacyActive"] = picojson::value(bLA);
						e["cascadeReason"] = picojson::value(std::string(szCascade));
						e["legacyReason"] = picojson::value(std::string(szLegacy));
						kDiv.push_back(picojson::value(e));
					}
				}
			}
			o["cities"] = picojson::value((double)iCities);
			o["builtCells"] = picojson::value((double)iCells);
			o["agree"] = picojson::value((double)iAgree);
			o["diverge"] = picojson::value((double)iDiv);
			picojson::value::object kHistO;
			for (std::map<std::string,int>::const_iterator it = kHist.begin(); it != kHist.end(); ++it)
				kHistO[it->first] = picojson::value((double)it->second);
			o["reasonHistogram"] = picojson::value(kHistO); // UNCAPPED divergence histogram
			o["divergences"] = picojson::value(kDiv);
			if (bFull) o["all"] = picojson::value(kCells);
			return CvString(picojson::value(o).serialize().c_str());
		}

		// MODIFIER SWEEP (modifier-cascade-shadow-spec §3.2): per (the player's cities x PILOT yield family), the cascade
		// effective vs legacy getYieldRate100 (both x1 realized), decomposed (flat/percent/mult), cause-tagged + care-graded
		// (Fine..Meltdown). `type=full` = the COMPLETE per-cell array (render-the-whole-state / total-observability bar);
		// default = the divergence triage list (delta!=0, cap 250) + the UNCAPPED cause + care histograms. `type=`food|
		// production|commerce (or `channel=` folded into it by the dispatcher) scopes to one family. Game thread; the
		// per-building modifier parse is cached inside CvCascadeModifier. NB pre-completion the histogram is dominated by
		// missingDeposit/Bug -- only BUILDING deposits are wired (the expected parity work, §3.1a), not a real alarm yet.
		if (strcmp(szAction, "modifierSweep") == 0)
		{
			const bool bFull = (strcmp(szType, "full") == 0);
			int iOnly = -1; // channel scoping: type=<familyKey> (food|production|commerce|gold|...|greatPeople) -> just that family
			for (int f = 0; f < NUM_MODIFIER_FAMILIES; ++f)
				if (strcmp(szType, cascadeModifierFamilyInfo(f).szKey) == 0) iOnly = f;

			int iCities = 0, iCells = 0, iAgree = 0, iDiv = 0;
			int aCare[NUM_MODIFIER_CARE_LEVELS] = { 0 };
			picojson::value::array kDiv, kCells;
			std::map<std::string, int> kCause; // UNCAPPED divergence histogram by "cause:CareName"
			int iCityIter = 0;
			for (CvCity* pCity = kPlayer.firstCity(&iCityIter); pCity != NULL; pCity = kPlayer.nextCity(&iCityIter))
			{
				++iCities;
				CvCascadeContext kCtx(iPlayer, pCity->getID());
				for (int f = 0; f < NUM_MODIFIER_FAMILIES; ++f)
				{
					if (iOnly >= 0 && f != iOnly) continue;
					++iCells;
					CvModifierSlot slot;
					int iBase = 0, iCascade = 0, iLegacy = 0;
					cascadeModifierFamilyShadow(pCity, kCtx, f, slot, iBase, iCascade, iLegacy); // all-channel shadow (registry combine)
					int iCare = 0;
					const char* szCause = cascadeModifierClassify(iCascade, iLegacy, slot, iCare);
					if (iCare >= 0 && iCare < NUM_MODIFIER_CARE_LEVELS) ++aCare[iCare];
					if (iCascade == iLegacy) ++iAgree;
					else { ++iDiv; kCause[CvString::format("%s:%s", szCause, cascadeModifierCareName(iCare)).c_str()]++; }

					if (bFull && (int)kCells.size() < 4000)
					{
						picojson::value::object c;
						c["city"]      = picojson::value((double)pCity->getID());
						c["channel"]   = picojson::value(std::string(cascadeModifierFamilyInfo(f).szKey));
						c["base"]      = picojson::value((double)iBase);
						c["flat"]      = picojson::value((double)slot.iFlat);
						c["percent"]   = picojson::value((double)slot.iPercent);
						c["mult100"]   = picojson::value((double)slot.iMultiplierX100);
						c["cascade"]   = picojson::value((double)iCascade);
						c["legacy"]    = picojson::value((double)iLegacy);
						c["delta"]     = picojson::value((double)(iCascade - iLegacy));
						c["cause"]     = picojson::value(std::string(szCause));
						c["care"]      = picojson::value((double)iCare);
						c["careName"]  = picojson::value(std::string(cascadeModifierCareName(iCare)));
						kCells.push_back(picojson::value(c));
					}
					if (iCascade != iLegacy && (int)kDiv.size() < 250)
					{
						picojson::value::object e;
						e["city"]     = picojson::value((double)pCity->getID());
						e["channel"]  = picojson::value(std::string(cascadeModifierFamilyInfo(f).szKey));
						e["base"]     = picojson::value((double)iBase);
						e["flat"]     = picojson::value((double)slot.iFlat);
						e["percent"]  = picojson::value((double)slot.iPercent);
						e["mult100"]  = picojson::value((double)slot.iMultiplierX100);
						e["cascade"]  = picojson::value((double)iCascade);
						e["legacy"]   = picojson::value((double)iLegacy);
						e["delta"]    = picojson::value((double)(iCascade - iLegacy));
						e["cause"]    = picojson::value(std::string(szCause));
						e["care"]     = picojson::value((double)iCare);
						e["careName"] = picojson::value(std::string(cascadeModifierCareName(iCare)));
						kDiv.push_back(picojson::value(e));
					}
				}
			}
			o["parityMode"] = picojson::value(cascadeModifierParityMode);
			o["calcFlow"]   = picojson::value((double)cascadeModifierCalcFlow);
			o["channel"]    = picojson::value(std::string(iOnly >= 0 ? cascadeModifierFamilyInfo(iOnly).szKey : "all"));
			o["cities"]     = picojson::value((double)iCities);
			o["cells"]      = picojson::value((double)iCells);
			o["agree"]      = picojson::value((double)iAgree);
			o["diverge"]    = picojson::value((double)iDiv);
			picojson::value::object kCareO;
			for (int c = 0; c < NUM_MODIFIER_CARE_LEVELS; ++c)
				kCareO[cascadeModifierCareName(c)] = picojson::value((double)aCare[c]);
			o["careHistogram"] = picojson::value(kCareO);
			picojson::value::object kCauseO;
			for (std::map<std::string,int>::const_iterator it = kCause.begin(); it != kCause.end(); ++it)
				kCauseO[it->first] = picojson::value((double)it->second);
			o["causeHistogram"] = picojson::value(kCauseO); // UNCAPPED: every divergence counted (not just the 250 sample)
			o["divergences"] = picojson::value(kDiv);
			if (bFull) o["all"] = picojson::value(kCells);
			return CvString(picojson::value(o).serialize().c_str());
		}

		// MOVEMENT/RANGE SWEEP (#430 movement model -- observe-then-shadow, modifier.md 6.6). Two layers now: the
		// LEGACY decomposition dumped systematically (per-unit movement points + range; per-(unit,edge) moveCost),
		// AND the CASCADE SHADOW column (cascadeCost/cascadeDelta/cascadeCause/cascadeCare per edge + the divergence
		// + cause/care histograms). The cascade resolver (CvCascadeMovement) sources the PLOT-SUBSTRATE
		// (terrain/feature/route moveCost) from the migrated JSON and reads the unit-side + globals from the engine
		// (cut-1: the unit-plane is the next channel), so a divergence localises to the substrate migration. The
		// shadow diffs the cascade against the FRESH legacy iFinal (`cost`), NEVER the AI-cached `engineCost`.
		// LEGACY decomposition (kept as the authority the cascade mirrors), per-unit movement points + range; per-
		// (unit, adjacent-edge) moveCost broken into terrain/feature/hills/river/peak/route-min/discount/double-move/
		// floor) -- the magnitude analogue of /diagnostic/modifierSweep, the map that licenses the later cutover.
		// Each edge is cross-checked against the engine's own CvPlot::movementCost (`engineCost`). The check is
		// SPLIT human/non-human (the field `human`), because CvPlot::movementCost caches its result ONLY for
		// non-human units, keyed by m_movementCharacteristicsHash -- and that hash folds in only the base-unit
		// zobrist + promotions/unitcombats flagged changesMoveThroughPlots(), NOT getExtraMoveDiscount or
		// baseMoves (CvUnit.cpp, verified 2026-06-20). So AI units that differ only in move-discount/base-moves
		// COLLIDE on the cache and return each other's cost. Therefore: `edgeMismatchHuman` MUST be 0 (it is the
		// clean decomposition-validity check, cache-free); `edgeMismatchNonHuman` is INFORMATIONAL -- a small count
		// reflects the engine's AI movement-cost cache collision/staleness (a real legacy quirk, not a mirror bug;
		// the future movement shadow must diff the FRESH `cost`, never the AI-cached `engineCost`). No type= needed.
		if (strcmp(szAction, "movementSweep") == 0)
		{
			const bool bFull = (strcmp(szType, "full") == 0);
			const int iUnitCap = bFull ? 100000 : 400;  // detail rows; histograms count ALL units regardless
			const int iEdgeCap = bFull ? 20000 : 1500;  // per-(unit, neighbour) moveCost rows

			int iUnits = 0, iEdges = 0, iEdgeMismatch = 0, iEdgeMismatchHuman = 0, iEdgeMismatchNonHuman = 0;
			picojson::value::array kUnits, kEdges;
			std::map<int, int> kBaseMovesHist; // baseMoves -> count (uncapped)
			std::map<int, int> kRangeHist;     // range -> count, range>0 only (uncapped)
			// CASCADE SHADOW (the cascade-vs-legacy diff column): the cascade resolver sources the PLOT-SUBSTRATE
			// (terrain/feature/route moveCost) from the migrated JSON; a divergence vs the FRESH legacy iFinal
			// (NOT the AI-cached engineCost) localises to that migration. Cut-1 scope (CvCascadeMovement.h).
			int iCascadeDiverge = 0;
			std::map<std::string, int> kCauseHist; // cause-tag -> count (uncapped)
			std::map<std::string, int> kCareHist;  // care-name -> count (uncapped)
			const CvMovementSubstrate& kSub = cascadeMovementSubstrate(); // also primes the parse-once cache
			// the UNIT-PLANE shadow (the modifier-family channel): per-unit cascade-aggregated baseMoves/discount/
			// range/caps vs the engine's MIGRATED parts, + per-source attribution (the Meta rung) in detail rows.
			int iUnitDiverge = 0;
			std::map<std::string, int> kUnitCauseHist, kUnitCareHist; // uncapped
			const CvMovementUnitData& kUD = cascadeMovementUnitData(); // primes the unit-plane parse-once cache

			foreach_(const CvUnit* pUnit, kPlayer.units())
			{
				++iUnits;
				const UnitTypes eUT = pUnit->getUnitType();
				const int iBaseMoves = pUnit->baseMoves();
				const int iRange = pUnit->airRange();
				const bool bHuman = pUnit->isHuman(); // cache-free path -> the clean decomposition check
				kBaseMovesHist[iBaseMoves]++;
				if (iRange > 0) kRangeHist[iRange]++;

				// the UNIT-PLANE cascade shadow (every unit -> divergence count + histograms): aggregate the cascade
				// migrated parts (type + held promos/combats) and diff vs the engine's migrated parts (UnitInfo +
				// own getExtra*). The team/national scopes + commander cross-edge + flying runtime are engine-only
				// (not migrated) -- excluded, so a delta isolates the unit-plane movement/range data migration.
				CvUnitMoveAgg agg;
				cascadeUnitMoveAgg(pUnit, agg);
				const CvUnitInfo& kUI = GC.getUnitInfo(eUT);
				// FULL engine values -- the cascade agg now folds team/empire scope, so the diff spans the whole
				// baseMoves/airRange. The only residue is the commander/commodore cross-edge (getExtraMoves folds it,
				// the cascade doesn't -> tagged commanderCrossEdge) and runtime grants (circumnavigate sea moves).
				const int iEngMoves = pUnit->baseMoves();   // UnitInfo.getMoves + getExtraMoves(own+cmd) + team(domain)
				const int iEngDisc  = pUnit->getExtraMoveDiscount();             // own + commander
				const int iEngRange = pUnit->airRange();    // the full four-term (info + extra + team-AIR + national)
				const int iDMoves = agg.iMovesMigrated - iEngMoves;
				const int iDDisc  = agg.iMoveDiscount  - iEngDisc;
				const int iDRange = agg.iRangeMigrated - iEngRange;
				const bool bIgnoreMatch = (agg.bIgnoreTerrain == kUI.isIgnoreTerrainCost());
				const bool bFlatMatch   = (agg.bFlatMoveCost  == kUI.isFlatMovementCost());
				int iUCare = CARE_FINE; const char* szUCause = "match";
				if (iDMoves != 0 || iDDisc != 0 || iDRange != 0 || !bIgnoreMatch || !bFlatMatch)
				{
					const bool bCmd = (!pUnit->isCommander() && pUnit->getCommander() != NULL)
						|| (!pUnit->isCommodore() && pUnit->getCommodore() != NULL);
					if ((iDMoves != 0 || iDDisc != 0) && bCmd) { szUCause = "commanderCrossEdge"; iUCare = CARE_WEIRD; }
					else if (iDMoves != 0)   { szUCause = "moves";         iUCare = CARE_BUG; }
					else if (iDDisc  != 0)   { szUCause = "moveDiscount";  iUCare = CARE_BUG; }
					else if (iDRange != 0)   { szUCause = "range";         iUCare = CARE_BUG; }
					else if (!bIgnoreMatch)  { szUCause = "ignoreTerrain"; iUCare = CARE_BUG; }
					else                     { szUCause = "flatMoveCost";  iUCare = CARE_BUG; }
					++iUnitDiverge;
				}
				kUnitCauseHist[szUCause]++;
				kUnitCareHist[cascadeModifierCareName(iUCare)]++;

				if ((int)kUnits.size() < iUnitCap)
				{
					picojson::value::object u;
					u["id"]            = picojson::value((double)pUnit->getID());
					u["type"]          = picojson::value(std::string(eUT != NO_UNIT ? GC.getUnitInfo(eUT).getType() : "NO_UNIT"));
					u["domain"]        = picojson::value((double)(int)pUnit->getDomainType());
					u["baseMoves"]     = picojson::value((double)iBaseMoves);
					u["maxMoves"]      = picojson::value((double)pUnit->maxMoves());     // x100 budget
					u["movesLeft"]     = picojson::value((double)pUnit->movesLeft());
					u["moveDiscount"]  = picojson::value((double)pUnit->getExtraMoveDiscount());
					u["range"]         = picojson::value((double)iRange);
					u["flatMovement"]  = picojson::value(pUnit->flatMovementCost());
					u["ignoreTerrain"] = picojson::value(pUnit->ignoreTerrainCost());
					// --- the unit-plane cascade shadow (migrated parts vs engine) ---
					u["cascadeMovesMigrated"] = picojson::value((double)agg.iMovesMigrated);
					u["engineMovesMigrated"]  = picojson::value((double)iEngMoves);
					u["cascadeMovesDelta"]    = picojson::value((double)iDMoves);
					u["cascadeMoveDiscount"]  = picojson::value((double)agg.iMoveDiscount);
					u["cascadeDiscountDelta"] = picojson::value((double)iDDisc);
					u["cascadeRangeMigrated"] = picojson::value((double)agg.iRangeMigrated);
					u["engineRangeMigrated"]  = picojson::value((double)iEngRange);
					u["cascadeRangeDelta"]    = picojson::value((double)iDRange);
					u["cascadeIgnoreTerrain"] = picojson::value(agg.bIgnoreTerrain);
					u["cascadeFlatMoveCost"]  = picojson::value(agg.bFlatMoveCost);
					u["cascadeHillsDoubleMove"]      = picojson::value(agg.bHillsDoubleMove);
					u["cascadeTerrainDoubleMoveKeys"] = picojson::value((double)agg.aiTerrainDM.size());
					u["cascadeFeatureDoubleMoveKeys"] = picojson::value((double)agg.aiFeatureDM.size());
					u["cascadeUnitCause"] = picojson::value(std::string(szUCause));
					u["cascadeUnitCare"]  = picojson::value(std::string(cascadeModifierCareName(iUCare)));
					if (szUCause != std::string("match")) u["CASCADE_UNIT_DIVERGE"] = picojson::value(true);
					// META: per-source attribution -- which type/promotion/unitcombat contributed what
					picojson::value::array kSrc;
					for (size_t si = 0; si < agg.sources.size(); ++si)
					{
						const CvMoveSourceRef& ref = agg.sources[si];
						const CvMoveSourceProfile* p = ref.pProfile;
						picojson::value::object s;
						if (p != NULL) // unit / promotion / unitcombat -- the full per-source profile
						{
							const char* szKind = (ref.iKind == 0) ? "unit" : (ref.iKind == 1) ? "promotion" : "unitcombat";
							const char* szT = (ref.iKind == 0) ? GC.getUnitInfo((UnitTypes)ref.iType).getType()
								: (ref.iKind == 1) ? GC.getPromotionInfo((PromotionTypes)ref.iType).getType()
								: GC.getUnitCombatInfo((UnitCombatTypes)ref.iType).getType();
							s["kind"] = picojson::value(std::string(szKind));
							s["type"] = picojson::value(std::string(szT));
							if (p->iMoves != 0)        s["moves"]        = picojson::value((double)p->iMoves);
							if (p->iMoveDiscount != 0) s["moveDiscount"] = picojson::value((double)p->iMoveDiscount);
							if (p->iRange != 0)        s["range"]        = picojson::value((double)p->iRange);
							if (p->bIgnoreTerrain)     s["ignoreTerrain"] = picojson::value(true);
							if (p->bFlatMoveCost)      s["flatMoveCost"]  = picojson::value(true);
							if (p->bHillsDoubleMove)   s["hillsDoubleMove"] = picojson::value(true);
							if (!p->aiTerrainDM.empty()) s["terrainDoubleMove"] = picojson::value((double)p->aiTerrainDM.size());
							if (!p->aiFeatureDM.empty()) s["featureDoubleMove"] = picojson::value((double)p->aiFeatureDM.size());
						}
						else // tech (team route/domain) / trait (national range) -- the team/empire-scope aggregate
						{
							s["kind"] = picojson::value(std::string(ref.iKind == 3 ? "team" : "empire"));
							if (ref.iContribMoves != 0) s["moves"] = picojson::value((double)ref.iContribMoves);
							if (ref.iContribRange != 0) s["range"] = picojson::value((double)ref.iContribRange);
						}
						kSrc.push_back(picojson::value(s));
					}
					u["cascadeSources"] = picojson::value(kSrc); // the Meta per-source decomposition
					kUnits.push_back(picojson::value(u));
				}

				const CvPlot* pFrom = pUnit->plot();
				if (pFrom == NULL) continue;
				for (int d = 0; d < NUM_DIRECTION_TYPES && (int)kEdges.size() < iEdgeCap; ++d)
				{
					const CvPlot* pTo = plotDirection(pFrom->getX(), pFrom->getY(), (DirectionTypes)d);
					if (pTo == NULL) continue;
					++iEdges;
					MoveCostParts mc;
					decomposeMoveCost(pTo, pUnit, pFrom, mc);
					const int iEngine = pTo->movementCost(pUnit, pFrom); // what the game uses (AI-CACHED for non-human)
					const bool bMatch = (mc.iFinal == iEngine);
					if (!bMatch) { ++iEdgeMismatch; if (bHuman) ++iEdgeMismatchHuman; else ++iEdgeMismatchNonHuman; }

					// the cascade resolver, diffed against the FRESH legacy decomposition (mc.iFinal), never engineCost
					CvCascadeMoveCost cc;
					cascadeResolveMoveCost(pTo, pUnit, pFrom, cc);
					const int iCascadeDelta = cc.iFinal - mc.iFinal;
					int iCare = 0;
					const char* szCause = cascadeMoveClassify(iCascadeDelta, cc, iCare);
					if (iCascadeDelta != 0) ++iCascadeDiverge;
					kCauseHist[szCause]++;
					kCareHist[cascadeModifierCareName(iCare)]++;

					picojson::value::object e;
					e["unit"]            = picojson::value((double)pUnit->getID());
					e["human"]           = picojson::value(bHuman);
					e["fromX"]           = picojson::value((double)pFrom->getX());
					e["fromY"]           = picojson::value((double)pFrom->getY());
					e["toX"]             = picojson::value((double)pTo->getX());
					e["toY"]             = picojson::value((double)pTo->getY());
					e["denominator"]     = picojson::value((double)mc.iDenominator);
					e["earlyFlat"]       = picojson::value(mc.bEarlyFlat);
					e["routeBranch"]     = picojson::value(mc.bRouteBranch);
					e["routeCost"]       = picojson::value((double)mc.iRouteCost);
					e["routeFlatCost"]   = picojson::value((double)mc.iRouteFlatCost);
					e["terrain"]         = picojson::value((double)mc.iTerrain);
					e["feature"]         = picojson::value((double)mc.iFeature);
					e["hills"]           = picojson::value((double)mc.iHills);
					e["river"]           = picojson::value((double)mc.iRiver);
					e["peak"]            = picojson::value((double)mc.iPeak);
					e["discount"]        = picojson::value((double)mc.iDiscount);
					e["regularPreDenom"] = picojson::value((double)mc.iRegularPreDenom);
					e["doubleDiv"]       = picojson::value((double)mc.iDoubleDiv);
					e["ignoreTerrain"]   = picojson::value(mc.bIgnoreTerrain);
					e["cost"]            = picojson::value((double)mc.iFinal);  // recomposed from the parts above
					e["engineCost"]      = picojson::value((double)iEngine);    // CvPlot::movementCost -- the authority
					if (!bMatch) e["MISMATCH"] = picojson::value(true);
					// the cascade shadow column (plot-substrate sourced from the migrated JSON; cut-1)
					e["cascadeCost"]     = picojson::value((double)cc.iFinal);
					e["cascadeDelta"]    = picojson::value((double)iCascadeDelta);  // cascade - legacy(FRESH)
					e["cascadeCause"]    = picojson::value(std::string(szCause));
					e["cascadeCare"]     = picojson::value(std::string(cascadeModifierCareName(iCare)));
					if (cc.bSubstrateMiss)  e["cascadeSubstrateMiss"] = picojson::value(true);
					if (iCascadeDelta != 0) e["CASCADE_DIVERGE"] = picojson::value(true);
					kEdges.push_back(picojson::value(e));
				}
			}

			o["units"]        = picojson::value((double)iUnits);
			o["edges"]        = picojson::value((double)iEdges);
			o["edgeMismatch"] = picojson::value((double)iEdgeMismatch); // total (human + non-human)
			o["edgeMismatchHuman"]    = picojson::value((double)iEdgeMismatchHuman);    // MUST be 0 -- decomposition-validity check (cache-free)
			o["edgeMismatchNonHuman"] = picojson::value((double)iEdgeMismatchNonHuman); // informational -- AI movementCost cache collision/staleness
			o["moveDenominator"] = picojson::value((double)GC.getMOVE_DENOMINATOR());
			// ---- the cascade shadow summary (plot-substrate channel) ----
			o["cascadeDiverge"] = picojson::value((double)iCascadeDiverge); // edges where cascade != legacy(FRESH)
			picojson::value::object kCause;
			for (std::map<std::string, int>::const_iterator it = kCauseHist.begin(); it != kCauseHist.end(); ++it)
				kCause[it->first] = picojson::value((double)it->second);
			o["cascadeCauseHistogram"] = picojson::value(kCause);
			picojson::value::object kCare;
			for (std::map<std::string, int>::const_iterator it = kCareHist.begin(); it != kCareHist.end(); ++it)
				kCare[it->first] = picojson::value((double)it->second);
			o["cascadeCareHistogram"] = picojson::value(kCare);
			picojson::value::object kSubO;
			kSubO["terrainParsed"] = picojson::value((double)kSub.iTerrainParsed);
			kSubO["featureParsed"] = picojson::value((double)kSub.iFeatureParsed);
			kSubO["routeParsed"]   = picojson::value((double)kSub.iRouteParsed);
			kSubO["missing"]       = picojson::value((double)kSub.iMissing); // entities with no cascade moveCost datum
			o["cascadeSubstrate"] = picojson::value(kSubO);
			// ---- the unit-plane shadow summary (the modifier-family channel) ----
			o["cascadeUnitDiverge"] = picojson::value((double)iUnitDiverge); // units where a migrated part != engine
			picojson::value::object kUCause;
			for (std::map<std::string, int>::const_iterator it = kUnitCauseHist.begin(); it != kUnitCauseHist.end(); ++it)
				kUCause[it->first] = picojson::value((double)it->second);
			o["cascadeUnitCauseHistogram"] = picojson::value(kUCause);
			picojson::value::object kUCare;
			for (std::map<std::string, int>::const_iterator it = kUnitCareHist.begin(); it != kUnitCareHist.end(); ++it)
				kUCare[it->first] = picojson::value((double)it->second);
			o["cascadeUnitCareHistogram"] = picojson::value(kUCare);
			picojson::value::object kUDO;
			kUDO["unitParsed"]   = picojson::value((double)kUD.iUnitParsed);   // unit types with any movement/range/cap
			kUDO["promoParsed"]  = picojson::value((double)kUD.iPromoParsed);  // promotions with any movement datum
			kUDO["combatParsed"] = picojson::value((double)kUD.iCombatParsed); // unitcombats with any movement datum
			kUDO["techRouteParsed"]  = picojson::value((double)kUD.iTechRouteParsed);  // tech->route moveCost deposits
			kUDO["techDomainParsed"] = picojson::value((double)kUD.iTechDomainParsed); // tech->domain moves (curator gap: 0)
			kUDO["traitRangeParsed"] = picojson::value((double)kUD.iTraitRangeParsed); // trait->national range deposits
			o["cascadeUnitData"] = picojson::value(kUDO);
			picojson::value::object kBM;
			for (std::map<int, int>::const_iterator it = kBaseMovesHist.begin(); it != kBaseMovesHist.end(); ++it)
				kBM[CvString::format("%d", it->first).c_str()] = picojson::value((double)it->second);
			o["baseMovesHistogram"] = picojson::value(kBM);
			picojson::value::object kRH;
			for (std::map<int, int>::const_iterator it = kRangeHist.begin(); it != kRangeHist.end(); ++it)
				kRH[CvString::format("%d", it->first).c_str()] = picojson::value((double)it->second);
			o["rangeHistogram"] = picojson::value(kRH);
			o["unitsDetailed"] = picojson::value(kUnits);
			o["moveCostEdges"] = picojson::value(kEdges);
			return CvString(picojson::value(o).serialize().c_str());
		}

		// GAME-STATE dump (§A victory-progress gap #1): end-detection + victory countdowns so an autoplay agent can tell
		// the game is OVER and why -- without it the run has no terminal signal. No type needed. Game thread; const reads.
		if (strcmp(szAction, "game") == 0)
		{
			CvGame& kG = GC.getGame(); // getGameTurn() is non-const
			o["turn"] = picojson::value((double)kG.getGameTurn());
			o["elapsedTurns"] = picojson::value((double)kG.getElapsedGameTurns());
			o["maxTurns"] = picojson::value((double)kG.getMaxTurns());
			o["gameState"] = picojson::value((double)(int)kG.getGameState()); // 0 ON, 1 OVER, 2 EXTENDED
			o["gameOver"] = picojson::value(kG.getGameState() == GAMESTATE_OVER);
			o["winnerTeam"] = picojson::value((double)(int)kG.getWinner());
			o["victory"] = picojson::value((double)(int)kG.getVictory());
			o["era"] = picojson::value((double)(int)kG.getCurrentEra());
			// per-valid-victory countdown for the requested player's team (>=0 means in progress / triggered)
			picojson::value::object kVic;
			for (int v = 0; v < GC.getNumVictoryInfos(); ++v)
			{
				if (!kG.isVictoryValid((VictoryTypes)v)) continue;
				const int iCd = GET_TEAM((TeamTypes)iTeam).getVictoryCountdown((VictoryTypes)v);
				kVic[CvString::format("%d", v).c_str()] = picojson::value((double)iCd);
			}
			o["victoryCountdownThisTeam"] = picojson::value(kVic);
			return CvString(picojson::value(o).serialize().c_str());
		}

		// ============================ NO-TYPE, CITY-RELATIVE DUMP ACTIONS ============================
		// These take no `type=` (they are city-relative, like `game`), so they MUST sit BEFORE the type-guard
		// below -- `getInfoTypeForString("")` returns -1, so a no-type action placed after the guard can never be
		// reached (it errors "type not loaded"). `modifier` was previously below the guard, making it unreachable
		// without a dummy type despite its doc + bNoTypeAction listing -- relocated here 2026-06-19.

		// CITY-INPUT (owner 2026-06-19; calc-emulator-spec.md §5) -- the LIVE game-dump that feeds the external calc
		// emulator: a real city's full city-yields INPUT VECTOR + the live LEGACY and CASCADE outputs, so the offline
		// emulator can (a) reproduce getYieldRate100 EXACTLY from these terms -- the fidelity credential that licenses
		// the DESTROY pass -- and (b) design/tune the new calc on the same inputs. Legacy formula (CvCity::getYieldRate100,
		// verified): min(cap, max(100, (base + specialist) * modifier + 100 * extraYield)). NB `extraYield` is the
		// x1-TRUNCATED flat-outside term the formula actually uses (sub-100 precision is lost before the x100), so the
		// emulator must truncate likewise to match. Pilot channel = city yields; widens per the spec §4 channel order.
		if (strcmp(szAction, "cityInput") == 0)
		{
			o["city"] = picojson::value((double)iCityId);
			if (pCity == NULL)
			{
				o["error"] = picojson::value(std::string("no city"));
				return CvString(picojson::value(o).serialize().c_str());
			}
			o["cityName"] = picojson::value(std::string(narrowToAscii(pCity->getName()).GetCString()));
			o["population"] = picojson::value((double)pCity->getPopulation());
			o["cap"] = picojson::value((double)CITY_MAX_YIELD_RATE); // the getYieldRate100 clamp ceiling

			// the ACTIVE-building loadout (the cascade deposit source). Owner 2026-06-19: emit the ACTIVE set
			// (hasFullyActiveBuilding = present AND not resource/replacement-disabled AND not religiously-limited) so the
			// offline tester reads the live active set directly rather than re-deriving dormancy -- legacy's yield modifier
			// only includes active buildings, so this is the apples-to-apples deposit source. `dormant` is emitted apart
			// for observability (present-but-inactive). NB bands (e.g. education) are CUMULATIVE -- all active, all counted.
			picojson::value::array kBldgs, kDormant;
			for (int b = 0; b < GC.getNumBuildingInfos(); ++b)
			{
				if (!pCity->hasBuilding((BuildingTypes)b)) continue;
				const char* szT = GC.getBuildingInfo((BuildingTypes)b).getType();
				if (pCity->hasFullyActiveBuilding((BuildingTypes)b))
					kBldgs.push_back(picojson::value(std::string(szT)));
				else
					kDormant.push_back(picojson::value(std::string(szT)));
			}
			o["buildings"] = picojson::value(kBldgs);
			o["dormantBuildings"] = picojson::value(kDormant);

			// LOADOUT (owner 2026-06-19): the raw entity list the cascade calculator is fed -- techs/civics/buildings/plots.
			// (buildings above.) The calculator reads these + the Assets/Data JSON deposits and applies the cascade model,
			// then compares to legacy. techs = team-known; civics = player current; plots = the city's workable substrate.
			picojson::value::array kTechs;
			for (int t = 0; t < GC.getNumTechInfos(); ++t)
				if (GET_TEAM((TeamTypes)iTeam).isHasTech((TechTypes)t))
					kTechs.push_back(picojson::value(std::string(GC.getTechInfo((TechTypes)t).getType())));
			o["techs"] = picojson::value(kTechs);

			picojson::value::array kCivics;
			for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
			{
				const CivicTypes eCivic = kPlayer.getCivics((CivicOptionTypes)co);
				if (eCivic != NO_CIVIC)
					kCivics.push_back(picojson::value(std::string(GC.getCivicInfo(eCivic).getType())));
			}
			o["civics"] = picojson::value(kCivics);

			// player TRAITS (a production/yield deposit source the emulator must sum for parity, like civics).
			picojson::value::array kTraits;
			for (int t = 0; t < GC.getNumTraitInfos(); ++t)
				if (kPlayer.hasTrait((TraitTypes)t))
					kTraits.push_back(picojson::value(std::string(GC.getTraitInfo((TraitTypes)t).getType())));
			o["traits"] = picojson::value(kTraits);

			// resources = the city's AVAILABLE bonuses (vicinity + trade-connected; hasBonus). The loadout's plot
			// list only carries vicinity bonuses, but bonus-gated deposits often activate via trade -- so the
			// cascade calculator needs the full available set (calc-emulator-spec §2a). (CvBonusInfo.h incl. above.)
			picojson::value::array kBonuses;
			for (int b = 0; b < GC.getNumBonusInfos(); ++b)
				if (pCity->hasBonus((BonusTypes)b))
					kBonuses.push_back(picojson::value(std::string(GC.getBonusInfo((BonusTypes)b).getType())));
			o["resources"] = picojson::value(kBonuses);

			// city/player STATE flags the loadout needs for state-gated modifiers & conditions (owner 2026-06-19:
			// "you are not modelling for power = on"). Power follows the golden-age pattern -- a modifier gated by a
			// boolean toggle on top, not modelled at all today. (calc-emulator-spec §2a state booleans.)
			picojson::value::object kState;
			kState["isPowered"]   = picojson::value(pCity->isPower());
			kState["isCapital"]   = picojson::value(pCity->isCapital());
			kState["isGoldenAge"] = picojson::value(kPlayer.isGoldenAge());
			o["state"] = picojson::value(kState);

			picojson::value::array kPlots;
			for (int pi = 0; pi < pCity->getNumCityPlots(); ++pi)
			{
				const CvPlot* pPlot = pCity->getCityIndexPlot(pi);
				if (pPlot == NULL) continue;
				picojson::value::object pl;
				pl["i"]      = picojson::value((double)pi);
				pl["worked"] = picojson::value(pCity->isWorkingPlot(pi));
				const TerrainTypes eT = pPlot->getTerrainType();
				if (eT != NO_TERRAIN) pl["terrain"] = picojson::value(std::string(GC.getTerrainInfo(eT).getType()));
				const FeatureTypes eF = pPlot->getFeatureType();
				if (eF != NO_FEATURE) pl["feature"] = picojson::value(std::string(GC.getFeatureInfo(eF).getType()));
				const ImprovementTypes eI = pPlot->getImprovementType();
				if (eI != NO_IMPROVEMENT) pl["improvement"] = picojson::value(std::string(GC.getImprovementInfo(eI).getType()));
				const RouteTypes eR = pPlot->getRouteType();
				if (eR != NO_ROUTE) pl["route"] = picojson::value(std::string(GC.getRouteInfo(eR).getType()));
				const BonusTypes eB = pPlot->getBonusType((TeamTypes)iTeam);
				if (eB != NO_BONUS) pl["bonus"] = picojson::value(std::string(GC.getBonusInfo(eB).getType()));
				if (pPlot->isRiver()) pl["river"] = picojson::value(true);
				pl["yieldFood"]       = picojson::value((double)pPlot->getYield(YIELD_FOOD));
				pl["yieldProduction"] = picojson::value((double)pPlot->getYield(YIELD_PRODUCTION));
				pl["yieldCommerce"]   = picojson::value((double)pPlot->getYield(YIELD_COMMERCE));
				// per-plot legacy yield DECOMPOSITION (each cascade plot-component validates in isolation; no-guessing):
				// nature / improvement(base+riverside+irrigated+route+tech) / workingCity(buildings) / player terrainYieldChange + plot STATE.
				{
					const PlayerTypes ePlayerBY = pCity->getOwner();
					const CvCity* pWorkBY = pPlot->getWorkingCity();
					const char* aYN[3]={"natF","natP","natC"}; const char* aYI[3]={"impF","impP","impC"};
					const char* aYW[3]={"wcF","wcP","wcC"};    const char* aYT[3]={"terF","terP","terC"};
					const char* aYS[3]={"seaF","seaP","seaC"}; const char* aYC[3]={"ccF","ccP","ccC"};
					for (int yy=0; yy<3; ++yy)
					{
						const YieldTypes eYY=(YieldTypes)yy;
						const int nat=pPlot->calculateNatureYield(eYY,(TeamTypes)iTeam);
						const int imp=(eI!=NO_IMPROVEMENT)?pPlot->calculateImprovementYieldChange(eI,eYY,ePlayerBY):0;
						const int wcy=(pWorkBY!=NULL)?pWorkBY->getYieldChangeAt(pPlot,eYY):0;
						const int ter=(ePlayerBY!=NO_PLAYER&&eT!=NO_TERRAIN)?GET_PLAYER(ePlayerBY).getTerrainYieldChange(eT,eYY):0;
						// the components calculateYield adds that natC/impC/wcC/terC OMITTED (the unaccounted residual):
						// player SEA-plot yield (water tiles) + the city-CENTER bonus (getCityChange + pop/divisor).
						const int sea=(ePlayerBY!=NO_PLAYER&&pPlot->isWater())?GET_PLAYER(ePlayerBY).getSeaPlotYield(eYY):0;
						int cc=0;
						if (pPlot->isCity())
						{
							cc=GC.getYieldInfo(eYY).getCityChange();
							const int div=GC.getYieldInfo(eYY).getPopulationChangeDivisor();
							if (div!=0) cc+=pCity->getPopulation()/div;
						}
						if(nat) pl[aYN[yy]]=picojson::value((double)nat);
						if(imp) pl[aYI[yy]]=picojson::value((double)imp);
						if(wcy) pl[aYW[yy]]=picojson::value((double)wcy);
						if(ter) pl[aYT[yy]]=picojson::value((double)ter);
						if(sea) pl[aYS[yy]]=picojson::value((double)sea);
						if(cc)  pl[aYC[yy]]=picojson::value((double)cc);
					}
					if(pPlot->isIrrigationAvailable()) pl["irrig"]=picojson::value(true);
					if(pPlot->isRiverSide())          pl["riverside"]=picojson::value(true);
					if(pPlot->isHills())              pl["hills"]=picojson::value(true);
					if(pPlot->isPeak()||pPlot->isAsPeak()) pl["peak"]=picojson::value(true);
				}
				kPlots.push_back(picojson::value(pl));
			}
			o["plots"] = picojson::value(kPlots);
			// specialist ASSIGNMENT counts (engine state; cascade computes specialist yields = JSON values x these counts)
			{
				picojson::value::object specs;
				for (int s=0; s<GC.getNumSpecialistInfos(); ++s)
				{
					const int cnt=pCity->getSpecialistCount((SpecialistTypes)s)+pCity->getFreeSpecialistCount((SpecialistTypes)s);
					if(cnt>0) specs[GC.getSpecialistInfo((SpecialistTypes)s).getType()]=picojson::value((double)cnt);
				}
				o["specialists"]=picojson::value(specs);
			}

			// per-specialist COMMERCE breakdown -- the EXACT sources getSpecialistCommerce/getExtraSpecialistCommerce
			// sum (CvCity.cpp:12477-12488 + the x(1+pct) at 5167): intrinsic (SpecialistInfo.getCommerceChange),
			// perType (player getExtraSpecialistCommerce), local (city getLocalSpecialistExtraCommerce), all (player
			// getSpecialistExtraCommerce), pct (player getSpecialistCommercePercentChanges). So the spec/xspec gap
			// names its source per specialist, in EVERY city -- no manual in-game reads.
			{
				picojson::value::array sdet;
				CvPlayer& kP = GET_PLAYER((PlayerTypes)iPlayer);
				static const char* aCN[4] = { "gold", "research", "culture", "espionage" };
				for (int s=0; s<GC.getNumSpecialistInfos(); ++s)
				{
					const int cnt=pCity->getSpecialistCount((SpecialistTypes)s)+pCity->getFreeSpecialistCount((SpecialistTypes)s);
					if(cnt<=0) continue;
					const CvSpecialistInfo& si=GC.getSpecialistInfo((SpecialistTypes)s);
					picojson::value::object e;
					e["type"]=picojson::value(std::string(si.getType()));
					e["count"]=picojson::value((double)cnt);
					for(int c=0;c<4;++c)
					{
						const CommerceTypes eC=(CommerceTypes)c;
						const int intr=si.getCommerceChange(eC);
						const int pt=kP.getExtraSpecialistCommerce((SpecialistTypes)s,eC);
						const int lo=pCity->getLocalSpecialistExtraCommerce((SpecialistTypes)s,eC);
						const int al=kP.getSpecialistExtraCommerce(eC);
						const int pc=kP.getSpecialistCommercePercentChanges((SpecialistTypes)s,eC);
						if(intr||pt||lo||al||pc)
						{
							picojson::value::object cc;
							if(intr)cc["intrinsic"]=picojson::value((double)intr);
							if(pt)cc["perType"]=picojson::value((double)pt);
							if(lo)cc["local"]=picojson::value((double)lo);
							if(al)cc["all"]=picojson::value((double)al);
							if(pc)cc["pct"]=picojson::value((double)pc);
							e[aCN[c]]=picojson::value(cc);
						}
					}
					sdet.push_back(picojson::value(e));
				}
				o["specialistCommerceDetail"]=picojson::value(sdet);
			}

			// per-ACTIVE-corporation commerce breakdown (getCorporationCommerceByCorporation, CvCity.cpp:12606)
			// so the corp-output gap attributes to a named corp+family. Only active corps emit.
			{
				picojson::value::array cdet;
				for (int cp = 0; cp < GC.getNumCorporationInfos(); ++cp)
				{
					if (!pCity->isActiveCorporation((CorporationTypes)cp)) continue;
					picojson::value::object e;
					e["type"]=picojson::value(std::string(GC.getCorporationInfo((CorporationTypes)cp).getType()));
					static const char* aCN2[4] = { "gold", "research", "culture", "espionage" };
					for (int c=0;c<4;++c)
					{
						const int v=pCity->getCorporationCommerceByCorporation((CommerceTypes)c,(CorporationTypes)cp);
						if (v) e[aCN2[c]]=picojson::value((double)v);
					}
					cdet.push_back(picojson::value(e));
				}
				o["corporationCommerceDetail"]=picojson::value(cdet);
			}

			CvCascadeContext kCtx(iPlayer, iCityId);
			const int aFam[3] = { YIELD_FOOD, YIELD_PRODUCTION, YIELD_COMMERCE };
			const char* aFamName[3] = { "food", "production", "commerce" };
			picojson::value::array kYields;
			for (int f = 0; f < 3; ++f)
			{
				const YieldTypes eY = (YieldTypes)aFam[f];
				CvModifierSlot slot;
				cascadeModifierCitySlot(aFam[f], kCtx, slot);
				const int iBase = pCity->getBaseYieldRate(eY);
				picojson::value::object e;
				e["family"]        = picojson::value(std::string(aFamName[f]));
				// LEGACY input vector (the emulator reproduces getYieldRate100 from exactly these):
				e["base"]          = picojson::value((double)iBase);
				e["specialist"]    = picojson::value((double)pCity->getSpecialistYieldTotal(eY));
				e["modifier"]      = picojson::value((double)pCity->getBaseYieldRateModifier(eY)); // full % == 100 + sum%
				// MODIFIER BREAKDOWN (getBaseYieldRateModifier components, CvCity.cpp:11217) -- so the emulator
				// attributes the percent gap to the missing source (bonus/power/area/capital/player-trait), since
				// cascade_sim only sums building + civic %.
				e["modBonus"]    = picojson::value((double)pCity->getBonusYieldRateModifier(eY));
				e["modBuilding"] = picojson::value((double)pCity->getBuildingYieldModifier(eY));
				e["modPlayer"]   = picojson::value((double)kPlayer.getYieldRateModifier(eY));
				e["modEvent"]    = picojson::value((double)pCity->getYieldRateModifier(eY));
				e["modPower"]    = picojson::value((double)(pCity->isPower() ? pCity->getPowerYieldRateModifier(eY) : 0));
				e["modArea"]     = picojson::value((double)(pCity->area() != NULL ? pCity->area()->getYieldRateModifier(pCity->getOwner(), eY) : 0));
				e["modCapital"]  = picojson::value((double)(pCity->isCapital() ? kPlayer.getCapitalYieldRateModifier(eY) : 0));
				e["extraYield"]    = picojson::value((double)pCity->getExtraYield(eY));    // x1 TRUNCATED -- the term the formula uses
				e["extraYield100"] = picojson::value((double)pCity->getExtraYield100(eY)); // untruncated, for decompose
				// BASE decomposition (getBaseYieldRate, CvCity.cpp:22906) -- the additive base's named sub-sources:
				e["basePlotYield"]      = picojson::value((double)pCity->getPlotYield(eY));        // summed worked-plot yields
				e["baseTradeYield"]     = picojson::value((double)pCity->getTradeYield(eY));       // trade-route yield
				e["baseFreeCityYield"]  = picojson::value((double)kPlayer.getFreeCityYield(eY));   // player free-city yield
				e["baseGoldenAgeYield"] = picojson::value((double)(kPlayer.isGoldenAge() ? kPlayer.getGoldenAgeYield(eY) : 0)); // golden-age yield
				// EXTRA-bucket decomposition (getExtraYield100, CvCity.cpp:11323) -- flatExtra = extraYield100 - building - perPop×pop:
				e["extraBuildingYield100"] = picojson::value((double)pCity->getBuildingExtraYield100(eY)); // per-building flat ×100
				e["extraPerPopRate"]       = picojson::value((double)pCity->getBaseYieldPerPopRate(eY));   // per-pop rate (×pop in the bucket)
				// per-building BuildingYieldChange breakdown -- m_aBuildingYieldChange feeds m_aiExtraYield (the NON-building
				// part of the extra bucket: extraYield100 - extraBuildingYield100 - perPop, minus corp yield). Each entry is a
				// yield-change set by bonus / vicinity-bonus / vote-source / event (setBuildingYieldChange, CvCity.cpp:19187).
				// Emitted so the residual attributes to a NAMED building x source -- total observability, no guessing. x1.
				{
					picojson::value::object byc;
					for (int bb = 0; bb < GC.getNumBuildingInfos(); ++bb)
					{
						const int v = pCity->getBuildingYieldChange((BuildingTypes)bb, eY);
						if (v != 0) byc[GC.getBuildingInfo((BuildingTypes)bb).getType()] = picojson::value((double)v);
					}
					e["buildingYieldChange"] = picojson::value(byc);
				}
				// the OTHER m_aiExtraYield contributor: corp yield (getCorporationYield, CvCity.cpp:12556). With these two,
				// (extraYield100 - extraBuildingYield100)/100 == corporationYield + Σ buildingYieldChange -- fully attributable.
				e["corporationYield"] = picojson::value((double)pCity->getCorporationYield(eY));
				e["legacy100"]     = picojson::value((double)pCity->getYieldRate100(eY));  // ground truth (x100)
				// CASCADE seed (the current calc-flow, cascadeModifierApply):
				e["cascadeFlat"]    = picojson::value((double)slot.iFlat);
				e["cascadePercent"] = picojson::value((double)slot.iPercent);
				e["cascadeMult100"] = picojson::value((double)slot.iMultiplierX100);
				e["cascade"]        = picojson::value((double)cascadeModifierApply(slot, iBase));
				kYields.push_back(picojson::value(e));
			}
			o["yields"] = picojson::value(kYields);

			// PER-BUILDING legacy yield decomposition (calc-emulator-spec §5): each ACTIVE building's flat
			// contribution (getBaseYieldRateFromBuilding100, x100 = YieldChange*100 + perPop*pop + techChange +
			// dynamic) and its static percent (getYieldModifier) per yield, so the offline emulator ATTRIBUTES the
			// aggregate flat/percent divergence to NAMED buildings (compared vs the cascade's per-building JSON
			// deposit) instead of guessing. Active set only (legacy's modifier includes only active buildings);
			// buildings with zero contribution across all three yields are omitted to keep the dump lean.
			picojson::value::array kBldgYield;
			const int iPopBY = pCity->getPopulation();
			const TeamTypes eTeamBY = pCity->getTeam();
			const int aCom4[4] = { COMMERCE_GOLD, COMMERCE_RESEARCH, COMMERCE_CULTURE, COMMERCE_ESPIONAGE };
			const char* aYK[3][2] = { {"foodFlat100","foodPct"}, {"prodFlat100","prodPct"}, {"commFlat100","commPct"} };
			const char* aCK[4][2] = { {"goldFlat100","goldPct"}, {"resFlat100","resPct"}, {"culFlat100","culPct"}, {"espFlat100","espPct"} };
			for (int b = 0; b < GC.getNumBuildingInfos(); ++b)
			{
				if (!pCity->hasBuilding((BuildingTypes)b)) continue;
				if (!pCity->hasFullyActiveBuilding((BuildingTypes)b)) continue;
				const CvBuildingInfo& bi = GC.getBuildingInfo((BuildingTypes)b);
				picojson::value::object e;
				bool bAny = false;
				// YIELDS (flat100 + pct) -- always emitted (primary channel). flat mirrors processBuilding
				// (CvCity.cpp:4677): static YieldChange AND dynamic getBuildingYieldChange BOTH x100 + perPop*pop + tech.
				for (int f = 0; f < 3; ++f)
				{
					const YieldTypes eY = (YieldTypes)aFam[f];
					const int flat = (bi.getYieldChange(eY) + pCity->getBuildingYieldChange((BuildingTypes)b, eY)) * 100
					               + bi.getYieldPerPopChange(eY) * iPopBY
					               + GET_TEAM(eTeamBY).getBuildingYieldTechChange(eY, (BuildingTypes)b);
					const int pct = bi.getYieldModifier(eY);
					e[aYK[f][0]] = picojson::value((double)flat);
					e[aYK[f][1]] = picojson::value((double)pct);
					if (flat || pct) bAny = true;
				}
				// COMMERCE SPLIT (gold/research/culture/espionage) flat100 + pct -- only non-zero
				for (int c = 0; c < 4; ++c)
				{
					const CommerceTypes eC = (CommerceTypes)aCom4[c];
					const int flat = pCity->getBaseCommerceRateFromBuilding100(eC, (BuildingTypes)b);
					const int pct = bi.getCommerceModifier(eC);
					if (flat) { e[aCK[c][0]] = picojson::value((double)flat); bAny = true; }
					if (pct)  { e[aCK[c][1]] = picojson::value((double)pct);  bAny = true; }
					// CLEAN per-building commerce BASE: exactly what updateBuildingCommerce() sums into getBuildingCommerce
					// (same default args) so the Sum reconciles to bldgCommercePure100; returns 0 for an inactive building
					// (isActiveBuilding gate, CvCity.cpp:12148) -- i.e. the per-building view of the COMMERCE active-set.
					const int bldc = 100 * pCity->getBuildingCommerceByBuilding(eC, (BuildingTypes)b, false, false);
					static const char* aCKbld[4] = { "goldBld100", "resBld100", "culBld100", "espBld100" };
					if (bldc) { e[aCKbld[c]] = picojson::value((double)bldc); bAny = true; }
				}
				// HEALTH / HAPPINESS (realized per-building contribution) + FREE-XP
				const int iHe = pCity->getBuildingHealth((BuildingTypes)b);
				const int iHa = pCity->getBuildingHappiness((BuildingTypes)b);
				const int iXp = bi.getFreeExperience();
				if (iHe) { e["health"] = picojson::value((double)iHe); bAny = true; }
				if (iHa) { e["happiness"] = picojson::value((double)iHa); bAny = true; }
				if (iXp) { e["freeXp"] = picojson::value((double)iXp); bAny = true; }
				// PROPERTIES -- the building's per-turn property deposits ({PROPERTY_X: change})
				const CvProperties* pProps = bi.getProperties();
				if (pProps != NULL)
				{
					picojson::value::object kp;
					for (int pi = 0; pi < pProps->getNumProperties(); ++pi)
					{
						const PropertyTypes eP = pProps->getProperty(pi);
						const int iVal = pProps->getValue(pi);
						if (eP != NO_PROPERTY && iVal != 0)
							kp[GC.getPropertyInfo(eP).getType()] = picojson::value((double)iVal);
					}
					if (!kp.empty()) { e["properties"] = picojson::value(kp); bAny = true; }
				}
				if (!bAny) continue;
				e["type"] = picojson::value(std::string(bi.getType()));
				kBldgYield.push_back(picojson::value(e));
			}
			o["buildingYields"] = picojson::value(kBldgYield);

			// ---- CH.2 COMMERCE split (legacy-value-calc-map §2; reproduce getCommerceRateAtSliderPercent) ----
			// City-level inputs (shared by all four commerces) + the clamp consts the emulator needs:
			o["yieldCommerce100"] = picojson::value((double)pCity->getYieldRate100(YIELD_COMMERCE));
			o["prodRate"]         = picojson::value((double)pCity->getYieldRate(YIELD_PRODUCTION)); // x1, for prod->commerce
			o["isDisorder"]       = picojson::value(pCity->isDisorder());                           // disorder => all commerce 0
			o["maxYield100"]      = picojson::value((double)CITY_MAX_YIELD_RATE100);                // pre-modifier clamp
			o["minTolFalseAccum"] = picojson::value((double)MIN_TOL_FALSE_ACCUMULATE);              // very-negative -> cap sentinel
			const int aCom[4] = { COMMERCE_GOLD, COMMERCE_RESEARCH, COMMERCE_CULTURE, COMMERCE_ESPIONAGE };
			const char* aComName[4] = { "gold", "research", "culture", "espionage" };
			picojson::value::array kCommerce;
			for (int c = 0; c < 4; ++c)
			{
				const CommerceTypes eC = (CommerceTypes)aCom[c];
				picojson::value::object e;
				e["family"]         = picojson::value(std::string(aComName[c]));
				e["slider"]         = picojson::value((double)kPlayer.getCommercePercent(eC));       // player slider %
				// BASE-EXTRA decomposition (getBaseCommerceRateExtra, x100; §2) -- the named components, at STRUCTURAL
				// PARITY with the 3 base yields (the per-source breakdown the thin slider/total/realized triple lacked).
				e["baseExtra100"]            = picojson::value((double)pCity->getBaseCommerceRateExtra(eC));  // realized x100
				e["specialistCommerce"]      = picojson::value((double)pCity->getSpecialistCommerce(eC));       // x1
				e["extraSpecialistCommerce"] = picojson::value((double)pCity->getExtraSpecialistCommerceTotal(eC)); // x1
				// 3-way split of extraSpecialistCommerce (getExtraSpecialistCommerce, CvCity.cpp:11819 = count x
				// (LOCAL building-per-type + PLAYER-per-type + PLAYER-all-type)) -- names which part the calc misses:
				{
					int iLocal = 0, iPerType = 0, iTotalSpec = 0;
					for (int s = 0; s < GC.getNumSpecialistInfos(); ++s)
					{
						const int cnt = pCity->specialistCount((SpecialistTypes)s);
						if (cnt <= 0) continue;
						iTotalSpec += cnt;
						iLocal   += cnt * pCity->getLocalSpecialistExtraCommerce((SpecialistTypes)s, eC);
						iPerType += cnt * kPlayer.getExtraSpecialistCommerce((SpecialistTypes)s, eC);
					}
					e["xspecLocal"]   = picojson::value((double)iLocal);                                  // building per-type
					e["xspecPerType"] = picojson::value((double)iPerType);                                // civic/trait per-type
					e["xspecAll"]     = picojson::value((double)(iTotalSpec * kPlayer.getSpecialistExtraCommerce(eC))); // all-type x count
				}
				e["religionCommerce"]        = picojson::value((double)pCity->getReligionCommerce(eC));         // x1
				{	// per-religion decomposition -- so the religion-commerce gap names the religion (holy/state)
					picojson::value::object relC;
					for (int r = 0; r < GC.getNumReligionInfos(); ++r)
					{
						const int v = pCity->getReligionCommerceByReligion(eC, (ReligionTypes)r);
						if (v) relC[GC.getReligionInfo((ReligionTypes)r).getType()] = picojson::value((double)v);
					}
					e["religionCommerceByType"] = picojson::value(relC);
				}
				e["corporationCommerce"]     = picojson::value((double)pCity->getCorporationCommerce(eC));      // x1
				e["buildingCommerce100"]     = picojson::value((double)pCity->getBuildingCommerce100(eC));      // x100 (aggregate; 4-way split below)
				// buildingCommerce100 4-way decomposition (getBuildingCommerce100, CvCity.cpp:12131) -- so the missing
				// building-commerce sub-source names itself (e.g. the espionage -80 = a bonus/tech/perPop sub-source):
				e["bldgCommercePure100"]   = picojson::value((double)(100 * pCity->getBuildingCommerce(eC)));    // pure per-building flat ×100
				e["bldgCommerceBonus100"]  = picojson::value((double)pCity->getBonusCommercePercentChanges(eC)); // bonus-gated building commerce ×100
				e["bldgCommerceTech100"]   = picojson::value((double)pCity->getBuildingCommerceTechChange(eC));  // tech-gated building commerce ×100
				e["bldgCommercePerPop100"] = picojson::value((double)(pCity->getCommercePerPopFromBuildings(eC) * pCity->getPopulation())); // per-pop building commerce ×100
				e["mintedCommerce100"]     = picojson::value((double)(eC == COMMERCE_GOLD ? pCity->getMintedCommerceTimes100() : 0)); // gold only ×100
				e["goldenAgeCommerce"]     = picojson::value((double)(kPlayer.isGoldenAge() ? kPlayer.getGoldenAgeCommerce(eC) : 0));  // x1, golden-age base commerce
				e["stateReligionBuildingCommerce"] = picojson::value((double)kPlayer.getStateReligionBuildingCommerce(eC)); // x1
				e["playerExtraCommerce100"]  = picojson::value((double)kPlayer.getExtraCommerce100(eC));        // x100
				// MODIFIER decomposition (getTotalCommerceRateModifier, base 100; §2 -- event/from-buildings are added
				// then subtracted into modPlayer, the double-count the emulator must mirror) -- at parity with the yield mods:
				e["totalModifier"]   = picojson::value((double)pCity->getTotalCommerceRateModifier(eC)); // realized base 100
				e["modBonus"]        = picojson::value((double)pCity->getBonusCommerceRateModifier(eC)); // city, bonus-sourced
				e["modBuilding"]     = picojson::value((double)pCity->getBuildingCommerceModifier(eC));  // city, building-sourced
				e["modCity"]         = picojson::value((double)pCity->getCommerceRateModifier(eC));      // city own
				e["modPlayer"]       = picojson::value((double)kPlayer.getCommerceRateModifier(eC));     // player (incl. the - subtractions)
				e["modEvent"]        = picojson::value((double)kPlayer.getCommerceRateModifierfromEvents(eC));    // player, event
				e["modFromBuildings"]= picojson::value((double)kPlayer.getCommerceRateModifierfromBuildings(eC)); // player, from-buildings
				e["modCapital"]      = picojson::value((double)(pCity->isCapital() ? kPlayer.getCapitalCommerceRateModifier(eC) : 0));
				e["prodToCommerce"]  = picojson::value((double)pCity->getProductionToCommerceModifier(eC));
				e["realized100"]     = picojson::value((double)pCity->getCommerceRateTimes100(eC));   // ground truth (x100)
				// per-BONUS decomposition (audit): modBonus aggregates getBonusCommerceModifier(bonus,eC) over the city's
				// bonuses; mintedCommerce100 (gold) aggregates count×getBonusMintedPercent(bonus). Emit the per-bonus split:
				{
					picojson::value::object kBonusMod, kBonusMint;
					for (int bz = 0; bz < GC.getNumBonusInfos(); ++bz)
					{
						const int cnt = pCity->getNumBonuses((BonusTypes)bz);
						if (cnt <= 0) continue;
						const char* bn = GC.getBonusInfo((BonusTypes)bz).getType();
						const int bm = kPlayer.getBonusCommerceModifier((BonusTypes)bz, eC);
						if (bm) kBonusMod[bn] = picojson::value((double)bm);
						if (eC == COMMERCE_GOLD)
						{
							const int mp = kPlayer.getBonusMintedPercent((BonusTypes)bz);
							if (mp) kBonusMint[bn] = picojson::value((double)(cnt * mp));
						}
					}
					if (!kBonusMod.empty())  e["bonusCommerceModByBonus"] = picojson::value(kBonusMod);
					if (!kBonusMint.empty()) e["mintedByBonus"]           = picojson::value(kBonusMint);
				}
				kCommerce.push_back(picojson::value(e));
			}
			o["commerce"] = picojson::value(kCommerce);

			// per-religion city count (countReligionLevels) -- the global count a SHRINE building scales its commerce
			// by: religion.shrine.{commerce} x countReligionLevels(religion) at world scope (the #430 shrine assembly).
			// Emitted so the offline emulator can reconstruct shrine commerce (the count is engine state, not in JSON).
			{
				picojson::value::object kRel;
				for (int r = 0; r < GC.getNumReligionInfos(); ++r)
				{
					const int iLevels = GC.getGame().countReligionLevels((ReligionTypes)r);
					if (iLevels != 0)
						kRel[GC.getReligionInfo((ReligionTypes)r).getType()] = picojson::value((double)iLevels);
				}
				o["religionLevels"] = picojson::value(kRel);
			}

			// ---- CH.4 DEFENSE (legacy-value-calc-map §4): max(building,natural)+playerMod+bonus, then damage-decay floored at extraMin ----
			{
				picojson::value::object d;
				d["totalDefense"]              = picojson::value((double)pCity->getTotalDefense(false));
				d["defenseModifier"]           = picojson::value((double)pCity->getDefenseModifier(false)); // realized
				d["buildingDefense"]           = picojson::value((double)pCity->getBuildingDefense());
				d["naturalDefense"]            = picojson::value((double)pCity->getNaturalDefense());
				d["playerCityDefenseModifier"] = picojson::value((double)kPlayer.getCityDefenseModifier()); // aggregate (split below)
				d["playerExtraCityDefense"]    = picojson::value((double)kPlayer.getExtraCityDefense());      // leaf of getCityDefenseModifier
				d["playerTraitExtraCityDefense"]= picojson::value((double)kPlayer.getTraitExtraCityDefense()); // leaf of getCityDefenseModifier
				d["bonusDefense"]              = picojson::value((double)pCity->calculateBonusDefense()); // aggregate (per-bonus below)
				{
					picojson::value::object kBD;
					for (int bz = 0; bz < GC.getNumBonusInfos(); ++bz)
					{
						if (pCity->getNumBonuses((BonusTypes)bz) <= 0) continue;
						const int bd = pCity->getBonusDefenseChanges((BonusTypes)bz);
						if (bd) kBD[GC.getBonusInfo((BonusTypes)bz).getType()] = picojson::value((double)bd);
					}
					if (!kBD.empty()) d["bonusDefenseByBonus"] = picojson::value(kBD);
				}
				d["defenseDamage"]             = picojson::value((double)pCity->getDefenseDamage());
				d["maxDefenseDamage"]          = picojson::value((double)GC.getMAX_CITY_DEFENSE_DAMAGE());
				d["extraMinDefense"]           = picojson::value((double)pCity->getExtraMinDefense());
				d["minimumDefenseLevel"]       = picojson::value((double)pCity->getMinimumDefenseLevel()); // REALISTIC_SIEGE-gated floor (§4)
				d["minimumDefenseLevelRaw"]    = picojson::value((double)pCity->getMinimumDefenseLevelRaw()); // ungated raw m_iMinimumDefenseLevel
				d["cultureLevel"]              = picojson::value((double)pCity->getCultureLevel());     // naturalDefense picks this level's cityDefenseModifier
				d["realisticSiege"]            = picojson::value(GC.getGame().isOption(GAMEOPTION_COMBAT_REALISTIC_SIEGE)); // gates minimumDefenseLevel
				d["isOccupation"]              = picojson::value(pCity->isOccupation());
				o["defense"] = picojson::value(d);
			}

			// ---- CH.5 MAINTENANCE + UPKEEP (legacy-value-calc-map §5): era baseline + getModifiedIntValue(baseComponents, effectiveModifier) ----
			{
				picojson::value::object m;
				m["maintenanceTimes100"] = picojson::value((double)pCity->getMaintenanceTimes100()); // realized x100
				m["eraInitialPercent"]   = picojson::value((double)GC.getEraInfo(kPlayer.getCurrentEra()).getInitialCityMaintenancePercent());
				m["baseMaint100"]        = picojson::value((double)pCity->calculateBaseMaintenanceTimes100());
				m["buildingMaint100"]    = picojson::value((double)pCity->calculateBuildingMaintenanceTimes100());
				m["distanceMaint100"]    = picojson::value((double)pCity->calculateDistanceMaintenanceTimes100());
				m["numCitiesMaint100"]   = picojson::value((double)pCity->calculateNumCitiesMaintenanceTimes100());
				m["colonyMaint100"]      = picojson::value((double)pCity->calculateColonyMaintenanceTimes100());
				m["corporationMaint100"] = picojson::value((double)pCity->calculateCorporationMaintenanceTimes100());
				m["effectiveModifier"]   = picojson::value((double)pCity->getEffectiveMaintenanceModifier()); // aggregate (split below)
				// effectiveModifier split (getEffectiveMaintenanceModifier, CvCity.cpp:7590): city + player + area + connected:
				m["maintModCity"]      = picojson::value((double)pCity->getMaintenanceModifier());
				m["maintModPlayer"]    = picojson::value((double)kPlayer.getMaintenanceModifier());
				m["maintModArea"]      = picojson::value((double)(pCity->area() != NULL ? pCity->area()->getTotalAreaMaintenanceModifier(pCity->getOwner()) : 0));
				m["maintModConnected"] = picojson::value((double)((pCity->isConnectedToCapital() && !pCity->isCapital()) ? kPlayer.getConnectedCityMaintenanceModifier() : 0));
				// the embedded modifiers/gates/scalers INSIDE the calculate*MaintenanceTimes100 base components
				// (audit found these hidden in the base totals -- emit so each base component is reproducible):
				m["maintModifierApplied"] = picojson::value(!(pCity->isDisorder() || pCity->getPopulation() == 0)); // gate: else base stays era-only
				m["isRebel"]              = picojson::value(kPlayer.isRebel());          // halves distance/numCities/colony/corp bases
				m["isCoastalMaint"]       = picojson::value(pCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize())); // distance coastal mod gate
				m["playerDistanceMaintMod"]= picojson::value((double)kPlayer.getDistanceMaintenanceModifier());
				m["playerCoastalDistanceMaintMod"] = picojson::value((double)kPlayer.getCoastalDistanceMaintenanceModifier());
				m["playerNumCitiesMaintMod"]= picojson::value((double)kPlayer.getNumCitiesMaintenanceModifier());
				m["playerCorpMaintMod"]   = picojson::value((double)kPlayer.getCorporationMaintenanceModifier());
				m["teamCorpMaintMod"]     = picojson::value((double)GET_TEAM(pCity->getTeam()).getCorporationMaintenanceModifier());
				m["playerHomeAreaMaintMod"]= picojson::value((double)kPlayer.getHomeAreaMaintenanceModifier());
				m["playerOtherAreaMaintMod"]= picojson::value((double)kPlayer.getOtherAreaMaintenanceModifier());
				m["areaIsHomeArea"]       = picojson::value(pCity->area() != NULL && pCity->area()->isHomeArea(pCity->getOwner())); // home vs other area path
				m["areaMaintenanceModifier"]= picojson::value((double)(pCity->area() != NULL ? pCity->area()->getMaintenanceModifier(pCity->getOwner()) : 0)); // area base mod
				m["worldDistanceMaintPct"]= picojson::value((double)GC.getWorldInfo(GC.getMap().getWorldSize()).getDistanceMaintenancePercent());
				m["worldColonyMaintPct"]  = picojson::value((double)GC.getWorldInfo(GC.getMap().getWorldSize()).getColonyMaintenancePercent());
				m["worldCorpMaintPct"]    = picojson::value((double)GC.getWorldInfo(GC.getMap().getWorldSize()).getCorporationMaintenancePercent());
				m["handicapDistanceMaintPct"]= picojson::value((double)GC.getHandicapInfo(pCity->getHandicapType()).getDistanceMaintenancePercent());
				m["handicapNumCitiesMaintPct"]= picojson::value((double)GC.getHandicapInfo(pCity->getHandicapType()).getNumCitiesMaintenancePercent());
				m["handicapColonyMaintPct"]= picojson::value((double)GC.getHandicapInfo(pCity->getHandicapType()).getColonyMaintenancePercent());
				m["handicapCorpMaintPct"] = picojson::value((double)GC.getHandicapInfo(pCity->getHandicapType()).getCorporationMaintenancePercent());
				// deeper calculate*MaintenanceTimes100 internals (audit batch-2):
				m["treatNegativeGoldAsMaintenance"] = picojson::value(GC.getTREAT_NEGATIVE_GOLD_AS_MAINTENANCE() != 0); // building-maint gate
				m["baseDistanceMaintPer100Plots"]   = picojson::value((double)GC.getBASE_DISTANCE_MAINTENANCE_PER_100_PLOTS()); // distance coefficient
				m["isGovernmentCenter"]   = picojson::value(pCity->isGovernmentCenter());
				m["optNoVassalStates"]    = picojson::value(GC.getGame().isOption(GAMEOPTION_NO_VASSAL_STATES));         // gates colony maint
				m["optAdvancedRealisticCorporations"] = picojson::value(GC.getGame().isOption(GAMEOPTION_ADVANCED_REALISTIC_CORPORATIONS)); // corp handicap squaring
				m["citiesPerPlayerInArea"]= picojson::value((double)(pCity->area() != NULL ? pCity->area()->getCitiesPerPlayer(pCity->getOwner()) : 0)); // colony maint multiplier
				m["playerNumCities"]      = picojson::value((double)kPlayer.getNumCities());                 // numCities maint input (iCities = numCities - 1)
				m["handicapMaxColonyMaintenance"] = picojson::value((double)GC.getHandicapInfo(pCity->getHandicapType()).getMaxColonyMaintenance()); // colony maint cap
				{
					const CvCity* pCap = kPlayer.getCapitalCity();
					m["distanceToCapital"] = picojson::value((double)(pCap != NULL ? plotDistance(pCity->getX(), pCity->getY(), pCap->getX(), pCap->getY()) : 0));
					m["capitalInSameArea"] = picojson::value(pCap != NULL && pCap->area() == pCity->area());
				}
				// vassal numCities-maintenance inputs (per-vassal getNumCities + distinct vassal count):
				{
					int iVassalCities = 0, iDistinctVassals = 0;
					for (int vt = 0; vt < MAX_PC_TEAMS; ++vt)
					{
						if (vt == pCity->getTeam()) continue;
						if (GET_TEAM((TeamTypes)vt).isAlive() && GET_TEAM((TeamTypes)vt).isVassal(pCity->getTeam()))
						{
							++iDistinctVassals;
							for (int pl = 0; pl < MAX_PC_PLAYERS; ++pl)
								if (GET_PLAYER((PlayerTypes)pl).isAlive() && GET_PLAYER((PlayerTypes)pl).getTeam() == (TeamTypes)vt)
									iVassalCities += GET_PLAYER((PlayerTypes)pl).getNumCities();
						}
					}
					m["vassalCityCount"]  = picojson::value((double)iVassalCities);
					m["distinctVassals"]  = picojson::value((double)iDistinctVassals);
				}
				// per-corporation maintenance internals (HQ commerce + prereq bonuses × city bonus counts):
				{
					picojson::value::array kCorp;
					for (int cp = 0; cp < GC.getNumCorporationInfos(); ++cp)
					{
						if (!pCity->isActiveCorporation((CorporationTypes)cp)) continue;
						const CvCorporationInfo& kC = GC.getCorporationInfo((CorporationTypes)cp);
						picojson::value::object co;
						co["corp"] = picojson::value(std::string(kC.getType()));
						int iHq = 0;
						for (int cm = 0; cm < NUM_COMMERCE_TYPES; ++cm) iHq += kC.getHeadquarterCommerce(cm);
						co["hqCommerce"] = picojson::value((double)iHq);
						co["baseMaintenance"] = picojson::value((double)kC.getMaintenance()); // per-prereq-bonus base maint
						picojson::value::object kpb;
						const std::vector<BonusTypes>& vpb = kC.getPrereqBonuses();
						for (size_t i = 0; i < vpb.size(); ++i)
							if (vpb[i] != NO_BONUS)
								kpb[GC.getBonusInfo(vpb[i]).getType()] = picojson::value((double)pCity->getNumBonuses(vpb[i]));
						if (!kpb.empty()) co["prereqBonusCounts"] = picojson::value(kpb);
						kCorp.push_back(picojson::value(co));
					}
					if (!kCorp.empty()) m["corporations"] = picojson::value(kCorp);
				}
				m["isWeLoveTheKingDay"]  = picojson::value(pCity->isWeLoveTheKingDay());
				o["maintenance"] = picojson::value(m);
				o["civicUpkeep"]     = picojson::value((double)kPlayer.getCivicUpkeep(false));            // player upkeep (x1)
				o["finalUnitUpkeep"] = picojson::value((double)(int)kPlayer.getFinalUnitUpkeep());        // player unit upkeep (x1)
			}

			// ---- CH.6 GROWTH + foodKept (legacy-value-calc-map §9.3) ----
			{
				picojson::value::object g;
				g["foodProduced"]          = picojson::value((double)pCity->getYieldRate(YIELD_FOOD));
				g["foodConsumption"]       = picojson::value((double)pCity->foodConsumption());
				g["foodDifference"]        = picojson::value((double)pCity->foodDifference());   // realized
				g["food"]                  = picojson::value((double)pCity->getFood());
				g["growthThreshold"]       = picojson::value((double)pCity->growthThreshold());  // realized
				g["playerGrowthThreshold"] = picojson::value((double)kPlayer.getGrowthThreshold(pCity->getPopulation()));
				g["popGrowthRatePct"]      = picojson::value((double)(pCity->getPopulationgrowthratepercentage() + kPlayer.getPopulationgrowthratepercentage()));
				g["foodKeptPercent"]       = picojson::value((double)pCity->getFoodKeptPercent());
				g["foodKept"]              = picojson::value((double)pCity->getFoodKept());
				g["isHominid"]             = picojson::value(pCity->isHominid());
				g["foodWastage"]           = picojson::value((double)pCity->foodWastage());              // surplus waste in foodConsumption
				g["foodConsumedByPopulation"] = picojson::value((double)pCity->getFoodConsumedByPopulation()); // to reverse foodConsumption
				g["isDisorder"]            = picojson::value(pCity->isDisorder());          // foodDifference early-returns 0
				g["isFoodProduction"]      = picojson::value(pCity->isFoodProduction());    // foodDifference uses min(0,·) branch
				g["healthRateForFood"]     = picojson::value((double)pCity->healthRate());  // feeds foodConsumption
				// food-consumption intermediates + the growth/wastage DEFINES (the leaf constants behind the formulas):
				g["populationPlusProgress100"] = picojson::value((double)pCity->getPopulationPlusProgress100(0));
				g["foodConsumedPerPopulation100"] = picojson::value((double)pCity->getFoodConsumedPerPopulation100(0));
				g["foodConsumptionPerPopulation"] = picojson::value((double)GC.getFOOD_CONSUMPTION_PER_POPULATION());
				g["foodConsumptionPerPopulationPercent"] = picojson::value((double)GC.getFOOD_CONSUMPTION_PER_POPULATION_PERCENT());
				g["baseCityGrowthThreshold"] = picojson::value((double)GC.getDefineINT("BASE_CITY_GROWTH_THRESHOLD"));
				g["cityGrowthMultiplier"]    = picojson::value((double)GC.getDefineINT("CITY_GROWTH_MULTIPLIER"));
				g["eraGrowthPercent"]        = picojson::value((double)GC.getEraInfo(kPlayer.getCurrentEra()).getGrowthPercent());
				g["handicapAIGrowthPercent"] = picojson::value((double)GC.getHandicapInfo(kPlayer.getHandicapType()).getAIGrowthPercent());
				g["goldenAgePercentLessFoodForGrowth"] = picojson::value((double)GC.getDefineINT("GOLDEN_AGE_PERCENT_LESS_FOOD_FOR_GROWTH"));
				g["wastageStartConsumptionPercent"] = picojson::value((double)GC.getWASTAGE_START_CONSUMPTION_PERCENT());
				g["wastageGrowthFactor"]     = picojson::value((double)GC.getWASTAGE_GROWTH_FACTOR());
				g["gameSpeedGrowthPercent"]  = picojson::value((double)GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getSpeedPercent()); // growth-threshold scaler
				g["isNormalAI"]              = picojson::value(kPlayer.isNormalAI());        // gates AI growth modifier
				g["aiPerEraModifier"]        = picojson::value((double)GC.getHandicapInfo(GC.getGame().getHandicapType()).getAIPerEraModifier()); // AI per-era growth penalty
				g["currentEra"]              = picojson::value((double)kPlayer.getCurrentEra());   // scales aiPerEraModifier
				g["isGoldenAge"]             = picojson::value(kPlayer.isGoldenAge());       // gates golden-age growth bonus
				o["growth"] = picojson::value(g);
			}

			// ---- CH.3a HEALTH (legacy-value-calc-map §3): good/bad signed-split (emulator splits each via max/min 0) ----
			{
				picojson::value::object h;
				h["goodHealth"]              = picojson::value((double)pCity->goodHealth());   // realized
				h["badHealth"]               = picojson::value((double)pCity->badHealth());    // realized
				h["healthRate"]              = picojson::value((double)pCity->healthRate());
				h["freshWaterGoodHealth"]    = picojson::value((double)pCity->getFreshWaterGoodHealth());
				h["featureGoodHealth"]       = picojson::value((double)pCity->getFeatureGoodHealth());
				h["featureBadHealth"]        = picojson::value((double)pCity->getFeatureBadHealth());
				h["bonusGoodHealth"]         = picojson::value((double)pCity->getBonusGoodHealth());
				h["bonusBadHealth"]          = picojson::value((double)pCity->getBonusBadHealth());
				h["totalGoodBuildingHealth"] = picojson::value((double)pCity->totalGoodBuildingHealth()); // aggregate (city+area+player)
				h["totalBadBuildingHealth"]  = picojson::value((double)pCity->totalBadBuildingHealth());  // aggregate
				// building-health SCOPE split (§7 parallel city/area/player accumulators):
				h["cityBuildingGoodHealth"]   = picojson::value((double)pCity->getBuildingGoodHealth());
				h["cityBuildingBadHealth"]    = picojson::value((double)pCity->getBuildingBadHealth());
				h["areaBuildingGoodHealth"]   = picojson::value((double)(pCity->area() != NULL ? pCity->area()->getBuildingGoodHealth(pCity->getOwner()) : 0));
				h["areaBuildingBadHealth"]    = picojson::value((double)(pCity->area() != NULL ? pCity->area()->getBuildingBadHealth(pCity->getOwner()) : 0));
				h["playerBuildingGoodHealth"] = picojson::value((double)kPlayer.getBuildingGoodHealth());
				h["playerBuildingBadHealth"]  = picojson::value((double)kPlayer.getBuildingBadHealth());
				h["extraBuildingGoodHealth"]  = picojson::value((double)pCity->getExtraBuildingGoodHealth()); // in totalGoodBuildingHealth aggregate
				h["extraBuildingBadHealth"]   = picojson::value((double)pCity->getExtraBuildingBadHealth());  // in totalBadBuildingHealth aggregate
				h["isBuildingOnlyHealthy"]    = picojson::value(pCity->isBuildingOnlyHealthy());    // gates totalBadBuildingHealth to 0
				h["isNoUnhealthyPopulation"]  = picojson::value(pCity->isNoUnhealthyPopulation());  // gates unhealthyPopulation to 0
				h["extraHealth"]             = picojson::value((double)pCity->getExtraHealth());
				h["improvementGoodHealth"]   = picojson::value((double)pCity->getImprovementGoodHealth());
				h["improvementBadHealth"]    = picojson::value((double)pCity->getImprovementBadHealth());
				h["specialistGoodHealth"]    = picojson::value((double)pCity->getSpecialistGoodHealth());
				h["specialistBadHealth"]     = picojson::value((double)pCity->getSpecialistBadHealth());
				h["corporationHealth"]       = picojson::value((double)pCity->calculateCorporationHealth());
				h["extraTechHealth"]         = picojson::value((double)pCity->getExtraTechHealthTotal());
				h["espionageHealthCounter"]  = picojson::value((double)pCity->getEspionageHealthCounter());
				h["unhealthyPopulation"]     = picojson::value((double)pCity->unhealthyPopulation());
				h["populationHealth"]        = picojson::value((double)pCity->calculatePopulationHealth()); // per-pop health component (in good/bad)
				// omitted-bucket sources (signed; the goodHealth/badHealth residual closers):
				h["handicapHealth"]          = picojson::value((double)GC.getHandicapInfo(pCity->getHandicapType()).getHealthBonus());
				h["playerExtraHealth"]       = picojson::value((double)kPlayer.getExtraHealth());
				h["playerCivicHealth"]       = picojson::value((double)kPlayer.getCivicHealth());
				h["playerCivilizationHealth"]= picojson::value((double)kPlayer.getCivilizationHealth());
				h["playerWorldHealth"]       = picojson::value((double)kPlayer.getWorldHealth());
				h["playerProjectHealth"]     = picojson::value((double)kPlayer.getProjectHealth());
				o["health"] = picojson::value(h);
			}

			// ---- CH.3b HAPPINESS (legacy-value-calc-map §3): good/bad + percent-anger × pop / divisor ----
			{
				picojson::value::object hp;
				hp["happyLevel"]            = picojson::value((double)pCity->happyLevel());   // realized
				hp["unhappyLevel"]          = picojson::value((double)pCity->unhappyLevel()); // realized
				hp["angryPopulation"]       = picojson::value((double)pCity->angryPopulation());
				hp["buildingGoodHappiness"] = picojson::value((double)pCity->getBuildingGoodHappiness());
				hp["buildingBadHappiness"]  = picojson::value((double)pCity->getBuildingBadHappiness());
				hp["bonusGoodHappiness"]    = picojson::value((double)pCity->getBonusGoodHappiness());
				hp["bonusBadHappiness"]     = picojson::value((double)pCity->getBonusBadHappiness());
				hp["featureGoodHappiness"]  = picojson::value((double)pCity->getFeatureGoodHappiness());
				hp["featureBadHappiness"]   = picojson::value((double)pCity->getFeatureBadHappiness());
				hp["religionGoodHappiness"] = picojson::value((double)pCity->getReligionGoodHappiness());
				hp["religionBadHappiness"]  = picojson::value((double)pCity->getReligionBadHappiness());
				hp["militaryHappiness"]     = picojson::value((double)pCity->getMilitaryHappiness());
				hp["celebrityHappiness"]    = picojson::value((double)pCity->getCelebrityHappiness()); // unit-derived (§10.4 "must be dumped")
				hp["commerceHappiness"]     = picojson::value((double)pCity->getCommerceHappiness());
				hp["stateReligionHappiness"]= picojson::value((double)pCity->getCurrentStateReligionHappiness());
				hp["specialistHappiness"]   = picojson::value((double)pCity->getSpecialistHappiness());
				hp["specialistUnhappiness"] = picojson::value((double)pCity->getSpecialistUnhappiness());
				hp["largestCityHappiness"]  = picojson::value((double)pCity->getLargestCityHappiness());
				hp["extraHappiness"]        = picojson::value((double)pCity->getExtraHappiness());
				// anger-percent sources (sum × pop / PERCENT_ANGER_DIVISOR):
				hp["overcrowdingAnger"]     = picojson::value((double)pCity->getOvercrowdingPercentAnger());
				hp["noMilitaryAnger"]       = picojson::value((double)pCity->getNoMilitaryPercentAnger());
				hp["cultureAnger"]          = picojson::value((double)pCity->getCulturePercentAnger());
				hp["religionAnger"]         = picojson::value((double)pCity->getReligionPercentAnger());
				hp["hurryAnger"]            = picojson::value((double)pCity->getHurryPercentAnger());
				hp["conscriptAnger"]        = picojson::value((double)pCity->getConscriptPercentAnger());
				hp["warWearinessAnger"]     = picojson::value((double)pCity->getWarWearinessPercentAnger());
				hp["revIndexAnger"]         = picojson::value((double)pCity->getRevIndexPercentAnger());
				hp["percentAngerDivisor"]   = picojson::value((double)GC.getPERCENT_ANGER_DIVISOR());
				// omitted good-side buckets (the happyLevel residual closers; each max(0,·) in the engine):
				hp["revSuccessHappiness"]       = picojson::value((double)pCity->getRevSuccessHappiness());
				hp["extraBuildingGoodHappiness"]= picojson::value((double)pCity->getExtraBuildingGoodHappiness());
				hp["extraBuildingBadHappiness"] = picojson::value((double)pCity->getExtraBuildingBadHappiness());
				hp["areaBuildingHappiness"]     = picojson::value((double)(pCity->area() != NULL ? pCity->area()->getBuildingHappiness(pCity->getOwner()) : 0));
				hp["playerBuildingHappiness"]   = picojson::value((double)kPlayer.getBuildingHappiness());
				hp["playerExtraHappiness"]      = picojson::value((double)kPlayer.getExtraHappiness());
				hp["handicapHappy"]             = picojson::value((double)GC.getHandicapInfo(pCity->getHandicapType()).getHappyBonus());
				hp["vassalHappiness"]           = picojson::value((double)pCity->getVassalHappiness());
				hp["civicHappiness"]            = picojson::value((double)pCity->getCivicHappiness());
				hp["playerWorldHappiness"]      = picojson::value((double)kPlayer.getWorldHappiness());
				hp["playerProjectHappiness"]    = picojson::value((double)kPlayer.getProjectHappiness());
				hp["corporationHappiness"]      = picojson::value((double)pCity->calculateCorporationHappiness());
				hp["extraTechHappiness"]        = picojson::value((double)pCity->getExtraTechHappinessTotal());
				hp["happinessTimer"]            = picojson::value((double)pCity->getHappinessTimer());
				hp["tempHappy"]                 = picojson::value((double)GC.getTEMP_HAPPY());
				// the REMAINING anger-percent sources (legacy-value-calc-map §12 add-list -- the unhappyLevel residual
				// closers that make the percent-anger sum fully reproducible): the per-civic anger sum (the big one),
				// defy/revRequest/event timers, tax-rate + foreign culture unhappiness, landmark anger, city-over-limit.
				int iCivicAnger = 0;
				for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
				{
					const CivicTypes eCivicA = kPlayer.getCivics((CivicOptionTypes)co);
					if (eCivicA != NO_CIVIC) iCivicAnger += kPlayer.getCivicPercentAnger(eCivicA);
				}
				hp["civicAnger"]            = picojson::value((double)iCivicAnger);
				hp["defyResolutionAnger"]   = picojson::value((double)pCity->getDefyResolutionPercentAnger());
				hp["revRequestAnger"]       = picojson::value((double)pCity->getRevRequestPercentAnger());
				hp["eventAnger"]            = picojson::value((double)pCity->getEventAnger());
				hp["taxRateUnhappiness"]    = picojson::value((double)kPlayer.calculateTaxRateUnhappiness());
				hp["foreignUnhappyPercent"] = picojson::value((double)kPlayer.getForeignUnhappyPercent());
				hp["landmarkAnger"]         = picojson::value((double)pCity->getLandmarkAnger());
				hp["cityOverLimitUnhappy"]  = picojson::value((double)kPlayer.getCityOverLimitUnhappy());
				hp["playerLandmarkHappiness"] = picojson::value((double)kPlayer.getLandmarkHappiness()); // MAP_PERSONALIZED, both good+bad side
				hp["plotCulturePercent"]    = picojson::value((double)pCity->plot()->calculateCulturePercent(pCity->getOwner())); // foreign-unhappy operand (100 - this)
				hp["playerNumCities"]       = picojson::value((double)kPlayer.getNumCities());      // city-over-limit calc
				hp["playerCityLimit"]       = picojson::value((double)kPlayer.getCityLimit());      // city-over-limit gate + calc
				hp["espionageHappinessCounter"] = picojson::value((double)pCity->getEspionageHappinessCounter());
				hp["vassalUnhappiness"]     = picojson::value((double)pCity->getVassalUnhappiness());
				// the zero-out GATE FLAGS (a true flag short-circuits the whole ledger -- the emulator must honour them):
				hp["isNoUnhappiness"]        = picojson::value(pCity->isNoUnhappiness());
				hp["isNoUnhealthyPopulation"]= picojson::value(pCity->isNoUnhealthyPopulation());
				hp["isBuildingOnlyHealthy"]  = picojson::value(pCity->isBuildingOnlyHealthy());
				hp["playerNoCapitalUnhappiness"] = picojson::value(kPlayer.isNoCapitalUnhappiness());
				// anger TIMERS + modifiers + indices + counts feeding the get*PercentAnger calcs (audit: timer-driven, not derivable):
				hp["hurryAngerTimer"]       = picojson::value((double)pCity->getHurryAngerTimer());
				hp["conscriptAngerTimer"]   = picojson::value((double)pCity->getConscriptAngerTimer());
				hp["defyResolutionAngerTimer"] = picojson::value((double)pCity->getDefyResolutionAngerTimer());
				hp["revRequestAngerTimer"]  = picojson::value((double)pCity->getRevRequestAngerTimer());
				hp["warWearinessTimer"]     = picojson::value((double)pCity->getWarWearinessTimer());
				hp["landmarkAngerTimer"]    = picojson::value((double)pCity->getLandmarkAngerTimer());
				hp["cityWarWearinessModifier"] = picojson::value((double)pCity->getWarWearinessModifier());
				hp["hurryAngerModifier"]    = picojson::value((double)pCity->getHurryAngerModifier());
				hp["localRevIndex"]         = picojson::value((double)pCity->getLocalRevIndex());
				hp["revolutionIndex"]       = picojson::value((double)pCity->getRevolutionIndex());
				hp["militaryHappinessUnits"]= picojson::value((double)pCity->getMilitaryHappinessUnits());
				hp["religionCount"]         = picojson::value((double)pCity->getReligionCount());
				hp["playerWarWearinessPercentAnger"] = picojson::value((double)kPlayer.getWarWearinessPercentAnger());
				hp["playerWarWearinessModifier"]     = picojson::value((double)kPlayer.getWarWearinessModifier());
				{
					picojson::value::object kCH;
					for (int cc = 0; cc < NUM_COMMERCE_TYPES; ++cc)
						kCH[GC.getCommerceInfo((CommerceTypes)cc).getType()] = picojson::value((double)pCity->getCommerceHappinessPer((CommerceTypes)cc));
					hp["commerceHappinessPer"] = picojson::value(kCH);
				}
				hp["playerNoLandmarkAnger"] = picojson::value(kPlayer.isNoLandmarkAnger());                 // gates landmark anger
				hp["optMapPersonalized"]    = picojson::value(GC.getGame().isOption(GAMEOPTION_MAP_PERSONALIZED)); // gates landmark happy+anger
				hp["isCapital"]             = picojson::value(pCity->isCapital());                          // capital unhappiness-exemption gate
				hp["flatHurryAngerLength"]  = picojson::value((double)pCity->flatHurryAngerLength());        // hurry-anger divisor
				hp["flatConscriptAngerLength"] = picojson::value((double)pCity->flatConscriptAngerLength()); // conscript-anger divisor
				hp["flatDefyResolutionAngerLength"] = picojson::value((double)pCity->flatDefyResolutionAngerLength()); // defy-anger divisor
				o["happiness"] = picojson::value(hp);

				// ---- CH HURRY (legacy-value-calc-map §9.4 / §12): per-hurry gold + population COSTS (the dump gap --
				// only hurryAnger was emitted before). hurryCost() is the city's current production-cost basis; the
				// per-hurry getHurryGold/getHurryPopulation are pure const calcs (no canHurry gate needed for the dump). ----
				{
					picojson::value::object hu;
					hu["hurryCost"]   = picojson::value((double)pCity->hurryCost());
					hu["hurryAngerTimer"] = picojson::value((double)pCity->getHurryAngerTimer());
					picojson::value::array kHurry;
					for (int h = 0; h < GC.getNumHurryInfos(); ++h)
					{
						const HurryTypes eH = (HurryTypes)h;
						picojson::value::object e;
						e["type"]       = picojson::value(std::string(GC.getHurryInfo(eH).getType()));
						e["gold"]       = picojson::value((double)(int)pCity->getHurryGold(eH));
						e["population"] = picojson::value((double)pCity->getHurryPopulation(eH, pCity->hurryCost()));
						kHurry.push_back(picojson::value(e));
					}
					hu["byType"] = picojson::value(kHurry);
					o["hurry"] = picojson::value(hu);
				}

				// ---- CH buildRate building/project (legacy-value-calc-map §9.5 / §12: only the UNIT overload was
				// dumped, behind ?type=UNIT_). getProductionModifier(eItem) is the signed-% DISCOUNT on the item's cost.
				// Emit when ?type=BUILDING_X or PROJECT_X is supplied (the unit case stays in the unitBuild block below). ----
				if (strncmp(szType, "BUILDING_", 9) == 0)
				{
					const int iB = GC.getInfoTypeForString(szType, true);
					if (iB >= 0)
					{
						picojson::value::object br;
						br["type"]      = picojson::value(std::string(szType));
						br["buildRate"] = picojson::value((double)pCity->getProductionModifier((BuildingTypes)iB));
						o["buildRate"] = picojson::value(br);
					}
				}
				else if (strncmp(szType, "PROJECT_", 8) == 0)
				{
					const int iPr = GC.getInfoTypeForString(szType, true);
					if (iPr >= 0)
					{
						picojson::value::object br;
						br["type"]      = picojson::value(std::string(szType));
						br["buildRate"] = picojson::value((double)pCity->getProductionModifier((ProjectTypes)iPr));
						o["buildRate"] = picojson::value(br);
					}
				}
			}

			// ---- CH greatPeople (legacy-value-calc-map §9.5): base x modifier/100 (disorder -> 0) ----
			{
				picojson::value::object gp;
				gp["greatPeopleRate"]     = picojson::value((double)pCity->getGreatPeopleRate()); // realized
				gp["baseGreatPeopleRate"] = picojson::value((double)pCity->getBaseGreatPeopleRate()); // aggregate (city base + national)
				gp["totalGPRateModifier"] = picojson::value((double)pCity->getTotalGreatPeopleRateModifier()); // aggregate (split below)
				gp["greatPeopleProgress"] = picojson::value((double)pCity->getGreatPeopleProgress());
				gp["threshold"]           = picojson::value((double)kPlayer.greatPeopleThresholdNonMilitary());
				// base split (getBaseGreatPeopleRate, CvCity.cpp): max(0,m_iBaseGreatPeopleRate) + national; city base = base - national.
				gp["nationalGreatPeopleRate"] = picojson::value((double)kPlayer.getNationalGreatPeopleRate());
				gp["baseGreatPeopleRateRaw"] = picojson::value((double)pCity->getBaseGreatPeopleRateRaw()); // raw m_iBaseGreatPeopleRate (pre-max/national)
				// modifier split (getTotalGreatPeopleRateModifier): 100 + city + player + (stateReligion) + (goldenAge):
				gp["cityGPRateModifier"]   = picojson::value((double)pCity->getGreatPeopleRateModifier());
				gp["playerGPRateModifier"] = picojson::value((double)kPlayer.getGreatPeopleRateModifier());
				gp["stateReligionGPRateModifier"] = picojson::value((double)((kPlayer.getStateReligion() != NO_RELIGION && pCity->isHasReligion(kPlayer.getStateReligion())) ? kPlayer.getStateReligionGreatPeopleRateModifier() : 0));
				gp["goldenAgeGPRateModifier"] = picojson::value((double)(kPlayer.isGoldenAge() ? GC.getGOLDEN_AGE_GREAT_PEOPLE_MODIFIER() : 0));
				gp["isDisorder"]          = picojson::value(pCity->isDisorder()); // disorder -> rate forced 0 (distinguish from zero-base)
				o["greatPeople"] = picojson::value(gp);
			}

			// ---- CH tradeRoutes (legacy-value-calc-map §9.5): count + realized trade yields (per-partner profit not reproduced offline) ----
			{
				picojson::value::object tr;
				tr["tradeRoutes"]          = picojson::value((double)pCity->getTradeRoutes()); // realized count (clamped)
				tr["maxTradeRoutes"]       = picojson::value((double)pCity->getMaxTradeRoutes());
				// count components (getTradeRoutes, CvCity.cpp): game + player + (coastal) + extra, clamped [0,max]:
				tr["gameTradeRoutes"]      = picojson::value((double)GC.getGame().getTradeRoutes());
				tr["playerTradeRoutes"]    = picojson::value((double)kPlayer.getTradeRoutes());
				tr["coastalTradeRoutes"]   = picojson::value((double)kPlayer.getCoastalTradeRoutes());
				tr["extraTradeRoutes"]     = picojson::value((double)pCity->getExtraTradeRoutes());
				tr["maxTradeRoutesAdjustment"] = picojson::value((double)kPlayer.getMaxTradeRoutesAdjustment());
				tr["maxTradeRoutesConst"]  = picojson::value((double)GC.getMAX_TRADE_ROUTES());          // base max (before player adj)
				tr["isCoastalForTrade"]    = picojson::value(pCity->isCoastal(GC.getWorldInfo(GC.getMap().getWorldSize()).getOceanMinAreaSize())); // gates coastalTradeRoutes inclusion
				tr["tradeYieldFood"]       = picojson::value((double)pCity->getTradeYield(YIELD_FOOD));
				tr["tradeYieldProduction"] = picojson::value((double)pCity->getTradeYield(YIELD_PRODUCTION));
				tr["tradeYieldCommerce"]   = picojson::value((double)pCity->getTradeYield(YIELD_COMMERCE));
				// LEGACY trade-MODIFIER accumulators (totalTradeModifier + getTradeYieldModifier components) -- so the
				// offline calc compares its deposit-sum vs these legacy values per modifier and attributes any gap to a
				// NAMED source (curator under-emit vs missing source), never guessing. (calc-map §9.5.)
				tr["tradeYieldModFood"]        = picojson::value((double)kPlayer.getTradeYieldModifier(YIELD_FOOD));
				tr["tradeYieldModProduction"]  = picojson::value((double)kPlayer.getTradeYieldModifier(YIELD_PRODUCTION));
				tr["tradeYieldModCommerce"]    = picojson::value((double)kPlayer.getTradeYieldModifier(YIELD_COMMERCE));
				tr["tradeRouteModifier"]       = picojson::value((double)pCity->getTradeRouteModifier());
				tr["popTradeModifier"]         = picojson::value((double)pCity->getPopulationTradeModifier());
				tr["teamTradeModifier"]        = picojson::value((double)GET_TEAM(pCity->getTeam()).getTradeModifier());
				tr["cityForeignTradeRouteModifier"]   = picojson::value((double)pCity->getForeignTradeRouteModifier());
				tr["playerForeignTradeRouteModifier"] = picojson::value((double)kPlayer.getForeignTradeRouteModifier());
				tr["teamForeignTradeModifier"]        = picojson::value((double)GET_TEAM(pCity->getTeam()).getForeignTradeModifier());
				o["tradeRoutes"] = picojson::value(tr);
			}

			// ---- CH building-level city families (legacy-value-calc-map §10.3): standing modifier values (reading) ----
			{
				picojson::value::object bl;
				bl["localCaptureProbability"]    = picojson::value((double)pCity->getExtraLocalCaptureProbabilityModifier());
				bl["localCaptureResistance"]     = picojson::value((double)pCity->getExtraLocalCaptureResistanceModifier());
				bl["nationalCaptureProbability"] = picojson::value((double)kPlayer.getExtraNationalCaptureProbabilityModifier());
				bl["nationalCaptureResistance"]  = picojson::value((double)kPlayer.getExtraNationalCaptureResistanceModifier());
				bl["occupationTimer"]            = picojson::value((double)pCity->getOccupationTimer());
				bl["espionageDefenseModifier"]   = picojson::value((double)pCity->getEspionageDefenseModifier()); // aggregate (city + national)
				bl["nationalEspionageDefense"]   = picojson::value((double)kPlayer.getNationalEspionageDefense()); // city part = modifier - this
				bl["healRate"]                   = picojson::value((double)pCity->getHealRate());
				// heal-per-unitcombat (getHealUnitCombatTypeTotal, building HealUnitCombatType array) -- non-zero only:
				{
					picojson::value::object kHeal;
					for (int uc = 0; uc < GC.getNumUnitCombatInfos(); ++uc)
					{
						const int iHv = pCity->getHealUnitCombatTypeTotal((UnitCombatTypes)uc);
						if (iHv != 0) kHeal[GC.getUnitCombatInfo((UnitCombatTypes)uc).getType()] = picojson::value((double)iHv);
					}
					if (!kHeal.empty()) bl["healByUnitCombat"] = picojson::value(kHeal);
				}
				o["buildingLevel"] = picojson::value(bl);
			}

			// ---- CH property (legacy-value-calc-map §9.1): each PROPERTY_* CURRENT VALUE (reading). The per-turn
			// DELTA is the CvPropertySolver (sources -> interactions -> propagators); propagators are SPATIAL (#429),
			// so the offline reproduction (non-spatial sources+interactions) is a follow-up guard -- this is the state. ----
			{
				picojson::value::array pr;
				const CvProperties* pProps = pCity->getProperties();
				for (int p = 0; p < GC.getNumPropertyInfos(); ++p)
				{
					picojson::value::object e;
					e["type"]  = picojson::value(std::string(GC.getPropertyInfo((PropertyTypes)p).getType()));
					e["value"] = picojson::value((double)(pProps != NULL ? pProps->getValueByProperty((PropertyTypes)p) : 0));
					pr.push_back(picojson::value(e));
				}
				o["properties"] = picojson::value(pr);
			}

			// optional ?type=UNIT_X -> the unit-build start-XP + buildRate for that unit, per (city x unitType): the
			// building->combat-class XP (legacy-value-calc-map §11.4). Folds the unit-plane's reproducible bits into
			// cityInput (no per-unit-instance endpoint). startXP/buildRate are CvCity methods keyed by unit TYPE.
			if (strncmp(szType, "UNIT_", 5) == 0)
			{
				const int iUnit = GC.getInfoTypeForString(szType, true);
				if (iUnit >= 0)
				{
					const CvUnitInfo& kU = GC.getUnitInfo((UnitTypes)iUnit);
					const UnitCombatTypes eUC = (UnitCombatTypes)kU.getUnitCombatType();
					picojson::value::object ub;
					ub["unit"]                    = picojson::value(std::string(szType));
					ub["startXP"]                 = picojson::value((double)pCity->getProductionExperience((UnitTypes)iUnit)); // realized
					ub["buildRate"]               = picojson::value((double)pCity->getProductionModifier((UnitTypes)iUnit));   // realized %
					ub["canAcquireExperience"]    = picojson::value(kU.canAcquireExperience());
					ub["freeExpCity"]             = picojson::value((double)pCity->getFreeExperience());
					ub["freeExpPlayer"]           = picojson::value((double)kPlayer.getFreeExperience());
					ub["specialistFreeExp"]       = picojson::value((double)pCity->getSpecialistFreeExperience());
					ub["unitCombatFreeExpCity"]   = picojson::value((double)(eUC != NO_UNITCOMBAT ? pCity->getUnitCombatFreeExperience(eUC) : 0));
					ub["unitCombatFreeExpPlayer"] = picojson::value((double)(eUC != NO_UNITCOMBAT ? kPlayer.getUnitCombatFreeExperience(eUC) : 0));
					ub["domainFreeExp"]           = picojson::value((double)pCity->getDomainFreeExperience((DomainTypes)kU.getDomainType()));
					o["unitBuild"] = picojson::value(ub);
				}
			}
			return CvString(picojson::value(o).serialize().c_str());
		}

		// MODIFIER (owner 2026-06-19) -- the magnitude analogue of the can* gates: for the requested city (else the
		// capital), the cascade effective per PILOT yield family (food/production/commerce) + its flat/percent decomposition
		// + the legacy getYieldRate100, so the modifier computation is verifiable ON DEMAND (no per-turn-tee timing). No
		// type param. cascade = city-scope building contribution only (pilot, x1); legacy100 = full realized (x100) -- their
		// diff is the parity work (sources not yet deposited), surfaced in full by /diagnostic/modifierSweep (increment 3).
		if (strcmp(szAction, "modifier") == 0)
		{
			o["city"] = picojson::value((double)iCityId);
			if (pCity == NULL)
			{
				o["error"] = picojson::value(std::string("no city"));
				return CvString(picojson::value(o).serialize().c_str());
			}
			CvCascadeContext kCtx(iPlayer, iCityId);
			// ALL families + the cascade's PER-SOURCE decomposition (owner ruling 2026-06-20: to extend the cascade we
			// must know what we mirror -- set the cascade's deposits source-by-source against the legacy per-source dump).
			// optional ?type=<familyKey> scopes to one family; else all NUM_MODIFIER_FAMILIES.
			picojson::value::array kFam;
			for (int f = 0; f < NUM_MODIFIER_FAMILIES; ++f)
			{
				const char* szKey = cascadeModifierFamilyInfo(f).szKey;
				if (szType[0] != '\0' && strcmp(szType, szKey) != 0) continue;
				CvModifierSlot slot; int iBase = 0, iCascade = 0, iLegacy = 0;
				cascadeModifierFamilyShadow(pCity, kCtx, f, slot, iBase, iCascade, iLegacy);
				picojson::value::object e;
				e["family"]    = picojson::value(std::string(szKey));
				e["base"]      = picojson::value((double)iBase);
				e["flat"]      = picojson::value((double)slot.iFlat);
				e["percent"]   = picojson::value((double)slot.iPercent);
				e["mult100"]   = picojson::value((double)slot.iMultiplierX100);
				e["cascade"]   = picojson::value((double)iCascade);
				e["legacy"]    = picojson::value((double)iLegacy);
				// PER-SOURCE: each active building (city) / civic (empire) deposit feeding this family's slot, so the
				// cascade total is attributable source-by-source (the legacy per-source lives in cityInput.buildingYields).
				std::vector<CvModifierSourceContribution> srcs;
				cascadeModifierCitySources(f, kCtx, srcs);
				picojson::value::array kSrc;
				for (size_t s = 0; s < srcs.size(); ++s)
				{
					picojson::value::object se;
					se["source"] = picojson::value(std::string(srcs[s].bCivic
						? GC.getCivicInfo((CivicTypes)srcs[s].iEntity).getType()
						: GC.getBuildingInfo((BuildingTypes)srcs[s].iEntity).getType()));
					se["scope"]   = picojson::value(std::string(srcs[s].bCivic ? "empire" : "city"));
					se["flat"]    = picojson::value((double)srcs[s].slot.iFlat);
					se["percent"] = picojson::value((double)srcs[s].slot.iPercent);
					se["mult100"] = picojson::value((double)srcs[s].slot.iMultiplierX100);
					kSrc.push_back(picojson::value(se));
				}
				e["sources"] = picojson::value(kSrc);
				kFam.push_back(picojson::value(e));
			}
			o["families"] = picojson::value(kFam);
			return CvString(picojson::value(o).serialize().c_str());
		}

		// PLAYER-INPUT (calc-emulator §11): EMPIRE/player-scope value calcs -- gold-per-turn + science-per-turn
		// (reproducible from components), power/assets/demographics (readings). No type/city. (legacy-value-calc-map §11.1/11.2)
		if (strcmp(szAction, "playerInput") == 0)
		{
			o["isAnarchy"] = picojson::value(kPlayer.isAnarchy());
			picojson::value::object g;
			g["goldRate"]          = picojson::value((double)kPlayer.calculateGoldRate());       // realized net
			g["baseNetGold"]       = picojson::value((double)(int)kPlayer.calculateBaseNetGold());
			g["commerceGold"]      = picojson::value((double)kPlayer.getCommerceRate(COMMERCE_GOLD));
			g["goldPerTurnDeals"]  = picojson::value((double)kPlayer.getGoldPerTurn());
			g["finalExpense"]      = picojson::value((double)(int)kPlayer.getFinalExpense());
			g["preInflatedCosts"]  = picojson::value((double)(int)kPlayer.calculatePreInflatedCosts());
			g["inflationMod10000"] = picojson::value((double)kPlayer.getInflationMod10000());
			g["treasury"]          = picojson::value((double)(int)kPlayer.getGold());
			g["totalMaintenance"]  = picojson::value((double)kPlayer.getTotalMaintenance());
			// preInflatedCosts split (§11.1 = treasuryUpkeep + totalMaintenance + civicUpkeep + finalUnitUpkeep
			// + unitSupply + corpMaint); unitSupply/corpMaint/treasuryUpkeep weren't individually emitted:
			g["treasuryUpkeep"]       = picojson::value((double)(int)kPlayer.getTreasuryUpkeep());
			g["unitSupply"]           = picojson::value((double)kPlayer.calculateUnitSupply());
			g["corporateMaintenance"] = picojson::value((double)(int)kPlayer.getCorporateMaintenance());
			// embedded scalers in treasuryUpkeep / calculateUnitSupply (audit):
			g["gameSpeedPercent"]     = picojson::value((double)GC.getGameSpeedInfo(GC.getGame().getGameSpeedType()).getSpeedPercent()); // treasury scaling
			g["numOutsideUnits"]      = picojson::value((double)kPlayer.getNumOutsideUnits());  // unitSupply paid-unit count
			g["isResearchFlexible"]   = picojson::value(kPlayer.isCommerceFlexible(COMMERCE_RESEARCH)); // gold-rate path gate
			g["distantUnitSupportCostModifier"] = picojson::value((double)kPlayer.getDistantUnitSupportCostModifier());
			g["isNPC"]                = picojson::value(kPlayer.isNPC());                       // unitSupply 0 for NPC
			g["handicapAIUnitSupplyPercent"] = picojson::value((double)GC.getHandicapInfo(GC.getGame().getHandicapType()).getAIUnitSupplyPercent());
			g["initialFreeOutsideUnits"]     = picojson::value((double)GC.getDefineINT("INITIAL_FREE_OUTSIDE_UNITS"));        // unitSupply paid-unit offset
			g["initialOutsideUnitGoldPercent"]= picojson::value((double)GC.getDefineINT("INITIAL_OUTSIDE_UNIT_GOLD_PERCENT")); // unitSupply per-unit cost
			g["currentEra"]           = picojson::value((double)kPlayer.getCurrentEra());       // unitSupply era multiplier
			o["gold"] = picojson::value(g);
			picojson::value::object s;
			const TechTypes eCur = kPlayer.getCurrentResearch();
			s["currentResearch"]  = picojson::value((double)(int)eCur);
			s["researchRate"]     = picojson::value((double)kPlayer.calculateResearchRate(eCur));
			s["baseNetResearch"]  = picojson::value((double)(int)kPlayer.calculateBaseNetResearch(eCur));
			s["commerceResearch"] = picojson::value((double)kPlayer.getCommerceRate(COMMERCE_RESEARCH));
			s["baseResearchRate"] = picojson::value((double)GC.getDefineINT("BASE_RESEARCH_RATE"));
			s["nationalTechMod"]  = picojson::value((double)(eCur != NO_TECH ? kPlayer.getNationalTechResearchModifier(eCur) : 0));
			s["researchModifier"] = picojson::value((double)(eCur != NO_TECH ? kPlayer.calculateResearchModifier(eCur) : 0));
			o["science"] = picojson::value(s);
			picojson::value::object dm;
			dm["power"]            = picojson::value((double)kPlayer.getPower());
			dm["techPower"]        = picojson::value((double)kPlayer.getTechPower());
			dm["unitPower"]        = picojson::value((double)kPlayer.getUnitPower());
			dm["assets"]           = picojson::value((double)kPlayer.getAssets());
			dm["totalPopulation"]  = picojson::value((double)kPlayer.getTotalPopulation());
			dm["realPopulation"]   = picojson::value((double)(int)kPlayer.getRealPopulation());
			dm["totalLand"]        = picojson::value((double)kPlayer.getTotalLand());
			dm["totalLandScored"]  = picojson::value((double)kPlayer.getTotalLandScored());
			dm["numMilitaryUnits"] = picojson::value((double)kPlayer.getNumMilitaryUnits());
			o["demographics"] = picojson::value(dm);

			// ---- INFLATION source breakdown (legacy-value-calc-map §9.4 / §12 add-list): the per-source inputs to
			// getInflationMod10000 (only the realized mod + preInflatedCosts were dumped). hurriedCount drives the base;
			// the four source getters + handicap modify it. So the emulator reproduces the multiplier, not just reads it. ----
			{
				picojson::value::object inf;
				inf["inflationMod10000"] = picojson::value((double)kPlayer.getInflationMod10000());
				inf["hurriedCount"]      = picojson::value((double)kPlayer.getHurriedCount());
				inf["civicInflation"]    = picojson::value((double)kPlayer.getCivicInflation());
				inf["projectInflation"]  = picojson::value((double)kPlayer.getProjectInflation());
				inf["techInflation"]     = picojson::value((double)kPlayer.getTechInflation());
				inf["buildingInflation"] = picojson::value((double)kPlayer.getBuildingInflation());
				inf["handicapInflationPercent"] = picojson::value((double)GC.getHandicapInfo(kPlayer.getHandicapType()).getInflationPercent());
				// the remaining iMod sources (getInflationMod10000, CvPlayer.cpp): m_iInflationModifier + (−100×isRebel)
				// + the isNormalAI handicap AI-inflation/per-era ramp -- so the full multiplier is reproducible:
				inf["inflationModifier"] = picojson::value((double)kPlayer.getInflationModifier());
				inf["isRebel"]           = picojson::value(kPlayer.isRebel());
				inf["isNormalAI"]        = picojson::value(kPlayer.isNormalAI());
				inf["aiInflationPercent"]= picojson::value((double)GC.getHandicapInfo(GC.getGame().getHandicapType()).getAIInflationPercent());
				inf["aiPerEraModifier"]  = picojson::value((double)GC.getHandicapInfo(GC.getGame().getHandicapType()).getAIPerEraModifier());
				inf["currentEra"]        = picojson::value((double)kPlayer.getCurrentEra()); // scales aiPerEraModifier
				o["inflation"] = picojson::value(inf);
			}

			// ---- UPKEEP decomposition (legacy-value-calc-map §5 / §12: only the realized civic+unit totals were dumped).
			// The civilian/military gross-100 + net split + free allowances feed getFinalUnitUpkeep; the civic total feeds
			// the maintenance ledger. (Per-unit calcUpkeep100 is the unitInput channel.) ----
			{
				picojson::value::object up;
				up["finalUnitUpkeep"]        = picojson::value((double)(int)kPlayer.getFinalUnitUpkeep());
				up["unitUpkeepCivilian100"]  = picojson::value((double)(int)kPlayer.getUnitUpkeepCivilian100());
				up["unitUpkeepCivilianNet"]  = picojson::value((double)(int)kPlayer.getUnitUpkeepCivilianNet());
				up["unitUpkeepMilitary100"]  = picojson::value((double)(int)kPlayer.getUnitUpkeepMilitary100());
				up["unitUpkeepMilitaryNet"]  = picojson::value((double)(int)kPlayer.getUnitUpkeepMilitaryNet());
				up["freeUnitUpkeepCivilian"] = picojson::value((double)kPlayer.getFreeUnitUpkeepCivilian());
				up["freeUnitUpkeepMilitary"] = picojson::value((double)kPlayer.getFreeUnitUpkeepMilitary());
				up["civicUpkeep"]            = picojson::value((double)kPlayer.getCivicUpkeep(false));
				// the embedded upkeep/free/AI internals (audit: hidden in getFinalUnitUpkeep / getCivicUpkeep / free getters):
				up["civilianUnitUpkeepMod"]  = picojson::value((double)kPlayer.getCivilianUnitUpkeepMod());
				up["militaryUnitUpkeepMod"]  = picojson::value((double)kPlayer.getMilitaryUnitUpkeepMod());
				up["baseFreeUnitUpkeepCivilian"] = picojson::value((double)kPlayer.getBaseFreeUnitUpkeepCivilian());
				up["baseFreeUnitUpkeepMilitary"] = picojson::value((double)kPlayer.getBaseFreeUnitUpkeepMilitary());
				up["freeUnitUpkeepCivilianPopPercent"] = picojson::value((double)kPlayer.getFreeUnitUpkeepCivilianPopPercent());
				up["freeUnitUpkeepMilitaryPopPercent"] = picojson::value((double)kPlayer.getFreeUnitUpkeepMilitaryPopPercent());
				up["numCities"]              = picojson::value((double)kPlayer.getNumCities());        // civic-upkeep per-city basis
				up["playerUpkeepModifier"]   = picojson::value((double)kPlayer.getUpkeepModifier());   // applied to civic upkeep
				up["isNormalAI"]             = picojson::value(kPlayer.isNormalAI());                  // gates AI handicap mults
				up["isHumanPlayer"]          = picojson::value(kPlayer.isHumanPlayer());
				up["aiPerEraModifier"]       = picojson::value((double)GC.getHandicapInfo(GC.getGame().getHandicapType()).getAIPerEraModifier()); // AI per-era upkeep ramp
				up["isNPC"]                  = picojson::value(kPlayer.isNPC());                       // gates getFinalUnitUpkeep to 0
				up["handicapAICivicUpkeepPercent"] = picojson::value((double)GC.getHandicapInfo(GC.getGame().getHandicapType()).getAICivicUpkeepPercent());
				up["handicapAIUnitUpkeepPercent"]  = picojson::value((double)GC.getHandicapInfo(GC.getGame().getHandicapType()).getAIUnitUpkeepPercent());
				// civic-upkeep internals (getSingleCivicUpkeep / getCivicUpkeep, CvPlayer.cpp):
				up["totalPopulation"]    = picojson::value((double)kPlayer.getTotalPopulation());
				up["currentEra"]         = picojson::value((double)kPlayer.getCurrentEra());
				up["isRebel"]            = picojson::value(kPlayer.isRebel());            // halves civic upkeep
				up["isAnarchy"]          = picojson::value(kPlayer.isAnarchy());          // gates civic upkeep to 0
				up["upkeepPopulationOffset"] = picojson::value((double)GC.getDefineINT("UPKEEP_POPULATION_OFFSET"));
				up["upkeepCityOffset"]   = picojson::value((double)GC.getDefineINT("UPKEEP_CITY_OFFSET"));
				// per-civic upkeep type details (getSingleCivicUpkeep per active civic):
				{
					picojson::value::array kCivics;
					for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
					{
						const CivicTypes eCv = kPlayer.getCivics((CivicOptionTypes)co);
						if (eCv == NO_CIVIC) continue;
						picojson::value::object cu;
						cu["civic"]          = picojson::value(std::string(GC.getCivicInfo(eCv).getType()));
						cu["isNoCivicUpkeep"]= picojson::value(kPlayer.isNoCivicUpkeep((CivicOptionTypes)co));
						const int eUp = GC.getCivicInfo(eCv).getUpkeep();
						if (eUp != NO_UPKEEP)
						{
							cu["populationPercent"] = picojson::value((double)GC.getUpkeepInfo((UpkeepTypes)eUp).getPopulationPercent());
							cu["cityPercent"]       = picojson::value((double)GC.getUpkeepInfo((UpkeepTypes)eUp).getCityPercent());
						}
						kCivics.push_back(picojson::value(cu));
					}
					up["civics"] = picojson::value(kCivics);
				}
				up["handicapUnitUpkeepPercent"]  = picojson::value((double)GC.getHandicapInfo(kPlayer.getHandicapType()).getUnitUpkeepPercent());
				up["handicapCivicUpkeepPercent"] = picojson::value((double)GC.getHandicapInfo(kPlayer.getHandicapType()).getCivicUpkeepPercent());
				o["upkeep"] = picojson::value(up);
			}

			// ---- PLAYER WELLBEING aggregates (health-happiness / war-weariness per-system maps): the empire-scope
			// happiness/health sources + the war-weariness percent-anger that feeds every city's anger ledger. ----
			{
				picojson::value::object wb;
				wb["warWearinessPercentAnger"] = picojson::value((double)kPlayer.getWarWearinessPercentAnger());
				wb["buildingHappiness"]   = picojson::value((double)kPlayer.getBuildingHappiness());
				wb["extraHappiness"]      = picojson::value((double)kPlayer.getExtraHappiness());
				wb["worldHappiness"]      = picojson::value((double)kPlayer.getWorldHappiness());
				wb["buildingGoodHealth"]  = picojson::value((double)kPlayer.getBuildingGoodHealth());
				wb["buildingBadHealth"]   = picojson::value((double)kPlayer.getBuildingBadHealth());
				wb["noCapitalUnhappiness"]= picojson::value(kPlayer.isNoCapitalUnhappiness());
				o["wellbeing"] = picojson::value(wb);
			}
			return CvString(picojson::value(o).serialize().c_str());
		}

		// ---- UNIT-INPUT (legacy-value-calc-map §6 / §11.4 / §12 "ENTIRELY ABSENT") -- the unit-plane combat-stat
		// decomposition, the one whole channel with NO endpoint. Per the player's units: baseCombat + the aggregate
		// getExtra* stat set (CvUnit exposes AGGREGATE only -- per-source attribution is the promotion/unitcombat lists
		// the emulator sums from the Info JSON, §11.4 "CRITICAL build constraint") + HP + the promotion/unitcombat
		// membership. maxCombatStr is context-dependent (~730-line situational calc) -> NOT reproduced offline; the
		// dump emits the offline-reproducible baseCombatStrPreCheck + the extras (§11.4). Detail-capped like the sweeps;
		// the movement/range half lives in /diagnostic/movementSweep. ----
		if (strcmp(szAction, "unitInput") == 0)
		{
			const bool bFull = (strcmp(szType, "full") == 0);
			const int iCap = bFull ? 100000 : 400;
			picojson::value::array kUnits;
			int iUnits = 0;
			foreach_(const CvUnit* pUnit, kPlayer.units())
			{
				++iUnits;
				if ((int)kUnits.size() >= iCap) continue;
				const UnitTypes eUT = pUnit->getUnitType();
				picojson::value::object u;
				u["id"]   = picojson::value((double)pUnit->getID());
				u["type"] = picojson::value(std::string(eUT != NO_UNIT ? GC.getUnitInfo(eUT).getType() : "NO_UNIT"));
				u["x"]    = picojson::value((double)pUnit->getX());
				u["y"]    = picojson::value((double)pUnit->getY());
				u["level"] = picojson::value((double)pUnit->getLevel());
				u["experience"] = picojson::value((double)pUnit->getExperience());
				// combat strength (the offline-reproducible base; maxCombatStr is situational -> not dumped):
				u["baseCombatStr"]         = picojson::value((double)pUnit->baseCombatStr());
				u["baseCombatStrPreCheck"] = picojson::value((double)pUnit->baseCombatStrPreCheck());
				u["baseCombat"]            = picojson::value((double)pUnit->getBaseCombat()); // raw m_iBaseCombat (lossy in PreCheck)
				u["damage"]                = picojson::value((double)pUnit->getDamage());    // HP = maxHP - damage
				u["hp"]    = picojson::value((double)pUnit->getHP());
				u["maxHP"] = picojson::value((double)pUnit->getMaxHP());
				// the aggregate getExtra* stat set (the §11.4 dumpable set; per-source via the promo/combat lists below):
				picojson::value::object ex;
				ex["strength"]           = picojson::value((double)pUnit->getExtraStrength());
				ex["strengthModifier"]   = picojson::value((double)pUnit->getExtraStrengthModifier());
				ex["combatPercent"]      = picojson::value((double)pUnit->getExtraCombatPercent());
				ex["cityAttackPercent"]  = picojson::value((double)pUnit->getExtraCityAttackPercent());
				ex["cityDefensePercent"] = picojson::value((double)pUnit->getExtraCityDefensePercent());
				ex["withdrawal"]         = picojson::value((double)pUnit->getExtraWithdrawal());
				ex["collateralDamage"]   = picojson::value((double)pUnit->getExtraCollateralDamage());
				ex["bombardRate"]        = picojson::value((double)pUnit->getExtraBombardRate());
				ex["firstStrikes"]       = picojson::value((double)pUnit->getExtraFirstStrikes());
				ex["chanceFirstStrikes"] = picojson::value((double)pUnit->getExtraChanceFirstStrikes());
				ex["enemyHeal"]          = picojson::value((double)pUnit->getExtraEnemyHeal());
				ex["neutralHeal"]        = picojson::value((double)pUnit->getExtraNeutralHeal());
				ex["friendlyHeal"]       = picojson::value((double)pUnit->getExtraFriendlyHeal());
				ex["visibilityRange"]    = picojson::value((double)pUnit->getExtraVisibilityRange());
				ex["moves"]              = picojson::value((double)pUnit->getExtraMoves());
				ex["moveDiscount"]       = picojson::value((double)pUnit->getExtraMoveDiscount());
				// the REMAINING aggregate getExtra* set (CvUnit.h enumeration -- emit ALL, owner ruling 2026-06-20):
				ex["noDefensiveBonus"]      = picojson::value((double)pUnit->getExtraNoDefensiveBonusCount());
				ex["dropRange"]             = picojson::value((double)pUnit->getExtraDropRange());
				ex["airRange"]              = picojson::value((double)pUnit->getExtraAirRange());
				ex["intercept"]             = picojson::value((double)pUnit->getExtraIntercept());
				ex["evasion"]               = picojson::value((double)pUnit->getExtraEvasion());
				ex["religiousCombatMod"]    = picojson::value((double)pUnit->getExtraReligiousCombatModifier());
				ex["extraUpkeep100"]        = picojson::value((double)pUnit->getExtraUpkeep100());
				ex["hillsAttack"]           = picojson::value((double)pUnit->getExtraHillsAttackPercent());
				ex["hillsDefense"]          = picojson::value((double)pUnit->getExtraHillsDefensePercent());
				ex["combatModPerSizeMore"]  = picojson::value((double)pUnit->getExtraCombatModifierPerSizeMore());
				ex["combatModPerSizeLess"]  = picojson::value((double)pUnit->getExtraCombatModifierPerSizeLess());
				ex["combatModPerVolumeMore"]= picojson::value((double)pUnit->getExtraCombatModifierPerVolumeMore());
				ex["combatModPerVolumeLess"]= picojson::value((double)pUnit->getExtraCombatModifierPerVolumeLess());
				ex["maxHPExtra"]            = picojson::value((double)pUnit->getExtraMaxHP());
				ex["quality"]               = picojson::value((double)pUnit->getExtraQuality());
				ex["group"]                 = picojson::value((double)pUnit->getExtraGroup());
				ex["size"]                  = picojson::value((double)pUnit->getExtraSize());
				ex["cargoVolume"]           = picojson::value((double)pUnit->getExtraCargoVolume());
				ex["rBombardDamage"]        = picojson::value((double)pUnit->getExtraRBombardDamage());
				ex["rBombardDamageLimit"]   = picojson::value((double)pUnit->getExtraRBombardDamageLimit());
				ex["rBombardDamageMaxUnits"]= picojson::value((double)pUnit->getExtraRBombardDamageMaxUnits());
				ex["dcmBombRange"]          = picojson::value((double)pUnit->getExtraDCMBombRange());
				ex["dcmBombAccuracy"]       = picojson::value((double)pUnit->getExtraDCMBombAccuracy());
				ex["stealthStrikes"]        = picojson::value((double)pUnit->getExtraStealthStrikes());
				ex["stealthCombatMod"]      = picojson::value((double)pUnit->getExtraStealthCombatModifier());
				ex["trapDamageMax"]         = picojson::value((double)pUnit->getExtraTrapDamageMax());
				ex["trapDamageMin"]         = picojson::value((double)pUnit->getExtraTrapDamageMin());
				ex["trapComplexity"]        = picojson::value((double)pUnit->getExtraTrapComplexity());
				ex["numTriggers"]           = picojson::value((double)pUnit->getExtraNumTriggers());
				ex["gatherHerdCount"]       = picojson::value((double)pUnit->getExtraGatherHerdCount());
				ex["attackCombatMod"]       = picojson::value((double)pUnit->getExtraAttackCombatModifier());
				ex["defenseCombatMod"]      = picojson::value((double)pUnit->getExtraDefenseCombatModifier());
				ex["vsBarbs"]               = picojson::value((double)pUnit->getExtraVSBarbs());
				ex["damageModifier"]        = picojson::value((double)pUnit->getExtraDamageModifier());
				ex["unnerve"]               = picojson::value((double)pUnit->getExtraUnnerve());
				ex["enclose"]               = picojson::value((double)pUnit->getExtraEnclose());
				ex["lunge"]                 = picojson::value((double)pUnit->getExtraLunge());
				ex["dynamicDefense"]        = picojson::value((double)pUnit->getExtraDynamicDefense());
				ex["endurance"]             = picojson::value((double)pUnit->getExtraEndurance());
				ex["poisonProbabilityMod"]  = picojson::value((double)pUnit->getExtraPoisonProbabilityModifier());
				u["extra"] = picojson::value(ex);
				// per-unit upkeep: getUpkeep100 is the realized STORED x100 (calcUpkeep100 maintains it on init + every
				// source mutation + recalculateUnitUpkeep -> event-driven FRESH, not stale). The calcUpkeep100 FORMULA
				// sources (CvUnit.cpp): 100×baseUpkeep + extraUpkeep100, then ×upkeepModifier, then ×upkeepMultiplierSM; NPC skipped.
				u["upkeep100"]          = picojson::value((double)pUnit->getUpkeep100());
				u["baseUpkeep"]         = picojson::value((double)(eUT != NO_UNIT ? GC.getUnitInfo(eUT).getBaseUpkeep() : 0));
				u["upkeepModifier"]     = picojson::value((double)pUnit->getUpkeepModifier());
				u["upkeepMultiplierSM"] = picojson::value((double)pUnit->getUpkeepMultiplierSM());
				u["isNPC"]              = picojson::value(pUnit->isNPC());
				// per-KEYED getExtra* maps (non-zero only) -- the vs-keyed attribution the aggregate hides:
				{
					picojson::value::object kUC, kDom, kTerA, kTerD, kTerW, kFeaA, kFeaD, kFeaW, kFlank,
					                        kTrapDis, kTrapAvoid, kTrapTrig, kInvVis, kInvInvis, kInvVisR, kBuildWork;
					for (int uc = 0; uc < GC.getNumUnitCombatInfos(); ++uc)
					{
						const char* nm = GC.getUnitCombatInfo((UnitCombatTypes)uc).getType();
						const int m = pUnit->getExtraUnitCombatModifier((UnitCombatTypes)uc);
						const int fl = pUnit->getExtraFlankingStrengthbyUnitCombatType((UnitCombatTypes)uc);
						const int td = pUnit->getExtraTrapDisableUnitCombatType((UnitCombatTypes)uc);
						const int ta = pUnit->getExtraTrapAvoidanceUnitCombatType((UnitCombatTypes)uc);
						const int tt = pUnit->getExtraTrapTriggerUnitCombatType((UnitCombatTypes)uc);
						if (m)  kUC[nm]    = picojson::value((double)m);
						if (fl) kFlank[nm] = picojson::value((double)fl);
						if (td) kTrapDis[nm]   = picojson::value((double)td);
						if (ta) kTrapAvoid[nm] = picojson::value((double)ta);
						if (tt) kTrapTrig[nm]  = picojson::value((double)tt);
					}
					for (int dm = 0; dm < NUM_DOMAIN_TYPES; ++dm)
					{
						const int m = pUnit->getExtraDomainModifier((DomainTypes)dm);
						if (m) kDom[GC.getDomainInfo((DomainTypes)dm).getType()] = picojson::value((double)m);
					}
					for (int t = 0; t < GC.getNumTerrainInfos(); ++t)
					{
						const char* nm = GC.getTerrainInfo((TerrainTypes)t).getType();
						const int a = pUnit->getExtraTerrainAttackPercent((TerrainTypes)t);
						const int d = pUnit->getExtraTerrainDefensePercent((TerrainTypes)t);
						const int w = pUnit->getExtraTerrainWorkPercent((TerrainTypes)t);
						if (a) kTerA[nm] = picojson::value((double)a);
						if (d) kTerD[nm] = picojson::value((double)d);
						if (w) kTerW[nm] = picojson::value((double)w);
					}
					for (int fe = 0; fe < GC.getNumFeatureInfos(); ++fe)
					{
						const char* nm = GC.getFeatureInfo((FeatureTypes)fe).getType();
						const int a = pUnit->getExtraFeatureAttackPercent((FeatureTypes)fe);
						const int d = pUnit->getExtraFeatureDefensePercent((FeatureTypes)fe);
						const int w = pUnit->getExtraFeatureWorkPercent((FeatureTypes)fe);
						if (a) kFeaA[nm] = picojson::value((double)a);
						if (d) kFeaD[nm] = picojson::value((double)d);
						if (w) kFeaW[nm] = picojson::value((double)w);
					}
					for (int iv = 0; iv < GC.getNumInvisibleInfos(); ++iv)
					{
						const char* nm = GC.getInvisibleInfo((InvisibleTypes)iv).getType();
						const int vi = pUnit->getExtraVisibilityIntensityType((InvisibleTypes)iv);
						const int ii = pUnit->getExtraInvisibilityIntensityType((InvisibleTypes)iv);
						const int vr = pUnit->getExtraVisibilityIntensityRangeType((InvisibleTypes)iv);
						if (vi) kInvVis[nm]   = picojson::value((double)vi);
						if (ii) kInvInvis[nm] = picojson::value((double)ii);
						if (vr) kInvVisR[nm]  = picojson::value((double)vr);
					}
					for (int bu = 0; bu < GC.getNumBuildInfos(); ++bu)
					{
						const int w = pUnit->getExtraWorkModForBuild((BuildTypes)bu);
						if (w) kBuildWork[GC.getBuildInfo((BuildTypes)bu).getType()] = picojson::value((double)w);
					}
					if (!kUC.empty())   u["extraUnitCombatModifier"] = picojson::value(kUC);
					if (!kFlank.empty())u["extraFlankingByUnitCombat"]= picojson::value(kFlank);
					if (!kTrapDis.empty())   u["extraTrapDisableByUnitCombat"]   = picojson::value(kTrapDis);
					if (!kTrapAvoid.empty()) u["extraTrapAvoidanceByUnitCombat"] = picojson::value(kTrapAvoid);
					if (!kTrapTrig.empty())  u["extraTrapTriggerByUnitCombat"]   = picojson::value(kTrapTrig);
					if (!kDom.empty())  u["extraDomainModifier"]     = picojson::value(kDom);
					if (!kTerA.empty()) u["extraTerrainAttack"]      = picojson::value(kTerA);
					if (!kTerD.empty()) u["extraTerrainDefense"]     = picojson::value(kTerD);
					if (!kTerW.empty()) u["extraTerrainWork"]        = picojson::value(kTerW);
					if (!kFeaA.empty()) u["extraFeatureAttack"]      = picojson::value(kFeaA);
					if (!kFeaD.empty()) u["extraFeatureDefense"]     = picojson::value(kFeaD);
					if (!kFeaW.empty()) u["extraFeatureWork"]        = picojson::value(kFeaW);
					if (!kInvVis.empty())  u["extraVisibilityIntensity"]      = picojson::value(kInvVis);
					if (!kInvInvis.empty())u["extraInvisibilityIntensity"]    = picojson::value(kInvInvis);
					if (!kInvVisR.empty()) u["extraVisibilityIntensityRange"] = picojson::value(kInvVisR);
					if (!kBuildWork.empty())u["extraWorkModForBuild"]         = picojson::value(kBuildWork);
				}
				// MEMBERSHIP (the per-source attribution axis -- the emulator sums each promotion/unitcombat's bundle
				// from the Info JSON, since CvUnit exposes only the aggregate above, §11.4):
				picojson::value::array kPromos;
				for (int p = 0; p < GC.getNumPromotionInfos(); ++p)
					if (pUnit->isHasPromotion((PromotionTypes)p))
						kPromos.push_back(picojson::value(std::string(GC.getPromotionInfo((PromotionTypes)p).getType())));
				u["promotions"] = picojson::value(kPromos);
				picojson::value::array kCombats;
				for (int c = 0; c < GC.getNumUnitCombatInfos(); ++c)
					if (pUnit->isHasUnitCombat((UnitCombatTypes)c))
						kCombats.push_back(picojson::value(std::string(GC.getUnitCombatInfo((UnitCombatTypes)c).getType())));
				u["unitCombats"] = picojson::value(kCombats);
				kUnits.push_back(picojson::value(u));
			}
			o["units"] = picojson::value((double)iUnits);
			o["unitsDetailed"] = picojson::value(kUnits);
			return CvString(picojson::value(o).serialize().c_str());
		}

		const int iIdx = GC.getInfoTypeForString(szType, true);
		if (iIdx < 0)
		{
			o["error"] = picojson::value(std::string("type not loaded this game"));
			return CvString(picojson::value(o).serialize().c_str());
		}

		CvEntityAvailability kAvail;
		std::string sNotes;
		const bool bParsed = cascadeReadJsonAvailability(szType, kAvail, sNotes);

		if (strcmp(szAction, "canConstruct") == 0)
		{
			o["city"] = picojson::value((double)iCityId);
			o["legacy"] = (pCity != NULL) ? picojson::value(pCity->canConstruct((BuildingTypes)iIdx)) : picojson::value();
			o["legacyReason"] = (pCity != NULL) ? picojson::value(std::string(legacyBlockReason(pCity, (BuildingTypes)iIdx))) : picojson::value();
			if (bParsed)
			{
				CvCascadeContext kCtx(iPlayer, iCityId);
				bool bC = cascadeBuildable(kAvail, COUNTDOMAIN_BUILDING, iIdx, kCtx);
				if (kAvail.notConstructible) bC = false;                                         // cost==-1: never player-constructible
				if (pCity != NULL && pCity->hasBuilding((BuildingTypes)iIdx)) bC = false;        // generation: already built here
				if (cascadeIsObsoleteForTeam(COUNTDOMAIN_BUILDING, iIdx, iTeam)) bC = false;      // generation: obsolete
				if (cascadeIsReplacedInCity(iIdx, kCtx)) bC = false;                              // a successor is active in the city
				if (!cascadeBuildingGroupAllows(iIdx, kCtx)) bC = false;                          // SpecialBuilding group cap
				o["cascade"] = picojson::value(bC);
				o["cascadeReason"] = picojson::value(std::string(cascadeBlockReason(kAvail, (BuildingTypes)iIdx, pCity, iTeam, kCtx)));
				rjAddCapShadow(o, kAvail, COUNTDOMAIN_BUILDING, iIdx, kCtx);
			}
			else o["cascade"] = picojson::value();
		}
		else if (strcmp(szAction, "canTrain") == 0)
		{
			o["city"] = picojson::value((double)iCityId);
			o["legacy"] = (pCity != NULL) ? picojson::value(pCity->canTrain((UnitTypes)iIdx)) : picojson::value();
			if (bParsed)
			{
				CvCascadeContext kCtx(iPlayer, iCityId);
				// all-branches-alive resolver: requires + cap + obsolete + the upgrade band (build-list model)
				o["cascade"] = picojson::value(cascadeUnitTrainable(iIdx, kCtx));
				rjAddCapShadow(o, kAvail, COUNTDOMAIN_UNIT, iIdx, kCtx);
			}
			else o["cascade"] = picojson::value();
		}
		else if (strcmp(szAction, "canResearch") == 0)
		{
			o["legacy"] = picojson::value(kPlayer.canResearch((TechTypes)iIdx));
			if (bParsed)
			{
				CvCascadeContext kCtx(iPlayer, -1); // tech is team-scope -- no city
				bool bC = cascadeEvalCondition(kAvail.requiresBuild, kCtx) && cascadeEvalCondition(kAvail.requiresOperate, kCtx);
				if (GET_TEAM((TeamTypes)iTeam).isHasTech((TechTypes)iIdx)) bC = false;            // generation: already researched
				o["cascade"] = picojson::value(bC);
				if (kAvail.allowedCap >= 0) sNotes += " [cap pending: tech not tallied]";
			}
			else o["cascade"] = picojson::value();
		}
		else if (strcmp(szAction, "canDoCivics") == 0)
		{
			o["legacy"] = picojson::value(kPlayer.canDoCivics((CivicTypes)iIdx));
			if (bParsed)
			{
				CvCascadeContext kCtx(iPlayer, -1);
				o["cascade"] = picojson::value(cascadeEvalCondition(kAvail.requiresBuild, kCtx) && cascadeEvalCondition(kAvail.requiresOperate, kCtx));
			}
			else o["cascade"] = picojson::value();
		}
		else if (strcmp(szAction, "canCreate") == 0)
		{
			o["legacy"] = picojson::value(kPlayer.canCreate((ProjectTypes)iIdx));
			if (bParsed)
			{
				CvCascadeContext kCtx(iPlayer, iCityId);
				o["cascade"] = picojson::value(cascadeEvalCondition(kAvail.requiresBuild, kCtx) && cascadeEvalCondition(kAvail.requiresOperate, kCtx));
			}
			else o["cascade"] = picojson::value();
		}
		else if (strcmp(szAction, "canMaintain") == 0)
		{
			o["legacy"] = picojson::value(kPlayer.canMaintain((ProcessTypes)iIdx));
			o["cascade"] = picojson::value();
			sNotes = "cascade: no JSON requires surface for processes yet";
		}
		else if (strcmp(szAction, "whyNot") == 0)
		{
			// Trace the legacy canTrain decision INPUTS for a UNIT -- the lit map of the canTrain cavern, so the
			// hide-reason is self-evident (e.g. obsoleteTechResearched:true). type= must be a UNIT_*.
			const UnitTypes eU = (UnitTypes)iIdx;
			const CvUnitInfo& kU = GC.getUnitInfo(eU);
			o["city"] = picojson::value((double)iCityId);
			o["legacyCanTrain"] = (pCity != NULL) ? picojson::value(pCity->canTrain(eU)) : picojson::value();

			const int iObs = kU.getObsoleteTech();
			if (iObs >= 0)
			{
				o["obsoleteTech"] = picojson::value(std::string(GC.getTechInfo((TechTypes)iObs).getType()));
				o["obsoleteTechResearched"] = picojson::value(GET_TEAM((TeamTypes)iTeam).isHasTech((TechTypes)iObs));
			}
			const int iPre = kU.getPrereqAndTech();
			if (iPre >= 0)
			{
				o["prereqAndTech"] = picojson::value(std::string(GC.getTechInfo((TechTypes)iPre).getType()));
				o["prereqAndTechResearched"] = picojson::value(GET_TEAM((TeamTypes)iTeam).isHasTech((TechTypes)iPre));
			}
			o["forceUpgrade"] = picojson::value(kU.isForceUpgrade());
			o["numUnitUpgrades"] = picojson::value((double)kU.getNumUnitUpgrades());
			if (pCity != NULL)
			{
				o["supersedingUnitAvailable"] = picojson::value(pCity->isSupersedingUnitAvailable(eU));
				o["canUpgradeUnit"] = picojson::value(pCity->canUpgradeUnit(eU));
				o["plotCanTrain"] = picojson::value(pCity->plot()->canTrain(eU, false));   // CvPlot gate: bonus/terrain/domain
			}
			// unit bonus prereqs (the cascade's requires source) + does THIS city actually have them (public reads)?
			{
				picojson::value::object bonuses;
				const int iVic = kU.getPrereqVicinityBonus();
				if (iVic >= 0 && pCity != NULL)
					bonuses[GC.getBonusInfo((BonusTypes)iVic).getType()] =
						picojson::value(std::string("vicinity:") + (pCity->hasVicinityBonus((BonusTypes)iVic) ? "yes" : "NO"));
				const int iAnd = kU.getPrereqAndBonus();
				if (iAnd >= 0 && pCity != NULL)
					bonuses[GC.getBonusInfo((BonusTypes)iAnd).getType()] =
						picojson::value(std::string("and:") + (pCity->hasBonus((BonusTypes)iAnd) ? "yes" : "NO"));
				const std::vector<BonusTypes>& vOr = kU.getPrereqOrBonuses();
				for (size_t i = 0; i < vOr.size(); ++i)
				{
					const int iOr = (int)vOr[i];
					if (iOr >= 0 && pCity != NULL)
						bonuses[GC.getBonusInfo((BonusTypes)iOr).getType()] = picojson::value(std::string("or:")
							+ ((pCity->hasBonus((BonusTypes)iOr) || pCity->hasVicinityBonus((BonusTypes)iOr)) ? "yes" : "NO"));
				}
				if (!bonuses.empty()) o["unitBonusPrereqs"] = picojson::value(bonuses);
			}
			sNotes = "legacy canTrain inputs (obsolete/prereq/forceUpgrade/superseding/plot/bonus)";
		}
		else if (strcmp(szAction, "tally") == 0)
		{
			// COUNT reconstruction (state-mapping gap #2): the cascade tally's count for this BUILDING_/UNIT_ at all three
			// cross-city scopes + the legacy engine count, so empire/team/world totals are point-in-time readable.
			const bool bUnit = (strncmp(szType, "UNIT_", 5) == 0);
			const CountDomain eDom = bUnit ? COUNTDOMAIN_UNIT : COUNTDOMAIN_BUILDING;
			CvCascadeContext kCtx(iPlayer, iCityId);
			picojson::value::object t;
			t["world"]  = picojson::value((double)cascadeTally().count(eDom, iIdx, COUNTSCOPE_WORLD,  kCtx.contextFor(COUNTSCOPE_WORLD)));
			t["team"]   = picojson::value((double)cascadeTally().count(eDom, iIdx, COUNTSCOPE_TEAM,   kCtx.contextFor(COUNTSCOPE_TEAM)));
			t["empire"] = picojson::value((double)cascadeTally().count(eDom, iIdx, COUNTSCOPE_EMPIRE, kCtx.contextFor(COUNTSCOPE_EMPIRE)));
			o["domain"] = picojson::value(std::string(bUnit ? "unit" : "building"));
			o["tally"] = picojson::value(t);
			sNotes = "cascade tally counts at world/team/empire (this player's team/empire)";
		}
		else
		{
			o["error"] = picojson::value(std::string("unknown diagnostic action"));
			return CvString(picojson::value(o).serialize().c_str());
		}

		o["notes"] = picojson::value(sNotes);
		return CvString(picojson::value(o).serialize().c_str());
	}

	// GAME THREAD (publishIfDue): if a diagnostic request is pending, render its answer and mark it done.
	void serviceEvalMailbox()
	{
		if (!g_bLockInitialized || g_evalState != EVAL_PENDING)
		{
			return; // fast idle peek -- no lock taken when nothing is pending
		}
		char szAction[40]; char szType[96]; int iPlayer; int iCity;
		EnterCriticalSection(&g_evalLock);
		if (g_evalState != EVAL_PENDING) { LeaveCriticalSection(&g_evalLock); return; }
		strncpy(szAction, g_evalAction, sizeof(szAction)); szAction[sizeof(szAction) - 1] = '\0';
		strncpy(szType, g_evalType, sizeof(szType)); szType[sizeof(szType) - 1] = '\0';
		iPlayer = g_evalPlayer;
		iCity = g_evalCity;
		LeaveCriticalSection(&g_evalLock);

		const CvString szResult = evaluateGate(szAction, szType, iPlayer, iCity); // safe: game thread

		EnterCriticalSection(&g_evalLock);
		g_evalResult = szResult;
		g_evalState = EVAL_DONE;
		LeaveCriticalSection(&g_evalLock);
	}

	// SERVER THREAD: enqueue a request, then wait (bounded) for the game thread to render the answer.
	bool evalRequestBlocking(const char* szAction, const char* szType, int iPlayer, int iCity, CvString& szAnswerOut)
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

		// Extract the request target ("/units?id=123") -- everything between the
		// method and the next space/CR, length-capped.
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

		if (strcmp(szTarget, "/") == 0)
		{
			sendResponse(sock, "200 OK", "text/plain", CvString("hello world\n"), snapshotTurn());
		}
		else if (strcmp(szTarget, "/units") == 0)
		{
			// Parse the supported filters; unknown parameters are ignored.
			bool bFilterId = false, bFilterOwner = false;
			int iId = -1, iOwner = -1;
			char* szTok = szQuery;
			while (szTok != NULL && *szTok != '\0')
			{
				char* szNext = strchr(szTok, '&');
				if (szNext != NULL)
				{
					*szNext = '\0';
					++szNext;
				}
				if (strncmp(szTok, "id=", 3) == 0)
				{
					bFilterId = true;
					iId = atoi(szTok + 3);
				}
				else if (strncmp(szTok, "playerNumber=", 13) == 0)
				{
					bFilterOwner = true;
					iOwner = atoi(szTok + 13);
				}
				szTok = szNext;
			}
			sendResponse(sock, "200 OK", "application/json", renderUnits(bFilterId, iId, bFilterOwner, iOwner), snapshotTurn());
		}
		else if (strcmp(szTarget, "/players") == 0)
		{
			bool bFilterOwner = false;
			int iOwner = -1;
			char* szTok = szQuery;
			while (szTok != NULL && *szTok != '\0')
			{
				char* szNext = strchr(szTok, '&');
				if (szNext != NULL)
				{
					*szNext = '\0';
					++szNext;
				}
				if (strncmp(szTok, "playerNumber=", 13) == 0)
				{
					bFilterOwner = true;
					iOwner = atoi(szTok + 13);
				}
				szTok = szNext;
			}
			sendResponse(sock, "200 OK", "application/json", renderPlayers(bFilterOwner, iOwner), snapshotTurn());
		}
		else if (strcmp(szTarget, "/cities") == 0)
		{
			bool bFilterId = false, bFilterOwner = false;
			int iId = -1, iOwner = -1;
			char* szTok = szQuery;
			while (szTok != NULL && *szTok != '\0')
			{
				char* szNext = strchr(szTok, '&');
				if (szNext != NULL)
				{
					*szNext = '\0';
					++szNext;
				}
				if (strncmp(szTok, "id=", 3) == 0)
				{
					bFilterId = true;
					iId = atoi(szTok + 3);
				}
				else if (strncmp(szTok, "playerNumber=", 13) == 0)
				{
					bFilterOwner = true;
					iOwner = atoi(szTok + 13);
				}
				szTok = szNext;
			}
			sendResponse(sock, "200 OK", "application/json", renderCities(bFilterId, iId, bFilterOwner, iOwner), snapshotTurn());
		}
		else if (strcmp(szTarget, "/events") == 0)
		{
			// SSE turn-event stream (#407): the response never ends and the socket
			// joins the broadcast list (the caller must keep it open).
			if (g_sseClients.size() >= SSE_CLIENT_CAP)
			{
				sendResponse(sock, "503 Service Unavailable", "application/json", CvString("{\"error\":\"too many event streams\"}\n"), snapshotTurn());
			}
			else
			{
				beginEventStream(sock);
				g_sseClients.push_back(sock);
				return true;
			}
		}
		else if (strcmp(szTarget, "/diagnostic") == 0)
		{
			sendResponse(sock, "200 OK", "application/json", CvString(
				"{\"endpoints\":["
				"\"/diagnostic/canConstruct?type=BUILDING_X&player=N\","
				"\"/diagnostic/canTrain?type=UNIT_X&player=N\","
				"\"/diagnostic/canResearch?type=TECH_X&player=N\","
				"\"/diagnostic/canDoCivics?type=CIVIC_X&player=N\","
				"\"/diagnostic/canCreate?type=PROJECT_X&player=N\","
				"\"/diagnostic/canMaintain?type=PROCESS_X&player=N\","
				"\"/diagnostic/sweep?type=buildings|units&player=N\","
				"\"/diagnostic/placementSweep?type=full&player=N\","
				"\"/diagnostic/dormancySweep?type=full&player=N\","
				"\"/diagnostic/modifierSweep?type=full|food|production|commerce&player=N\","
				"\"/diagnostic/movementSweep?type=full&player=N\","
				"\"/diagnostic/game?player=N\","
				"\"/diagnostic/modifier?player=N&city=M\","
				"\"/diagnostic/cityInput?player=N&city=M\","
				"\"/diagnostic/playerInput?player=N\","
				"\"/diagnostic/tally?type=BUILDING_X|UNIT_X&player=N\"],"
				"\"note\":\"player defaults to the active player; evaluated against the current game state, "
				"no construction performed; canConstruct also returns the cascade verdict + cap shadow; "
				"placementSweep is the auto-placement maintainer shadow (cascade-would-place vs legacy presence)\"}\n"),
				snapshotTurn());
		}
		else if (strncmp(szTarget, "/diagnostic/", 12) == 0)
		{
			const char* szAction = szTarget + 12; // e.g. "canConstruct"
			char szType[96];
			szType[0] = '\0';
			int iPlayer = -1; // -1 == the active player (resolved on the game thread)
			int iCity = -1;   // -1 == the player's capital (resolved on the game thread)
			char* szTok = szQuery;
			while (szTok != NULL && *szTok != '\0')
			{
				char* szNext = strchr(szTok, '&');
				if (szNext != NULL)
				{
					*szNext = '\0';
					++szNext;
				}
				if (strncmp(szTok, "type=", 5) == 0)
				{
					strncpy(szType, szTok + 5, sizeof(szType));
					szType[sizeof(szType) - 1] = '\0';
				}
				else if (strncmp(szTok, "player=", 7) == 0)
				{
					iPlayer = atoi(szTok + 7);
				}
				else if (strncmp(szTok, "city=", 5) == 0)
				{
					iCity = atoi(szTok + 5);
				}
				else if (strncmp(szTok, "channel=", 8) == 0 && szType[0] == '\0')
				{
					// modifierSweep channel scoping (spec §3.2 `?channel=food|production|commerce`) -- folded into
					// szType so the snapshot mailbox needs no extra param (only used when no explicit type= given).
					strncpy(szType, szTok + 8, sizeof(szType));
					szType[sizeof(szType) - 1] = '\0';
				}
				szTok = szNext;
			}
			// type= is required for the per-type gate actions; the roster sweeps + the game-state dump + the per-city
			// modifier / cityInput dumps need none.
			const bool bNoTypeAction = (strcmp(szAction, "placementSweep") == 0
				|| strcmp(szAction, "dormancySweep") == 0 || strcmp(szAction, "modifierSweep") == 0
				|| strcmp(szAction, "movementSweep") == 0
				|| strcmp(szAction, "game") == 0
				|| strcmp(szAction, "modifier") == 0 || strcmp(szAction, "cityInput") == 0
				|| strcmp(szAction, "playerInput") == 0);
			if (szType[0] == '\0' && !bNoTypeAction)
			{
				sendResponse(sock, "400 Bad Request", "application/json",
					CvString("{\"error\":\"missing required query param: type=PREFIX_NAME\"}\n"), snapshotTurn());
			}
			else
			{
				CvString szAnswer;
				if (evalRequestBlocking(szAction, szType, iPlayer, iCity, szAnswer))
				{
					sendResponse(sock, "200 OK", "application/json", szAnswer, snapshotTurn());
				}
				else
				{
					sendResponse(sock, "503 Service Unavailable", "application/json",
						CvString("{\"error\":\"eval busy or game thread not ticking; retry\"}\n"), snapshotTurn());
				}
			}
		}
		else if (strcmp(szTarget, "/extractor") == 0)
		{
			sendResponse(sock, "200 OK", "application/json", CvString(
				"{\"endpoints\":["
				"\"/extractor/gamestate[?player=N]\"],"
				"\"note\":\"the RAW game-state as one document along the spine "
				"world->teams->empires->areas->cities->plots. Raw facts only (no calculated values); the only "
				"map-number is distanceFromCapital. ?player=N restricts to one player. Spec: "
				"Tools/ModifierCalc/README.md\"}\n"), snapshotTurn());
		}
		else if (strncmp(szTarget, "/extractor/", 11) == 0)
		{
			const char* szSub = szTarget + 11; // e.g. "gamestate"
			int iPlayer = -1;                  // -1 == ALL players
			char* szTok = szQuery;
			while (szTok != NULL && *szTok != '\0')
			{
				char* szNext = strchr(szTok, '&');
				if (szNext != NULL) { *szNext = '\0'; ++szNext; }
				if (strncmp(szTok, "player=", 7) == 0) iPlayer = atoi(szTok + 7);
				szTok = szNext;
			}
			if (strcmp(szSub, "gamestate") == 0)
			{
				CvString szAnswer;
				if (evalRequestBlocking("gamestate", "", iPlayer, -1, szAnswer))
				{
					sendResponse(sock, "200 OK", "application/json", szAnswer, snapshotTurn());
				}
				else
				{
					sendResponse(sock, "503 Service Unavailable", "application/json",
						CvString("{\"error\":\"eval busy or game thread not ticking; retry\"}\n"), snapshotTurn());
				}
			}
			else
			{
				sendResponse(sock, "404 Not Found", "application/json",
					CvString("{\"error\":\"unknown extractor endpoint; see /extractor\"}\n"), snapshotTurn());
			}
		}
		else
		{
			sendResponse(sock, "404 Not Found", "application/json", CvString("{\"error\":\"not found\"}\n"), snapshotTurn());
		}
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
		g_pendingEvents.push_back(szFrame);
	}
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

	bst::shared_ptr<GameSnapshot> pNew(new GameSnapshot());
	pNew->iTurn = GC.getGame().getGameTurn();
	pNew->szGameId = GC.getGame().getGameId();
	pNew->units.reserve(4096);
	pNew->cities.reserve(256);

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		const CvPlayerAI& kPlayer = GET_PLAYER((PlayerTypes)iI);
		if (!kPlayer.isAlive())
		{
			continue;
		}
		foreach_(const CvUnit* pLoopUnit, kPlayer.units())
		{
			UnitSnap snap;
			snap.iID = pLoopUnit->getID();
			snap.iOwner = pLoopUnit->getOwner();
			snap.iX = pLoopUnit->getX();
			snap.iY = pLoopUnit->getY();
			snap.iDamage = pLoopUnit->getDamage();
			snap.iLevel = pLoopUnit->getLevel();
			snap.iBaseMoves = pLoopUnit->baseMoves();
			snap.iMaxMoves = pLoopUnit->maxMoves();
			snap.iMovesLeft = pLoopUnit->movesLeft();
			snap.iMoveDiscount = pLoopUnit->getExtraMoveDiscount();
			snap.iRange = pLoopUnit->airRange();
			snap.iDomain = (int)pLoopUnit->getDomainType();

			const UnitTypes eType = pLoopUnit->getUnitType();
			snap.szType = eType != NO_UNIT ? GC.getUnitInfo(eType).getType() : "NO_UNIT";

			const UnitAITypes eAI = pLoopUnit->AI_getUnitAIType();
			snap.szAI = eAI != NO_UNITAI ? GC.getUnitAIInfo(eAI).getType() : "NO_UNITAI";

			const CvSelectionGroup* pGroup = pLoopUnit->getGroup();
			snap.iGroup = pGroup != NULL ? pGroup->getID() : -1;
			snap.iMissionAI = pGroup != NULL ? pGroup->AI_getMissionAIType() : -1;
			snap.iActivity = pGroup != NULL ? pGroup->getActivityType() : -1;

			pNew->units.push_back(snap);
		}
	}

	// Per-team tech counts, computed once per team and shared by its members.
	std::vector<int> aiTeamTechs(MAX_TEAMS, -1);

	pNew->players.reserve(MAX_PLAYERS);
	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		const CvPlayerAI& kPlayer = GET_PLAYER((PlayerTypes)iI);
		if (!kPlayer.isAlive())
		{
			continue;
		}
		const TeamTypes eTeam = kPlayer.getTeam();
		if (aiTeamTechs[eTeam] == -1)
		{
			int iTechs = 0;
			for (int iTech = 0; iTech < GC.getNumTechInfos(); iTech++)
			{
				if (GET_TEAM(eTeam).isHasTech((TechTypes)iTech))
				{
					iTechs++;
				}
			}
			aiTeamTechs[eTeam] = iTechs;
		}

		PlayerSnap snap;
		snap.iID = iI;
		snap.iTeam = eTeam;
		snap.iHuman = kPlayer.isHuman() ? 1 : 0;
		snap.iNPC = kPlayer.isNPC() ? 1 : 0;
		snap.iScore = GC.getGame().getPlayerScore((PlayerTypes)iI);
		snap.iEra = kPlayer.getCurrentEra();
		snap.iTechs = aiTeamTechs[eTeam];
		snap.iCities = kPlayer.getNumCities();
		snap.iPopulation = kPlayer.getTotalPopulation();
		snap.iUnits = kPlayer.getNumUnits();
		snap.iGold = kPlayer.getGold();
		snap.iGoldRate = kPlayer.calculateGoldRate();
		snap.iScienceRate = kPlayer.getCommerceRate(COMMERCE_RESEARCH);

		// One walk over the player's cities: the player's production total and the
		// /cities snapshot rows.
		int iProduction = 0;
		foreach_(const CvCity* pLoopCity, kPlayer.cities())
		{
			CitySnap city;
			city.iID = pLoopCity->getID();
			city.iOwner = iI;
			city.iX = pLoopCity->getX();
			city.iY = pLoopCity->getY();
			city.iPopulation = pLoopCity->getPopulation();
			city.iFood = pLoopCity->getYieldRate(YIELD_FOOD);
			city.iProduction = pLoopCity->getYieldRate(YIELD_PRODUCTION);
			city.iCommerce = pLoopCity->getYieldRate(YIELD_COMMERCE);
			city.iNumBuildings = pLoopCity->getNumBuildings();
			city.iCultureLevel = pLoopCity->getCultureLevel();
			city.iCapital = pLoopCity->isCapital() ? 1 : 0;

			const CvProperties* pProps = pLoopCity->getPropertiesConst();
			const PropertyTypes eCrime = GC.getPROPERTY_CRIME();
			const PropertyTypes eEducation = GC.getPROPERTY_EDUCATION();
			const PropertyTypes eDisease = GC.getPROPERTY_DISEASE();
			city.iCrime = eCrime > NO_PROPERTY ? pProps->getValueByProperty(eCrime) : 0;
			city.iEducation = eEducation > NO_PROPERTY ? pProps->getValueByProperty(eEducation) : 0;
			city.iDisease = eDisease > NO_PROPERTY ? pProps->getValueByProperty(eDisease) : 0;

			const UnitTypes eProdUnit = pLoopCity->getProductionUnit();
			const BuildingTypes eProdBuilding = pLoopCity->getProductionBuilding();
			const ProjectTypes eProdProject = pLoopCity->getProductionProject();
			const ProcessTypes eProdProcess = pLoopCity->getProductionProcess();
			if (eProdUnit != NO_UNIT) city.szProducing = GC.getUnitInfo(eProdUnit).getType();
			else if (eProdBuilding != NO_BUILDING) city.szProducing = GC.getBuildingInfo(eProdBuilding).getType();
			else if (eProdProject != NO_PROJECT) city.szProducing = GC.getProjectInfo(eProdProject).getType();
			else if (eProdProcess != NO_PROCESS) city.szProducing = GC.getProcessInfo(eProdProcess).getType();
			else city.szProducing = "NONE";
			city.iProducingTurns = city.szProducing != "NONE" && eProdProcess == NO_PROCESS
				? pLoopCity->getProductionTurnsLeft() : 0;

			city.szName = narrowToAscii(pLoopCity->getName());
			pNew->cities.push_back(city);

			iProduction += city.iProduction;
		}
		snap.iProduction = iProduction;

		const CivilizationTypes eCiv = kPlayer.getCivilizationType();
		snap.szCiv = eCiv != NO_CIVILIZATION ? GC.getCivilizationInfo(eCiv).getType() : "NO_CIVILIZATION";
		snap.szName = narrowToAscii(kPlayer.getName());

		const TechTypes eResearch = kPlayer.getCurrentResearch();
		snap.szResearch = eResearch != NO_TECH ? GC.getTechInfo(eResearch).getType() : "NONE";

		const HandicapTypes eHandicap = kPlayer.getHandicapType();
		snap.szHandicap = eHandicap != NO_HANDICAP ? GC.getHandicapInfo(eHandicap).getType() : "NO_HANDICAP";

		pNew->players.push_back(snap);
	}

	EnterCriticalSection(&g_snapshotLock);
	g_pSnapshot = pNew;
	LeaveCriticalSection(&g_snapshotLock);
}
