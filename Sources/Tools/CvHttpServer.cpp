#include "CvGameCoreDLL.h"
#include "CvHttpServer.h"
#include "CvBuildingInfo.h"
#include "CvBonusInfo.h" // bonus-name resolution in the /diagnostic/whyNot trace
#include "CvImprovementInfo.h" // cityInput loadout: worked-plot improvement type
#include "CvTraitInfo.h" // cityInput loadout: player trait list
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

	CvString evaluateGate(const char* szAction, const char* szType, int iPlayer, int iCityReq)
	{
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
			const int aFam[3] = { YIELD_FOOD, YIELD_PRODUCTION, YIELD_COMMERCE };
			const char* aFamName[3] = { "food", "production", "commerce" };
			int iOnly = -1; // channel scoping: type=food|production|commerce -> just that family
			for (int f = 0; f < 3; ++f) if (strcmp(szType, aFamName[f]) == 0) iOnly = f;

			int iCities = 0, iCells = 0, iAgree = 0, iDiv = 0;
			int aCare[NUM_MODIFIER_CARE_LEVELS] = { 0 };
			picojson::value::array kDiv, kCells;
			std::map<std::string, int> kCause; // UNCAPPED divergence histogram by "cause:CareName"
			int iCityIter = 0;
			for (CvCity* pCity = kPlayer.firstCity(&iCityIter); pCity != NULL; pCity = kPlayer.nextCity(&iCityIter))
			{
				++iCities;
				CvCascadeContext kCtx(iPlayer, pCity->getID());
				for (int f = 0; f < 3; ++f)
				{
					if (iOnly >= 0 && f != iOnly) continue;
					++iCells;
					CvModifierSlot slot;
					cascadeModifierCitySlot(aFam[f], kCtx, slot);
					const int iBase = cascadeModifierCityBase(pCity, aFam[f]); // base + specialist (legacy parity, CvCity.cpp:11253)
					const int iCascade = cascadeModifierApply(slot, iBase);
					const int iLegacy = pCity->getYieldRate100((YieldTypes)aFam[f]) / 100;
					int iCare = 0;
					const char* szCause = cascadeModifierClassify(iCascade, iLegacy, slot, iCare);
					if (iCare >= 0 && iCare < NUM_MODIFIER_CARE_LEVELS) ++aCare[iCare];
					if (iCascade == iLegacy) ++iAgree;
					else { ++iDiv; kCause[CvString::format("%s:%s", szCause, cascadeModifierCareName(iCare)).c_str()]++; }

					if (bFull && (int)kCells.size() < 4000)
					{
						picojson::value::object c;
						c["city"]      = picojson::value((double)pCity->getID());
						c["channel"]   = picojson::value(std::string(aFamName[f]));
						c["base"]      = picojson::value((double)iBase);
						c["flat"]      = picojson::value((double)slot.iFlat);
						c["percent"]   = picojson::value((double)slot.iPercent);
						c["mult100"]   = picojson::value((double)slot.iMultiplierX100);
						c["cascade"]   = picojson::value((double)iCascade);
						c["legacy"]    = picojson::value((double)iLegacy);
						c["legacy100"] = picojson::value((double)pCity->getYieldRate100((YieldTypes)aFam[f]));
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
						e["channel"]  = picojson::value(std::string(aFamName[f]));
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
			o["channel"]    = picojson::value(std::string(iOnly >= 0 ? aFamName[iOnly] : "all"));
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
				kPlots.push_back(picojson::value(pl));
			}
			o["plots"] = picojson::value(kPlots);

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
				e["baseExtra100"]   = picojson::value((double)pCity->getBaseCommerceRateExtra(eC));  // x100 base extras
				e["totalModifier"]  = picojson::value((double)pCity->getTotalCommerceRateModifier(eC)); // base 100
				e["prodToCommerce"] = picojson::value((double)pCity->getProductionToCommerceModifier(eC));
				e["realized100"]    = picojson::value((double)pCity->getCommerceRateTimes100(eC));   // ground truth (x100)
				kCommerce.push_back(picojson::value(e));
			}
			o["commerce"] = picojson::value(kCommerce);

			// ---- CH.4 DEFENSE (legacy-value-calc-map §4): max(building,natural)+playerMod+bonus, then damage-decay floored at extraMin ----
			{
				picojson::value::object d;
				d["totalDefense"]              = picojson::value((double)pCity->getTotalDefense(false));
				d["defenseModifier"]           = picojson::value((double)pCity->getDefenseModifier(false)); // realized
				d["buildingDefense"]           = picojson::value((double)pCity->getBuildingDefense());
				d["naturalDefense"]            = picojson::value((double)pCity->getNaturalDefense());
				d["playerCityDefenseModifier"] = picojson::value((double)kPlayer.getCityDefenseModifier());
				d["bonusDefense"]              = picojson::value((double)pCity->calculateBonusDefense());
				d["defenseDamage"]             = picojson::value((double)pCity->getDefenseDamage());
				d["maxDefenseDamage"]          = picojson::value((double)GC.getMAX_CITY_DEFENSE_DAMAGE());
				d["extraMinDefense"]           = picojson::value((double)pCity->getExtraMinDefense());
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
				m["effectiveModifier"]   = picojson::value((double)pCity->getEffectiveMaintenanceModifier());
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
				h["totalGoodBuildingHealth"] = picojson::value((double)pCity->totalGoodBuildingHealth());
				h["totalBadBuildingHealth"]  = picojson::value((double)pCity->totalBadBuildingHealth());
				h["extraHealth"]             = picojson::value((double)pCity->getExtraHealth());
				h["improvementGoodHealth"]   = picojson::value((double)pCity->getImprovementGoodHealth());
				h["improvementBadHealth"]    = picojson::value((double)pCity->getImprovementBadHealth());
				h["specialistGoodHealth"]    = picojson::value((double)pCity->getSpecialistGoodHealth());
				h["specialistBadHealth"]     = picojson::value((double)pCity->getSpecialistBadHealth());
				h["corporationHealth"]       = picojson::value((double)pCity->calculateCorporationHealth());
				h["extraTechHealth"]         = picojson::value((double)pCity->getExtraTechHealthTotal());
				h["espionageHealthCounter"]  = picojson::value((double)pCity->getEspionageHealthCounter());
				h["unhealthyPopulation"]     = picojson::value((double)pCity->unhealthyPopulation());
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
				o["happiness"] = picojson::value(hp);
			}

			// ---- CH greatPeople (legacy-value-calc-map §9.5): base x modifier/100 (disorder -> 0) ----
			{
				picojson::value::object gp;
				gp["greatPeopleRate"]     = picojson::value((double)pCity->getGreatPeopleRate()); // realized
				gp["baseGreatPeopleRate"] = picojson::value((double)pCity->getBaseGreatPeopleRate());
				gp["totalGPRateModifier"] = picojson::value((double)pCity->getTotalGreatPeopleRateModifier());
				gp["greatPeopleProgress"] = picojson::value((double)pCity->getGreatPeopleProgress());
				gp["threshold"]           = picojson::value((double)kPlayer.greatPeopleThresholdNonMilitary());
				o["greatPeople"] = picojson::value(gp);
			}

			// ---- CH tradeRoutes (legacy-value-calc-map §9.5): count + realized trade yields (per-partner profit not reproduced offline) ----
			{
				picojson::value::object tr;
				tr["tradeRoutes"]          = picojson::value((double)pCity->getTradeRoutes());
				tr["maxTradeRoutes"]       = picojson::value((double)pCity->getMaxTradeRoutes());
				tr["tradeYieldFood"]       = picojson::value((double)pCity->getTradeYield(YIELD_FOOD));
				tr["tradeYieldProduction"] = picojson::value((double)pCity->getTradeYield(YIELD_PRODUCTION));
				tr["tradeYieldCommerce"]   = picojson::value((double)pCity->getTradeYield(YIELD_COMMERCE));
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
				bl["espionageDefenseModifier"]   = picojson::value((double)pCity->getEspionageDefenseModifier());
				bl["healRate"]                   = picojson::value((double)pCity->getHealRate());
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
			const int aFam[3] = { YIELD_FOOD, YIELD_PRODUCTION, YIELD_COMMERCE };
			const char* aFamName[3] = { "food", "production", "commerce" };
			picojson::value::array kFam;
			for (int f = 0; f < 3; ++f)
			{
				CvModifierSlot slot;
				cascadeModifierCitySlot(aFam[f], kCtx, slot);
				const int iBase = cascadeModifierCityBase(pCity, aFam[f]); // base + specialist (legacy parity, CvCity.cpp:11253)
				picojson::value::object e;
				e["family"]    = picojson::value(std::string(aFamName[f]));
				e["base"]      = picojson::value((double)iBase);
				e["flat"]      = picojson::value((double)slot.iFlat);
				e["percent"]   = picojson::value((double)slot.iPercent);
				e["mult100"]   = picojson::value((double)slot.iMultiplierX100);
				e["cascade"]   = picojson::value((double)cascadeModifierApply(slot, iBase)); // active calc-flow (legacy-flat-outside)
				e["legacy100"] = picojson::value((double)pCity->getYieldRate100((YieldTypes)aFam[f]));
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
