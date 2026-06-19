#include "CvGameCoreDLL.h"
#include "CvHttpServer.h"
#include "CvBuildingInfo.h"
#include "CvBonusInfo.h" // bonus-name resolution in the /diagnostic/whyNot trace
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
					"\"group\":%d,\"missionAI\":%d,\"activity\":%d,\"damage\":%d,\"level\":%d}",
					u.iID, u.iOwner, u.iX, u.iY, u.szType.c_str(), u.szAI.c_str(),
					u.iGroup, u.iMissionAI, u.iActivity, u.iDamage, u.iLevel);
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

			// the present-building loadout (the pilot's cascade deposit source -- city-scope buildings)
			picojson::value::array kBldgs;
			for (int b = 0; b < GC.getNumBuildingInfos(); ++b)
			{
				if (pCity->hasBuilding((BuildingTypes)b))
					kBldgs.push_back(picojson::value(std::string(GC.getBuildingInfo((BuildingTypes)b).getType())));
			}
			o["buildings"] = picojson::value(kBldgs);

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
				const int iBase = pCity->getBaseYieldRate((YieldTypes)aFam[f]);
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
				"\"/diagnostic/game?player=N\","
				"\"/diagnostic/modifier?player=N&city=M\","
				"\"/diagnostic/cityInput?player=N&city=M\","
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
				szTok = szNext;
			}
			// type= is required for the per-type gate actions; the roster sweeps + the game-state dump + the per-city
			// modifier / cityInput dumps need none.
			const bool bNoTypeAction = (strcmp(szAction, "placementSweep") == 0
				|| strcmp(szAction, "dormancySweep") == 0 || strcmp(szAction, "game") == 0
				|| strcmp(szAction, "modifier") == 0 || strcmp(szAction, "cityInput") == 0);
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
