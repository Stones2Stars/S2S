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

	// --- game-thread gate/state evaluation -----------------------------------------------
	// GAME-THREAD ONLY: read live CvPlayer/CvCity/CvGame state and render the JSON answer for a
	// /state or /computed route. Called from serviceEvalMailbox (the server thread may not touch game objects).

	// WHY does the engine's canConstruct block eBuilding in pCity? Returns the FIRST failing legacy gate by
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
		if (!pCity->isValidBuildingLocation(eBuilding))
		{
			// decompose isValidBuildingLocation (CvCity.cpp:18540) into its sub-gates so a "location" failure is attributable
			if (kB.isWater())
			{
				if ((!kB.isRiver() || !pCity->plot()->isRiver()) && !pCity->isCoastal(kB.getMinAreaSize())) return "locCoastalArea";
			}
			else if (pCity->area()->getNumTiles() < kB.getMinAreaSize()) return "locLandArea";
			else if (kB.isRiver() && !pCity->plot()->isRiver()) return "locRiver";
			if (!pCity->isValidTerrainForBuildings(eBuilding)) return "locTerrain";
			if (kB.isFreshWater() && !pCity->plot()->isFreshWater()) return "locFreshwater";
			return "locMapCategory";
		}
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
		// --- PLAYER-level gates (CvPlayer::canConstructInternal:6571 -- NOT replicated by the city-side checks above;
		//     this residual otherwise collapses into "other") ---
		if (kP.getNumCities() < kB.getNumCitiesPrereq()) return "numCities";
		if (kP.getHighestUnitLevel() < kB.getUnitLevelPrereq()) return "unitLevel";
		if (GC.getGame().isNoNukes() && kB.isAllowsNukes()) return "noNukes";
		if (eSB != NO_SPECIALBUILDING && !GC.getGame().isSpecialBuildingValid(eSB)) return "specialBuildingInvalid";
		if (!(*kP.getPropertiesConst() <= *(kB.getPrereqPlayerMaxProperties()))) return "playerMaxProperty";
		if (!(*kP.getPropertiesConst() >= *(kB.getPrereqPlayerMinProperties()))) return "playerMinProperty";
		if (kB.isPrereqPower() && !pCity->isPower()) return "prereqPower";
		// making-count cap variants (canConstructInternal bTestVisible block; legacyBlockReason only had the no-making forms)
		if (GC.getGame().isBuildingMaxedOut(eBuilding, kT.getBuildingMaking(eBuilding))
		||  kT.isBuildingMaxedOut(eBuilding, kT.getBuildingMaking(eBuilding))
		||  kP.isBuildingMaxedOut(eBuilding, kP.getBuildingMaking(eBuilding))) return "capMaking";
		if (eSB != NO_SPECIALBUILDING && kP.isBuildingGroupMaxedOut(eSB, kP.getBuildingGroupMaking(eSB))) return "groupCapMaking";
		// the GOM ConstructCondition BoolExpr (CvCity::canConstructInternal:2986) -- the one gate the typed re-walk above
		// does NOT cover (CLAY_PIT-class). Evaluated against the city game-object exactly as the engine does.
		if (kB.getConstructCondition() != NULL && !kB.getConstructCondition()->evaluate(pCity->getGameObject())) return "constructCondition";
		return "other";
	}

	// ====================================================================================================
	// /extractor -- the RAW game-state dump (world -> teams -> empires -> areas -> cities -> plots).
	// CLEAN, raw-FACTS-ONLY extraction: NO calculated value ever appears here (DEC-calc-zero-ride-in). The
	// only map-derived number is distanceFromCapital. This is the dedicated extraction surface -- read it
	// directly, feed it to StoneBase (the validator), or build features on it. Spec:
	// docs/specs/json.md. Runs on the game thread (mailbox), where every fact is readable.
	// ====================================================================================================
	picojson::value::object extractCity(CvPlayer& kPlayer, CvCity* pCity, int iTeam)
	{
		picojson::value::object c;
		// City identity: id is unique only WITHIN a player, so always pair it with owner (the globally-unique
		// tuple) + name + x/y, per docs/specs/http-endpoints.md "City identity". `globalId` is the derived
		// "<PP>-<id>" snowflake (owner zero-padded to 2 digits) -- a single stable globally-unique reference for
		// the API + the in-game hover tooltip; decode back to the (owner,id) tuple by splitting on '-'.
		c["owner"]       = picojson::value((double)pCity->getOwner());
		c["id"]          = picojson::value((double)pCity->getID());
		c["globalId"]    = picojson::value(std::string(CvString::format("%02d-%d", pCity->getOwner(), pCity->getID()).GetCString()));
		c["name"]        = picojson::value(std::string(narrowToAscii(pCity->getName()).GetCString()));
		c["population"]  = picojson::value((double)pCity->getPopulation());
		c["latitude"]    = picojson::value((double)pCity->plot()->getLatitude());  // building MinLatitude/MaxLatitude gate (canConstruct)
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

		// FOLD-IN INPUT yields (out-of-scope for drycalc -> belong in /state per http-endpoints.md:42 "drycalc folds
		// that yield in, it does not compute it"). Per-channel: the resolved trade-route yield (getTradeYield), the
		// player free-city yield (getFreeCityYield), and the city's raw savegame event-granted extra (m_aiExtraYield,
		// reconstructed = getExtraYield100 - building-flats100 - perPop*pop, /100 -- the only getExtraYield100 part the
		// cascade does NOT compute itself). getYieldRate100 base = plot + trade + freeCity + goldenAge; extra bucket =
		// cityExtraYield + buildingFlats + perPop (calc-map §1.1/§1.2).
		picojson::value::object tradeY, freeCityY, cityExtraY;
		for (int y = 0; y < NUM_YIELD_TYPES; ++y)
		{
			const char* yn = GC.getYieldInfo((YieldTypes)y).getType();
			tradeY[yn]    = picojson::value((double)pCity->getTradeYield((YieldTypes)y));
			freeCityY[yn] = picojson::value((double)kPlayer.getFreeCityYield((YieldTypes)y));
			const int iRawExtra = (pCity->getExtraYield100((YieldTypes)y) - pCity->getBuildingExtraYield100((YieldTypes)y)
			                       - pCity->getBaseYieldPerPopRate((YieldTypes)y) * pCity->getPopulation()) / 100;
			cityExtraY[yn] = picojson::value((double)iRawExtra);
		}
		c["tradeYield"]     = picojson::value(tradeY);
		c["freeCityYield"]  = picojson::value(freeCityY);
		c["cityExtraYield"] = picojson::value(cityExtraY);

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

		// LOCATION model (isValidBuildingLocation, CvCity.cpp:18540): a bWater building needs isCoastal(minAreaSize)
		// = an adjacent water area >= minAreaSize tiles; a non-water building needs the city's LAND area >= minAreaSize.
		// Emit both so the cascade can evaluate the minAreaSize gate (cascade had only the boolean `coast`).
		c["landArea"] = picojson::value((double)pCity->plot()->area()->getNumTiles());
		{
			int iMaxAdjWater = 0;
			for (int d = 0; d < NUM_DIRECTION_TYPES; ++d)
			{
				const CvPlot* pAdj = plotDirection(pCity->getX(), pCity->getY(), (DirectionTypes)d);
				if (pAdj != NULL && pAdj->isWater())
				{
					const int n = pAdj->area()->getNumTiles();
					if (n > iMaxAdjWater) iMaxAdjWater = n;
				}
			}
			c["maxAdjWater"] = picojson::value((double)iMaxAdjWater);
		}

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
		// NB the per-city canConstruct/canTrain verdicts (a drycalc TARGET) are NOT emitted here -- /state is
		// inputs only. They live on /computed/canConstruct?type=...&city=... (see docs/specs/http-endpoints.md).

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
			// bonusConnected: this plot's bonus counts toward hasVicinityBonus (the OBTAINED/improved semantic) --
			// owned by the city + isHasValidBonus (revealed + tech + improvement) + isConnectedTo this city
			// (CvCity::hasVicinityBonus:21110). The OWN-centre plot (isCity) counts unconditionally (handled cascade-side).
			if (eB != NO_BONUS && pPlot->getOwner() == pCity->getOwner()
				&& pPlot->isHasValidBonus() && pPlot->isConnectedTo(pCity))
				pl["bonusConnected"] = picojson::value(true);
			if (pPlot->isRiver())               pl["river"] = picojson::value(true);
			if (pPlot->isIrrigationAvailable()) pl["irrig"] = picojson::value(true);
			if (pPlot->isHills())               pl["hills"] = picojson::value(true);
			if (pPlot->isPeak() || pPlot->isAsPeak()) pl["peak"] = picojson::value(true);
			// relief = REAL plotType (getPlotType): getBaseYield keys getPeakChange/getHillsChange on THIS, not on
			// isPeak()||isAsPeak() -- a feature-induced as-peak (Kilimanjaro on flat land) is PLOT_LAND, no relief base yield.
			{ const PlotTypes eRel = pPlot->getPlotType();
			  if (eRel == PLOT_PEAK)       pl["relief"] = picojson::value(std::string("PEAK"));
			  else if (eRel == PLOT_HILLS) pl["relief"] = picojson::value(std::string("HILLS")); }
			if (pPlot->isWater())               pl["water"] = picojson::value(true);
			if (pPlot->isCoastalLand())         pl["coast"] = picojson::value(true);  // coastal land (HAS_COAST predicate)
			if (pPlot->isFreshWater())          pl["freshwater"] = picojson::value(true);  // HAS_FRESHWATER (river OR adjacent lake)
			if (pPlot == pCity->plot())         pl["isCity"] = picojson::value(true);  // THIS city's OWN centre plot (not a neighbour city-centre that overlaps the workable radius) -- the centre-plot facts HAS_RIVER/COAST/FRESHWATER/latitude read it
			if (pPlot->getTeam() == pCity->getTeam()) pl["owned"] = picojson::value(true);  // owned by the city's team -- isValidTerrainForBuildings counts a terrain/improvement/peak/hill plot only if owned (CvCity.cpp:20450/20526)
			else if (pPlot->getTeam() == NO_TEAM) pl["neutral"] = picojson::value(true);  // unowned -- a FEATURE prereq counts a neutral plot too unless GAMEOPTION_EXP_STRICT_VICINITY (CvCity.cpp:20556)
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

		// world buildingsCreated -- getBuildingCreatedCount per building (CvGame::isBuildingMaxedOut, the world-wonder
		// cap gate, CvGame.cpp:5118). CUMULATIVE ever-created (persists past obsolescence/loss), NOT the current
		// getBuildingCount -- a world wonder created then obsoleted still caps. Only non-zero emitted. The dry-calc's
		// world-scope `allowed` cap reads THIS, not the sum of empires' current buildingCounts.
		{
			picojson::value::object created;
			for (int iB = 0; iB < GC.getNumBuildingInfos(); ++iB)
			{
				const int n = GC.getGame().getBuildingCreatedCount((BuildingTypes)iB);
				if (n > 0) created[GC.getBuildingInfo((BuildingTypes)iB).getType()] = picojson::value((double)n);
			}
			world["buildingsCreated"] = picojson::value(created);
		}

		// world unitsCreated -- getUnitCreatedCount per unit (CvGame::isUnitMaxedOut world-cap gate, CvGame.cpp:5098:
		// getUnitCreatedCount + iExtra >= getMaxGlobalInstances). CUMULATIVE lifetime-created (a hero born once then
		// poofed still consumes its world slot -- tally.md), NOT the live unitCount. The dry-calc's world-scope unit
		// `allowed` cap reads THIS. Only non-zero emitted.
		{
			picojson::value::object uCreated;
			for (int iU = 0; iU < GC.getNumUnitInfos(); ++iU)
			{
				const int n = GC.getGame().getUnitCreatedCount((UnitTypes)iU);
				if (n > 0) uCreated[GC.getUnitInfo((UnitTypes)iU).getType()] = picojson::value((double)n);
			}
			world["unitsCreated"] = picojson::value(uCreated);
		}

		// world victories -- the ENABLED victory conditions (isVictoryValid). A building's VictoryPrereq gate
		// (CvCity::canConstruct, e.g. UN_MISSION needs the diplomatic victory enabled) reads this.
		{
			picojson::value::array vics;
			for (int v = 0; v < GC.getNumVictoryInfos(); ++v)
				if (GC.getGame().isVictoryValid((VictoryTypes)v))
					vics.push_back(picojson::value(std::string(GC.getVictoryInfo((VictoryTypes)v).getType())));
			world["victories"] = picojson::value(vics);
		}

		// world.config -- RAW game-define scalars the calc needs that are NOT entity data. Resolved game-side
		// (authoritative; e.g. the world-size trade-profit % needs no world-size guess offline). The TRADE block
		// feeds the trade-route profit/yield port (calc-map 9.5): the profit defines + the per-yield YieldInfo
		// trade-modifier BASE that seeds getTradeYieldModifier (CvPlayer.cpp:397). The deposit-modeled trade
		// modifiers (building/civic/trait/tech) are NOT here -- the cascade computes those from Assets/Data.
		picojson::value::object config;
		config["theirPopulationTradePercent"]   = picojson::value((double)GC.getTHEIR_POPULATION_TRADE_PERCENT());
		config["tradeProfitPercent"]            = picojson::value((double)GC.getTRADE_PROFIT_PERCENT());
		config["worldTradeProfitPercent"]       = picojson::value((double)GC.getWorldInfo(GC.getMap().getWorldSize()).getTradeProfitPercent());
		config["buildingPrereqModifier"]        = picojson::value((double)GC.getWorldInfo(GC.getMap().getWorldSize()).getBuildingPrereqModifier()); // PrereqNumOfBuildings scaling (CvPlayer::getBuildingPrereqBuilding)
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
		picojson::value::object yCityChange, yPopDiv, yGAYield, yGAThresh, yHills, yPeak, yMinCity;
		for (int y = 0; y < NUM_YIELD_TYPES; ++y)
		{
			const char* yn = GC.getYieldInfo((YieldTypes)y).getType();
			yCityChange[yn] = picojson::value((double)GC.getYieldInfo((YieldTypes)y).getCityChange());
			yPopDiv[yn]     = picojson::value((double)GC.getYieldInfo((YieldTypes)y).getPopulationChangeDivisor());
			// per-plot GOLDEN-AGE yield: +getGoldenAgeYield on each worked plot whose yield >= threshold (calculateYield)
			yGAYield[yn]    = picojson::value((double)GC.getYieldInfo((YieldTypes)y).getGoldenAgeYield());
			yGAThresh[yn]   = picojson::value((double)GC.getYieldInfo((YieldTypes)y).getGoldenAgeYieldThreshold());
			// PLOT-TYPE base yield: a PEAK plot gets getPeakChange, a HILLS plot getHillsChange -- part of
			// getBaseYield -> calculateNatureYield (CvPlot::recalculateBaseYield, the nature addend).
			yHills[yn]      = picojson::value((double)GC.getYieldInfo((YieldTypes)y).getHillsChange());
			yPeak[yn]       = picojson::value((double)GC.getYieldInfo((YieldTypes)y).getPeakChange());
			// city-CENTRE floor: CvPlot::calculateYield does max(getMinCity, yield) on the isCity() plot (calc-map §10.1)
			yMinCity[yn]    = picojson::value((double)GC.getYieldInfo((YieldTypes)y).getMinCity());
		}
		config["yieldCityChange"] = picojson::value(yCityChange);
		config["yieldPopulationChangeDivisor"] = picojson::value(yPopDiv);
		config["yieldGoldenAgeYield"] = picojson::value(yGAYield);
		config["yieldGoldenAgeThreshold"] = picojson::value(yGAThresh);
		config["yieldHillsChange"] = picojson::value(yHills);
		config["yieldPeakChange"] = picojson::value(yPeak);
		config["yieldMinCity"] = picojson::value(yMinCity);
		// +/- this on each worked plot whose RUNNING pre-improvement yield clears a player extra/less-yield
		// threshold (CvPlot::calculateYield 8393-8401; the Industrious/Nomad-style trait mechanic). A single global define.
		config["extraYield"] = picojson::value((double)GC.getDefineINT("EXTRA_YIELD"));
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
		// world.noNukes -- the DYNAMIC no-nukes state (NOT the GAMEOPTION_NO_NUKES option; e.g. a UN resolution).
		// CvPlayer::canConstruct rejects an isAllowsNukes building while isNoNukes() holds (CvPlayer.cpp:6746), so the
		// cascade must drop allowsNukes buildings (MANHATTAN_PROJECT) when this is true.
		world["noNukes"] = picojson::value(kGame.isNoNukes());

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

				// NB the availability verdicts (canResearch / canDoCivics / build-unlock) are drycalc TARGETS, so they
				// are NOT emitted on /state -- they live on /computed/availableTechs|availableCivics|availableBuilds.

				picojson::value::object sliders;
				for (int cc = 0; cc < 4; ++cc)
					sliders[aCommName[cc]] = picojson::value((double)kPlayer.getCommercePercent((CommerceTypes)aCommType[cc]));
				emp["sliders"] = picojson::value(sliders);

				emp["cityCount"] = picojson::value((double)kPlayer.getNumCities());

				// empire-wide TALLY -- the engine's own live per-player counters (CvPlayer::changeBuildingCount /
				// changeUnitCount, maintained in handleBuildingCounts). This is the aggregate count for empire-scope
				// `requires.build` count atoms (min(BUILDING_X,N) / min(UNIT_X,N)) StoneBase cannot roll up
				// without seeing every city; we emit the counter directly (no re-derivation).
				picojson::value::object bldgCounts;
				picojson::value::object bldgMaking;   // in-PRODUCTION count (getBuildingMaking) -- canConstruct counts these toward the instance cap (capMaking)
				for (int bc = 0; bc < GC.getNumBuildingInfos(); ++bc)
				{
					const int n = kPlayer.getBuildingCount((BuildingTypes)bc);
					if (n > 0) bldgCounts[GC.getBuildingInfo((BuildingTypes)bc).getType()] = picojson::value((double)n);
					const int m = kPlayer.getBuildingMaking((BuildingTypes)bc);
					if (m > 0) bldgMaking[GC.getBuildingInfo((BuildingTypes)bc).getType()] = picojson::value((double)m);
				}
				emp["buildingCounts"] = picojson::value(bldgCounts);
				emp["buildingsMaking"] = picojson::value(bldgMaking);
				picojson::value::object unitCounts;
				for (int uc = 0; uc < GC.getNumUnitInfos(); ++uc)
				{
					const int n = kPlayer.getUnitCount((UnitTypes)uc);
					if (n > 0) unitCounts[GC.getUnitInfo((UnitTypes)uc).getType()] = picojson::value((double)n);
				}
				emp["unitCounts"] = picojson::value(unitCounts);
				// unitsMaking -- in-PRODUCTION unit count (getUnitMaking); canTrain counts these toward the instance
				// cap (CvPlayer::canTrain:6481-6482, empire = player's own making; world = the team's, summed offline).
				picojson::value::object unitMaking;
				for (int um = 0; um < GC.getNumUnitInfos(); ++um)
				{
					const int m = kPlayer.getUnitMaking((UnitTypes)um);
					if (m > 0) unitMaking[GC.getUnitInfo((UnitTypes)um).getType()] = picojson::value((double)m);
				}
				emp["unitsMaking"] = picojson::value(unitMaking);
				// era index -- the EMPIRE unit `allowed` cap is ERA-SCALED for a base of 5 (CvPlayer::isUnitMaxedOut
				// :13584-13592: cap += era*5 when getMaxPlayerInstances()==5). The dry-calc needs the era to reproduce it.
				emp["era"] = picojson::value((double)kPlayer.getCurrentEra());

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

	// The per-plot building->improvement keyed contribution summed FRESH over the working city's ACTIVE buildings -- what
	// getYield (the plot cache) now uses, vs the stale getImprovementYieldChange city cache. Used so plotDecomp's total
	// and cityImprovement match the engine's real (fresh) basePlotYield = getPlotYield = Σ getYield. (owner ruling 2026-06-27)
	int freshCityImprovement(const CvPlot* pPlot, YieldTypes eYield)
	{
		const CvCity* pWC = pPlot->getWorkingCity();
		const ImprovementTypes eImp = pPlot->getImprovementType();
		if (pWC == NULL || eImp == NO_IMPROVEMENT || !(pPlot->isRoute() || !pPlot->isImpassable(pWC->getTeam()))) return 0;
		int iFresh = 0;
		foreach_(const BuildingTypes eB, pWC->getHasBuildings())
		{
			if (pWC->isDisabledBuilding(eB)) continue;
			foreach_(const ImprovementArray& pr, GC.getBuildingInfo(eB).getImprovementYieldChanges())
				if ((ImprovementTypes)pr.first == eImp) { iFresh += pr.second[eYield]; break; }
		}
		return iFresh;
	}

	// Decompose a city's getPlotYield (the basePlotYield base term) into the NAMED addends of CvPlot::calculateYield
	// (CvPlot.cpp:8320; legacy-value-calc-map §10.1), each summed over the city's WORKED plots. Emitted on
	// /computed/cities/yields so (a) the external calculator localises a basePlotYield divergence to ONE named source
	// instead of guessing, and (b) the in-engine shadow knows exactly which addend it replaces at cutover. The `total`
	// field equals getPlotYield(eYield) (the same plots, the same per-plot calc), so it reconciles the parts.
	picojson::value::object workedPlotYieldDecomposition(const CvCity* pCity, YieldTypes eYield)
	{
		const TeamTypes eTeam = pCity->getTeam();
		const CvPlayer& kOwner = GET_PLAYER(pCity->getOwner());
		const CvYieldInfo& kYield = GC.getYieldInfo(eYield);

		int natureYield = 0, extraYield = 0, cityCentreChange = 0, populationChange = 0;
		int playerTerrainChange = 0, seaPlotYield = 0;
		// workingCity (getYieldChangeAt, CvCity.cpp:10862) sub-decomposed into its four components, so a divergence
		// localises to plot-type vs city-terrain vs river vs city-improvement rather than the lump.
		int plotTypeChange = 0, cityTerrainChange = 0, riverPlotChange = 0, cityImprovementChange = 0;
		int improvementChange = 0, routeChange = 0, goldenAgeYield = 0, total = 0;
		// improvement (calculateImprovementYieldChange, CvPlot.cpp:8247-8301) sub-split into its named addends so a
		// divergence localises to base/riverSide/irrigated/route(improvement's per-route bump)/player(trait+civic+
		// building-global accumulator)/team(improvement TechYieldChanges) rather than the lump.
		int impBase = 0, impRiverSide = 0, impIrrigated = 0, impRoute = 0, impPlayer = 0, impTeam = 0, impBonus = 0;
		// ExtraYield/LessYield player threshold (calculateYield 8393-8401): +/- EXTRA_YIELD per plot whose RUNNING
		// pre-improvement yield clears the player's extra/less threshold (e.g. Industrious prod>=7 +1, Nomad prod>=5 -1).
		int thresholdYield = 0;
		const int iExtraYield = GC.getDefineINT("EXTRA_YIELD");
		const int iExtraThreshold = kOwner.getExtraYieldThreshold(eYield);
		const int iLessThreshold  = kOwner.getLessYieldThreshold(eYield);
		// PER-PLOT array (so a basePlotYield divergence localises to the exact plot+substrate, not the aggregate).
		picojson::value::array perPlotArr;

		for (int iPlot = 0; iPlot < pCity->getNumCityPlots(); ++iPlot)
		{
			if (!pCity->isWorkingPlot(iPlot)) continue;
			const CvPlot* pPlot = pCity->getCityIndexPlot(iPlot);
			if (pPlot == NULL || pPlot->getTerrainType() == NO_TERRAIN) continue;

			total       += pPlot->getYield(eYield);   // the plot cache (fresh, building->improvement summed live) = the engine's real base yield
			natureYield += pPlot->calculateNatureYield(eYield, eTeam);
			extraYield  += pPlot->getExtraYield(eYield);

			const bool bCityCentre = (pPlot == pCity->plot());
			if (bCityCentre)
			{
				cityCentreChange += kYield.getCityChange();
				if (kYield.getPopulationChangeDivisor() != 0)
					populationChange += pCity->getPopulation() / kYield.getPopulationChangeDivisor();
			}

			if (pPlot->isRoute() || !pPlot->isImpassable(eTeam))
			{
				playerTerrainChange += kOwner.getTerrainYieldChange(pPlot->getTerrainType(), eYield);
				if (pPlot->isWater()) seaPlotYield += kOwner.getSeaPlotYield(eYield);
				const CvCity* pWorkingCity = pPlot->getWorkingCity();
				if (pWorkingCity != NULL)
				{
					plotTypeChange    += pWorkingCity->getPlotYieldChange(pPlot->getPlotType(), eYield);
					cityTerrainChange += pWorkingCity->getTerrainYieldChange(pPlot->getTerrainType(), eYield);
					if (pPlot->isRiver()) riverPlotChange += pWorkingCity->getRiverPlotYield(eYield);
					const ImprovementTypes eImp = pPlot->getImprovementType();
					if (eImp != NO_IMPROVEMENT) cityImprovementChange += freshCityImprovement(pPlot, eYield);   // fresh active-buildings sum (what getYield uses), not the stale cache
				}
			}

			// Pre-improvement RUNNING yield (8347-8401): nature + extra + centre + reachable adds + landmark -- the base
			// that BOTH the extra/less threshold (8393) AND the golden-age threshold (8403) test, BEFORE improvement/
			// route (8430/8435) are added. Computed ALWAYS (the golden-age check needs it even with no extra/less).
			int iRun = pPlot->calculateNatureYield(eYield, eTeam) + pPlot->getExtraYield(eYield);
			if (bCityCentre)
			{
				iRun += kYield.getCityChange();
				if (kYield.getPopulationChangeDivisor() != 0)
					iRun += pCity->getPopulation() / kYield.getPopulationChangeDivisor();
			}
			if (pPlot->isRoute() || !pPlot->isImpassable(eTeam))
			{
				iRun += kOwner.getTerrainYieldChange(pPlot->getTerrainType(), eYield);
				if (pPlot->isWater()) iRun += kOwner.getSeaPlotYield(eYield);
				const CvCity* pWC = pPlot->getWorkingCity();
				if (pWC != NULL) iRun += pWC->getYieldChangeAt(pPlot, eYield);
			}
			if (pPlot->getLandmarkType() != NO_LANDMARK && GC.getGame().isOption(GAMEOPTION_MAP_PERSONALIZED))
				iRun += kOwner.getLandmarkYield(eYield);
			// extra/less-yield threshold (8393-8401) modifies the running yield BEFORE the golden-age check
			int iPlotThreshold = 0;
			if (iExtraThreshold > 0 && iRun >= iExtraThreshold) iPlotThreshold += iExtraYield;
			if (iLessThreshold  > 0 && iRun >= iLessThreshold)  iPlotThreshold -= iExtraYield;
			thresholdYield += iPlotThreshold;
			// GOLDEN AGE (8403): tests the POST-threshold, PRE-improvement/route running yield -- NOT calculateYield
			// (the final yield). The earlier dump tested the final yield, which over-fires on tiles whose improvement
			// pushes them past the threshold. iGoldenBase is the exact figure a cascade's per-plot golden-age must match.
			const int iGoldenBase = iRun + iPlotThreshold;
			int iPlotGolden = 0;
			if (kOwner.isGoldenAge() && iGoldenBase >= kYield.getGoldenAgeYieldThreshold())
				iPlotGolden = kYield.getGoldenAgeYield();
			goldenAgeYield += iPlotGolden;

			if (!bCityCentre)
			{
				const ImprovementTypes eImprovement = pPlot->getImprovementType();
				if (eImprovement != NO_IMPROVEMENT)
				{
					improvementChange += pPlot->calculateImprovementYieldChange(eImprovement, eYield, pCity->getOwner());
					// Mirror the engine's real-player branch (8247-8301; ePlayer set, not bOptimal) component by component.
					const CvImprovementInfo& kImp = GC.getImprovementInfo(eImprovement);
					impBase += kImp.getYieldChange(eYield);
					if (pPlot->getRiverCrossingCount() > 0) impRiverSide += kImp.getRiverSideYieldChange(eYield);
					if (pPlot->isIrrigationAvailable()) impIrrigated += kImp.getIrrigatedYieldChange(eYield);
					const RouteTypes eImpRoute = pPlot->getRouteType();
					if (eImpRoute != NO_ROUTE)          impRoute += kImp.getRouteYieldChanges(eImpRoute, eYield);
					impPlayer += kOwner.getImprovementYieldChange(eImprovement, eYield);
					impTeam   += GET_TEAM(eTeam).getImprovementYieldChange(eImprovement, eYield);
					const BonusTypes eImpBonus = pPlot->getBonusType(eTeam);   // improvement's per-bonus bump (8305-8310)
					if (eImpBonus != NO_BONUS) impBonus += kImp.getImprovementBonusYield(eImpBonus, eYield);
				}
				const RouteTypes eRoute = pPlot->getRouteType();
				if (eRoute != NO_ROUTE) routeChange += GC.getRouteInfo(eRoute).getYieldChange(eYield);
			}

			// per-plot row: substrate + the realized addends, so a basePlotYield -1 localises to THIS plot's terrain/
			// feature/bonus/improvement vs the cascade's per-plot basePlotRows.
			{
				picojson::value::object pp;
				pp["terrain"] = picojson::value(std::string(GC.getTerrainInfo(pPlot->getTerrainType()).getType()));
				if (pPlot->getFeatureType() != NO_FEATURE) pp["feature"] = picojson::value(std::string(GC.getFeatureInfo(pPlot->getFeatureType()).getType()));
				if (pPlot->getBonusType(eTeam) != NO_BONUS) pp["bonus"] = picojson::value(std::string(GC.getBonusInfo(pPlot->getBonusType(eTeam)).getType()));
				// REVEAL: the TRUE bonus (ignoring reveal) + whether the team has discovered it. A bonus on a plot gives
				// NO improvement bump until revealed (getBonusType(eTeam) -> NO_BONUS), so a divergence may be a reveal gap.
				const BonusTypes eTrueBonusP = pPlot->getBonusType();
				if (eTrueBonusP != NO_BONUS)
				{
					pp["trueBonus"] = picojson::value(std::string(GC.getBonusInfo(eTrueBonusP).getType()));
					pp["bonusRevealed"] = picojson::value(pPlot->getBonusType(eTeam) != NO_BONUS);
				}
				const ImprovementTypes eImpP = pPlot->getImprovementType();
				if (eImpP != NO_IMPROVEMENT) pp["improvement"] = picojson::value(std::string(GC.getImprovementInfo(eImpP).getType()));
				pp["total"]  = picojson::value((double)pPlot->getYield(eYield));   // the plot cache (fresh) -- the engine's real per-plot yield
				pp["nature"] = picojson::value((double)pPlot->calculateNatureYield(eYield, eTeam));
				pp["imp"]    = picojson::value((double)((!bCityCentre && eImpP != NO_IMPROVEMENT) ? pPlot->calculateImprovementYieldChange(eImpP, eYield, pCity->getOwner()) : 0));
				// nature + improvement SUB-sources (pin a -1 to terrain/feature/bonus food, or impBase vs impBonus).
				pp["terrainFood"] = picojson::value((double)GC.getTerrainInfo(pPlot->getTerrainType()).getYield(eYield));
				if (pPlot->getFeatureType() != NO_FEATURE) pp["featureFood"] = picojson::value((double)GC.getFeatureInfo(pPlot->getFeatureType()).getYieldChange(eYield));
				if (pPlot->getBonusType(eTeam) != NO_BONUS) pp["bonusFood"] = picojson::value((double)GC.getBonusInfo(pPlot->getBonusType(eTeam)).getYieldChange(eYield));
				if (!bCityCentre && eImpP != NO_IMPROVEMENT) {
					const CvImprovementInfo& kI = GC.getImprovementInfo(eImpP);
					pp["impBase"] = picojson::value((double)kI.getYieldChange(eYield));
					if (pPlot->getBonusType(eTeam) != NO_BONUS) pp["impBonus"] = picojson::value((double)kI.getImprovementBonusYield(pPlot->getBonusType(eTeam), eYield));
					// FULL per-plot improvement split so imp == impBase+impBonus+impRiverSide+impIrrigated+impRoute+impPlayer+impTeam
					// exactly (no unattributed remainder). impTeam = the team's improvement-yield accumulator (reveal-tech-fed
					// improvement TechYieldChanges); impPlayer = trait/civic/building-global accumulator.
					if (pPlot->getRiverCrossingCount() > 0) pp["impRiverSide"] = picojson::value((double)kI.getRiverSideYieldChange(eYield));
					if (pPlot->isIrrigationAvailable())  pp["impIrrigated"] = picojson::value((double)kI.getIrrigatedYieldChange(eYield));
					const RouteTypes eImpRouteP = pPlot->getRouteType();
					if (eImpRouteP != NO_ROUTE)          pp["impRoute"]     = picojson::value((double)kI.getRouteYieldChanges(eImpRouteP, eYield));
					pp["impPlayer"] = picojson::value((double)kOwner.getImprovementYieldChange(eImpP, eYield));
					pp["impTeam"]   = picojson::value((double)GET_TEAM(eTeam).getImprovementYieldChange(eImpP, eYield));
				}
				// per-plot getYieldChangeAt split (the working-city KEYED deposits: plotType + terrain + river +
				// improvement) so a cascade `keyed` -1 pins to which member (terrain vs improvement vs plot-type) is short.
				{
					const CvCity* pWC = pPlot->getWorkingCity();
					if (pWC != NULL)
					{
						pp["cityPlotType"] = picojson::value((double)pWC->getPlotYieldChange(pPlot->getPlotType(), eYield));
						pp["cityTerrain"]  = picojson::value((double)pWC->getTerrainYieldChange(pPlot->getTerrainType(), eYield));
						if (pPlot->isRiver()) pp["cityRiver"] = picojson::value((double)pWC->getRiverPlotYield(eYield));
						if (eImpP != NO_IMPROVEMENT) pp["cityImprovement"] = picojson::value((double)freshCityImprovement(pPlot, eYield));   // fresh active-buildings sum (what getYield uses)
					}
				}
				pp["water"]  = picojson::value(pPlot->isWater());
				pp["center"] = picojson::value(bCityCentre);
				// the golden-age threshold base (pre-improvement/route, post-threshold) + the bonus it fired, so a
				// cascade's per-plot golden-age decision is attributable tile-by-tile (8403).
				pp["preImpBase"] = picojson::value((double)iGoldenBase);
				pp["goldenAge"]  = picojson::value((double)iPlotGolden);
				perPlotArr.push_back(picojson::value(pp));
			}
		}

		picojson::value::object decomposition;
		decomposition["total"]            = picojson::value((double)total);
		decomposition["nature"]           = picojson::value((double)natureYield);
		decomposition["extra"]            = picojson::value((double)extraYield);
		decomposition["cityChange"]       = picojson::value((double)cityCentreChange);
		decomposition["popChange"]        = picojson::value((double)populationChange);
		decomposition["playerTerrain"]    = picojson::value((double)playerTerrainChange);
		decomposition["seaPlot"]          = picojson::value((double)seaPlotYield);
		decomposition["plotTypeChange"]   = picojson::value((double)plotTypeChange);
		decomposition["cityTerrain"]      = picojson::value((double)cityTerrainChange);
		decomposition["riverPlot"]        = picojson::value((double)riverPlotChange);
		decomposition["cityImprovement"]  = picojson::value((double)cityImprovementChange);
		decomposition["improvement"]      = picojson::value((double)improvementChange);
		decomposition["impBase"]          = picojson::value((double)impBase);
		decomposition["impRiverSide"]     = picojson::value((double)impRiverSide);
		decomposition["impIrrigated"]     = picojson::value((double)impIrrigated);
		decomposition["impRoute"]         = picojson::value((double)impRoute);
		decomposition["impPlayer"]        = picojson::value((double)impPlayer);
		decomposition["impTeam"]          = picojson::value((double)impTeam);
		decomposition["impBonus"]         = picojson::value((double)impBonus);
		decomposition["route"]            = picojson::value((double)routeChange);
		decomposition["goldenAge"]        = picojson::value((double)goldenAgeYield);
		decomposition["threshold"]        = picojson::value((double)thresholdYield);
		decomposition["playerExtraThreshold"] = picojson::value((double)iExtraThreshold);   // engine kOwner.getExtraYieldThreshold
		decomposition["playerLessThreshold"]  = picojson::value((double)iLessThreshold);    // engine kOwner.getLessYieldThreshold
		// Self-check: total minus every named addend. A non-zero residual = an un-decomposed calculateYield
		// component (landmark / city min-floor / per-plot max(0) clamp on a net-negative plot) still to name.
		const int named = natureYield + extraYield + cityCentreChange + populationChange + playerTerrainChange
			+ seaPlotYield + plotTypeChange + cityTerrainChange + riverPlotChange + cityImprovementChange
			+ improvementChange + routeChange + goldenAgeYield + thresholdYield;
		decomposition["residual"]         = picojson::value((double)(total - named));
		decomposition["plots"]            = picojson::value(perPlotArr);   // per-plot rows (food/yield divergence localiser)
		return decomposition;
	}

	// ---- /state slice renderers: RAW inputs only (no drycalc target ever). iPlayerFilter < 0 == all alive
	// players; else just that one. City ids are not unique across empires, so each row carries owner+id (cities
	// reuse extractCity, which stamps owner/id/name/x/y). See docs/specs/http-endpoints.md. ----
	// /state/plots -- the GLOBAL map (the World StoneBase keys on): every plot by its map INDEX, the same all-plots
	// walk PlotSnapshot.cpp uses (idx == CvMap::plotNum(x,y)). Each plot carries its raw contents + the predicate
	// facts the cascade reads + the city<->plot link (workingCity {owner,id} + worked). RAW inputs only -- no yields,
	// no verdicts (http-endpoints "no drycalc TARGET"); the per-city /state plot lists carry only these idx ids.
	CvString extractPlots()
	{
		const CvMap& kMap = GC.getMap();
		picojson::value::object root;
		root["turn"] = picojson::value((double)GC.getGame().getGameTurn());
		root["mapW"] = picojson::value((double)kMap.getGridWidth());
		root["mapH"] = picojson::value((double)kMap.getGridHeight());
		picojson::value::array plots;
		const int iNumPlots = kMap.numPlots();

		// Inverse of getCityIndexPlot: which cities' WORKABLE RADIUS (geometric fat cross) includes each plot. The
		// engine only exposes plot->getWorkingCity() (the single ASSIGNED city); the full radius is a many-to-many
		// (overlapping crosses), so we build the plot->cities map once by walking every city's getCityIndexPlot. This
		// is what lets StoneBase reconstruct each city's full EvalState.Plots (the VICINITY substrate) without
		// re-deriving Civ4 city geometry -- the plot map carries the whole plot<->city relationship.
		std::map<int, picojson::value::array> radiusByIdx;
		for (int iP = 0; iP < MAX_PLAYERS; ++iP)
		{
			CvPlayer& kP = GET_PLAYER((PlayerTypes)iP);
			if (!kP.isAlive()) continue;
			int iLoop;
			for (CvCity* pCity = kP.firstCity(&iLoop); pCity != NULL; pCity = kP.nextCity(&iLoop))
				for (int pi = 0; pi < pCity->getNumCityPlots(); ++pi)
				{
					const CvPlot* pRP = pCity->getCityIndexPlot(pi);
					if (pRP == NULL) continue;
					picojson::value::object cref;
					cref["owner"] = picojson::value((double)pCity->getOwner());
					cref["id"]    = picojson::value((double)pCity->getID());
					radiusByIdx[kMap.plotNum(pRP->getX(), pRP->getY())].push_back(picojson::value(cref));
				}
		}

		for (int i = 0; i < iNumPlots; ++i)
		{
			const CvPlot* pPlot = kMap.plotByIndex(i);
			if (pPlot == NULL) continue;
			picojson::value::object pl;
			pl["idx"] = picojson::value((double)i);                       // global map index == CvMap::plotNum(x,y) -- the World plot id
			pl["x"]   = picojson::value((double)pPlot->getX());
			pl["y"]   = picojson::value((double)pPlot->getY());
			const TerrainTypes eT = pPlot->getTerrainType();
			if (eT != NO_TERRAIN) pl["terrain"] = picojson::value(std::string(GC.getTerrainInfo(eT).getType()));
			const FeatureTypes eF = pPlot->getFeatureType();
			if (eF != NO_FEATURE) pl["feature"] = picojson::value(std::string(GC.getFeatureInfo(eF).getType()));
			const ImprovementTypes eI = pPlot->getImprovementType();
			if (eI != NO_IMPROVEMENT) pl["improvement"] = picojson::value(std::string(GC.getImprovementInfo(eI).getType()));
			const RouteTypes eR = pPlot->getRouteType();
			if (eR != NO_ROUTE) pl["route"] = picojson::value(std::string(GC.getRouteInfo(eR).getType()));
			CvCity* pWork = pPlot->getWorkingCity();
			// Bonus as REVEALED to the working city's team (getBonusType(team)) -- matches /state/cities + the engine's
			// yield calc: an UNREVEALED late-era resource (methane ice / uranium / ...) contributes nothing until its tech.
			const BonusTypes eB = pPlot->getBonusType(pWork != NULL ? pWork->getTeam() : NO_TEAM);
			if (eB != NO_BONUS)
			{
				pl["bonus"] = picojson::value(std::string(GC.getBonusInfo(eB).getType()));
				// vicinity-bonus (OBTAINED semantic): owned by the working city + valid + connected to it (CvCity::hasVicinityBonus)
				if (pWork != NULL && pPlot->getOwner() == pWork->getOwner()
					&& pPlot->isHasValidBonus() && pPlot->isConnectedTo(pWork))
					pl["bonusConnected"] = picojson::value(true);
			}
			if (pPlot->isBeingWorked())         pl["worked"] = picojson::value(true);
			if (pPlot->isRiver())               pl["river"] = picojson::value(true);
			if (pPlot->isIrrigationAvailable()) pl["irrig"] = picojson::value(true);
			if (pPlot->isHills())               pl["hills"] = picojson::value(true);
			if (pPlot->isPeak() || pPlot->isAsPeak()) pl["peak"] = picojson::value(true);
			// relief = REAL plotType (getPlotType): getBaseYield keys getPeakChange/getHillsChange on THIS, not on
			// isPeak()||isAsPeak() -- a feature-induced as-peak (Kilimanjaro on flat land) is PLOT_LAND, no relief base yield.
			{ const PlotTypes eRel = pPlot->getPlotType();
			  if (eRel == PLOT_PEAK)       pl["relief"] = picojson::value(std::string("PEAK"));
			  else if (eRel == PLOT_HILLS) pl["relief"] = picojson::value(std::string("HILLS")); }
			if (pPlot->isWater())               pl["water"] = picojson::value(true);
			if (pPlot->isCoastalLand())         pl["coast"] = picojson::value(true);
			if (pPlot->isFreshWater())          pl["freshwater"] = picojson::value(true);
			if (pPlot->isCity())                pl["isCity"] = picojson::value(true);
			if (pPlot->getTeam() != NO_TEAM)    pl["ownerTeam"] = picojson::value((double)pPlot->getTeam());
			if (pWork != NULL)
			{
				picojson::value::object wc;                              // canonical (owner,id) city handle (http-endpoints City identity)
				wc["owner"] = picojson::value((double)pWork->getOwner());
				wc["id"]    = picojson::value((double)pWork->getID());
				pl["workingCity"] = picojson::value(wc);
			}
			// radiusCities -- EVERY city whose workable radius (getCityIndexPlot) includes this plot (a superset of
			// workingCity; overlapping fat crosses make it many-to-many). The per-city EvalState.Plots is "all plots
			// whose radiusCities contains me", so the per-city plot arrays become fully derivable from this map.
			{
				std::map<int, picojson::value::array>::iterator it = radiusByIdx.find(i);
				if (it != radiusByIdx.end()) pl["radiusCities"] = picojson::value(it->second);
			}
			const CvArea* pArea = pPlot->area();
			if (pArea != NULL) pl["area"] = picojson::value((double)pArea->getID());
			// per-plot stored EXTRA yield (event/effect savegame state; a calculateYield addend, an INPUT not a TARGET)
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
		root["plots"] = picojson::value(plots);
		return CvString(picojson::value(root).serialize().c_str());
	}

	CvString stateSlice(const char* szAction, int iPlayerFilter, int iCityReq)
	{
		picojson::value::object root;
		root["turn"] = picojson::value((double)GC.getGame().getGameTurn());
		picojson::value::array arr;

		for (int p = 0; p < MAX_PLAYERS; ++p)
		{
			CvPlayer& kPlayer = GET_PLAYER((PlayerTypes)p);
			if (!kPlayer.isAlive()) continue;
			if (iPlayerFilter >= 0 && p != iPlayerFilter) continue;
			const int iTeam = (int)kPlayer.getTeam();

			if (strcmp(szAction, "stateTechs") == 0)
			{
				picojson::value::object e;
				e["player"] = picojson::value((double)p);
				e["team"]   = picojson::value((double)iTeam);
				picojson::value::array techs;
				for (int t = 0; t < GC.getNumTechInfos(); ++t)
					if (GET_TEAM((TeamTypes)iTeam).isHasTech((TechTypes)t))
						techs.push_back(picojson::value(std::string(GC.getTechInfo((TechTypes)t).getType())));
				e["techs"] = picojson::value(techs);
				arr.push_back(picojson::value(e));
			}
			else if (strcmp(szAction, "statePlayers") == 0)
			{
				picojson::value::object e;
				e["id"]        = picojson::value((double)p);
				e["team"]      = picojson::value((double)iTeam);
				e["era"]       = picojson::value(std::string(GC.getEraInfo(kPlayer.getCurrentEra()).getType()));
				e["isHuman"]   = picojson::value(kPlayer.isHuman());
				e["isNPC"]     = picojson::value(kPlayer.isNPC());
				e["isAnarchy"] = picojson::value(kPlayer.isAnarchy());
				e["isRebel"]   = picojson::value(kPlayer.isRebel());
				e["cityCount"] = picojson::value((double)kPlayer.getNumCities());
				picojson::value::array civics;
				for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
				{
					const CivicTypes eC = kPlayer.getCivics((CivicOptionTypes)co);
					if (eC != NO_CIVIC) civics.push_back(picojson::value(std::string(GC.getCivicInfo(eC).getType())));
				}
				e["civics"] = picojson::value(civics);
				picojson::value::array traits;
				for (int tr = 0; tr < GC.getNumTraitInfos(); ++tr)
					if (kPlayer.hasTrait((TraitTypes)tr))
						traits.push_back(picojson::value(std::string(GC.getTraitInfo((TraitTypes)tr).getType())));
				e["traits"] = picojson::value(traits);
				const ReligionTypes eState = kPlayer.getStateReligion();
				if (eState != NO_RELIGION) e["stateReligion"] = picojson::value(std::string(GC.getReligionInfo(eState).getType()));
				arr.push_back(picojson::value(e));
			}
			else if (strcmp(szAction, "stateUnits") == 0)
			{
				foreach_(const CvUnit* pUnit, kPlayer.units())
				{
					picojson::value::object e;
					const UnitTypes eUT = pUnit->getUnitType();
					const UnitAITypes eAI = pUnit->AI_getUnitAIType();
					const CvSelectionGroup* pGrp = pUnit->getGroup();
					e["owner"]      = picojson::value((double)p);
					e["id"]         = picojson::value((double)pUnit->getID());
					e["type"]       = picojson::value(std::string(eUT != NO_UNIT ? GC.getUnitInfo(eUT).getType() : "NO_UNIT"));
					e["ai"]         = picojson::value(std::string(eAI != NO_UNITAI ? GC.getUnitAIInfo(eAI).getType() : "NO_UNITAI"));
					e["x"]          = picojson::value((double)pUnit->getX());
					e["y"]          = picojson::value((double)pUnit->getY());
					e["group"]      = picojson::value((double)(pGrp != NULL ? pGrp->getID() : -1));
					e["damage"]     = picojson::value((double)pUnit->getDamage());
					e["level"]      = picojson::value((double)pUnit->getLevel());
					e["experience"] = picojson::value((double)pUnit->getExperience());
					picojson::value::array promos;
					for (int pr = 0; pr < GC.getNumPromotionInfos(); ++pr)
						if (pUnit->isHasPromotion((PromotionTypes)pr))
							promos.push_back(picojson::value(std::string(GC.getPromotionInfo((PromotionTypes)pr).getType())));
					e["promotions"] = picojson::value(promos);
					arr.push_back(picojson::value(e));
				}
			}
			else if (strcmp(szAction, "stateCities") == 0)
			{
				int iLoop;
				for (CvCity* pCity = kPlayer.firstCity(&iLoop); pCity != NULL; pCity = kPlayer.nextCity(&iLoop))
				{
					if (iCityReq >= 0 && pCity->getID() != iCityReq) continue;
					arr.push_back(picojson::value(extractCity(kPlayer, pCity, iTeam)));
				}
			}
		}
		root["data"] = picojson::value(arr);
		return CvString(picojson::value(root).serialize().c_str());
	}

	CvString evaluateGate(const char* szAction, const char* szType, int iPlayer, int iCityReq)
	{
		// /state/all -- the entire raw game-state in one document; iPlayer < 0 == ALL players (runs before the
		// single-player resolution below, so the "all players" walk is reachable).
		if (strcmp(szAction, "gamestate") == 0)
			return extractGameState(iPlayer);

		// /state/plots -- the GLOBAL map dump (not per-player); resolved before the per-player stateSlice below.
		if (strcmp(szAction, "statePlots") == 0)
			return extractPlots();

		// /state/* granular slices (raw inputs); also all-players-capable, so resolved before the single-player block.
		if (strncmp(szAction, "state", 5) == 0)
			return stateSlice(szAction, iPlayer, iCityReq);

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

		// CITY-BUILDABLE -- the engine's canConstruct TRUE-set for the city: the buildable ORACLE the external
		// cascade is checked against (bulk; the per-type /computed/canConstruct is the single-entity gate). No
		// type; city-relative. NB BUILDING_LEECH_CATCHER appears here -- a known engine lie; the cascade is
		// CORRECT to omit it (owner ruling), so a Leech-Catcher-only divergence is expected, not a fail.
		if (strcmp(szAction, "cityBuildable") == 0)
		{
			o["city"] = picojson::value((double)iCityId);
			if (pCity == NULL)
			{
				o["error"] = picojson::value(std::string("no city"));
				return CvString(picojson::value(o).serialize().c_str());
			}
			o["globalId"] = picojson::value(std::string(CvString::format("%02d-%d", pCity->getOwner(), iCityId).GetCString()));
			picojson::value::array buildable;
			for (int b = 0; b < GC.getNumBuildingInfos(); ++b)
				if (pCity->canConstruct((BuildingTypes)b))
					buildable.push_back(picojson::value(std::string(GC.getBuildingInfo((BuildingTypes)b).getType())));
			o["buildable"] = picojson::value(buildable);
			return CvString(picojson::value(o).serialize().c_str());
		}

		// CITY-TRAINABLE -- the engine's canTrain TRUE-set for the city: the trainability ORACLE the external cascade
		// is checked against (bulk; the per-type /computed/canTrain is the single-entity gate). No type; city-relative.
		// Default canTrain args (bContinue=false, bTestVisible=false, bIgnoreCost=false, bIgnoreUpgrades=false), so the
		// upgrade-superseded units ARE excluded -- the cascade reproduces that via requires.build.dormant.
		if (strcmp(szAction, "cityTrainable") == 0)
		{
			o["city"] = picojson::value((double)iCityId);
			if (pCity == NULL)
			{
				o["error"] = picojson::value(std::string("no city"));
				return CvString(picojson::value(o).serialize().c_str());
			}
			o["globalId"] = picojson::value(std::string(CvString::format("%02d-%d", pCity->getOwner(), iCityId).GetCString()));
			picojson::value::array trainable;
			for (int u = 0; u < GC.getNumUnitInfos(); ++u)
				if (pCity->canTrain((UnitTypes)u))
					trainable.push_back(picojson::value(std::string(GC.getUnitInfo((UnitTypes)u).getType())));
			o["trainable"] = picojson::value(trainable);
			return CvString(picojson::value(o).serialize().c_str());
		}

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
			o["globalId"] = picojson::value(std::string(CvString::format("%02d-%d", pCity->getOwner(), iCityId).GetCString())); // the "<PP>-<id>" snowflake (matches /state/cities)
			o["cityName"] = picojson::value(std::string(narrowToAscii(pCity->getName()).GetCString()));
			o["population"] = picojson::value((double)pCity->getPopulation());
			o["cap"] = picojson::value((double)CITY_MAX_YIELD_RATE); // the getYieldRate100 clamp ceiling
			// FREE-SPECIALIST AMOUNT oracle (the static free-specialist slot count the cascade must COMPUTE from grants,
			// then verify here — NOT a /state ride-in). totalFreeSpecialists = city(buildings' getFreeSpecialist) + area +
			// player + Σ improvement freeSpecialists×plots + per-wonder; cityFreeSpecialist = the city building-sourced
			// generic part only (getFreeSpecialist). These are the GENERIC "any" free specialists, distinct from the
			// per-type assigned counts in /state.specialists (getSpecialistCount + getFreeSpecialistCount).
			o["totalFreeSpecialists"] = picojson::value((double)pCity->totalFreeSpecialists());
			o["cityFreeSpecialist"]   = picojson::value((double)pCity->getFreeSpecialist());
			// PER-TERM decomposition of totalFreeSpecialists (CvCity::totalFreeSpecialists, CvCity.cpp:5747) — emitted so a
			// divergence ATTRIBUTES to a NAMED term (area/player/improvement/wonder), never a guessed aggregate (THE NO-
			// GUESSING RULE / total-observability). total = max(0, city + area + player + improvement + wonder) [0 if pop<1].
			{
				const CvPlayer& kFsPlayer = GET_PLAYER(pCity->getOwner());
				o["areaFreeSpecialist"]   = picojson::value((double)pCity->area()->getFreeSpecialist(pCity->getOwner()));
				o["playerFreeSpecialist"] = picojson::value((double)kFsPlayer.getFreeSpecialist());
				int iImpFree = 0;
				for (int iFsI = 0; iFsI < GC.getNumImprovementInfos(); ++iFsI)
				{
					const int iFsRate = pCity->getImprovementFreeSpecialists((ImprovementTypes)iFsI);
					if (iFsRate != 0) iImpFree += iFsRate * pCity->countNumImprovedPlots((ImprovementTypes)iFsI);
				}
				o["improvementFreeSpecialists"] = picojson::value((double)iImpFree);
				int iWonderFree = 0;
				if (kFsPlayer.hasFreeSpecialistperWorldWonder())    iWonderFree += pCity->getNumWorldWonders();
				if (kFsPlayer.hasFreeSpecialistperNationalWonder()) iWonderFree += pCity->getNumNationalWonders();
				if (kFsPlayer.hasFreeSpecialistperTeamProject())    iWonderFree += pCity->getNumTeamWonders();
				o["wonderFreeSpecialist"] = picojson::value((double)iWonderFree);
				// raw per-city wonder counts (so the cascade can verify its allowed-scope wonder-count derivation directly).
				o["numWorldWonders"]    = picojson::value((double)pCity->getNumWorldWonders());
				o["numNationalWonders"] = picojson::value((double)pCity->getNumNationalWonders());
				o["numTeamWonders"]     = picojson::value((double)pCity->getNumTeamWonders());
			}

			// the ACTIVE-building loadout (the cascade deposit source). Owner 2026-06-19: emit the ACTIVE set
			// (hasFullyActiveBuilding = present AND not resource/replacement-disabled AND not religiously-limited) so the
			// StoneBase reads the live active set directly rather than re-deriving dormancy -- legacy's yield modifier
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
					// relief = the REAL plotType (getPlotType). getBaseYield keys getPeakChange/getHillsChange on THIS,
					// NOT on isPeak()||isAsPeak() -- a feature-induced as-peak (e.g. Kilimanjaro on flat land) is PLOT_LAND
					// and gets NO relief base yield, while the "peak" predicate flag above still counts it for HAS_PEAK.
					{ const PlotTypes eRel=pPlot->getPlotType();
					  if(eRel==PLOT_PEAK)       pl["relief"]=picojson::value(std::string("PEAK"));
					  else if(eRel==PLOT_HILLS) pl["relief"]=picojson::value(std::string("HILLS")); }
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

			// per-specialist YIELD breakdown -- the EXACT 5 terms m_aiSpecialistYieldTotal sums (CvCity.cpp:5156 +
			// getExtraSpecialistYield 11745-57): intrinsic (SpecialistInfo.getYieldChange), perType (player
			// getExtraSpecialistYield(spec,y)), local (city getLocalSpecialistExtraYield), all (player
			// getSpecialistExtraYield(y)), pct (player getSpecialistYieldPercentChanges, applied /100). Mirrors
			// specialistCommerceDetail so a specialist-yield gap attributes to a NAMED engine term, no guessing.
			{
				picojson::value::array sydet;
				CvPlayer& kPy = GET_PLAYER((PlayerTypes)iPlayer);
				static const char* aYN[3] = { "food", "production", "commerce" };
				for (int s=0; s<GC.getNumSpecialistInfos(); ++s)
				{
					const int cnt=pCity->getSpecialistCount((SpecialistTypes)s)+pCity->getFreeSpecialistCount((SpecialistTypes)s);
					if(cnt<=0) continue;
					const CvSpecialistInfo& si=GC.getSpecialistInfo((SpecialistTypes)s);
					picojson::value::object e;
					e["type"]=picojson::value(std::string(si.getType()));
					e["count"]=picojson::value((double)cnt);
					for(int y=0;y<3;++y)
					{
						const YieldTypes eY=(YieldTypes)y;
						const int intr=si.getYieldChange(eY);
						const int pt=kPy.getExtraSpecialistYield((SpecialistTypes)s,eY);
						const int lo=pCity->getLocalSpecialistExtraYield((SpecialistTypes)s,eY);
						const int al=kPy.getSpecialistExtraYield(eY);
						const int pc=kPy.getSpecialistYieldPercentChanges((SpecialistTypes)s,eY);
						if(intr||pt||lo||al||pc)
						{
							picojson::value::object yy;
							if(intr)yy["intrinsic"]=picojson::value((double)intr);
							if(pt)yy["perType"]=picojson::value((double)pt);
							if(lo)yy["local"]=picojson::value((double)lo);
							if(al)yy["all"]=picojson::value((double)al);
							if(pc)yy["pct"]=picojson::value((double)pc);
							e[aYN[y]]=picojson::value(yy);
						}
					}
					sydet.push_back(picojson::value(e));
				}
				o["specialistYieldDetail"]=picojson::value(sydet);
			}

			const int aFam[3] = { YIELD_FOOD, YIELD_PRODUCTION, YIELD_COMMERCE };
			const char* aFamName[3] = { "food", "production", "commerce" };
			picojson::value::array kYields;
			for (int f = 0; f < 3; ++f)
			{
				const YieldTypes eY = (YieldTypes)aFam[f];
				const int iBase = pCity->getBaseYieldRate(eY);
				picojson::value::object e;
				e["family"]        = picojson::value(std::string(aFamName[f]));
				// LEGACY input vector (the emulator reproduces getYieldRate100 from exactly these):
				e["base"]          = picojson::value((double)iBase);
				e["specialist"]    = picojson::value((double)pCity->getSpecialistYieldTotal(eY));
				e["modifier"]      = picojson::value((double)pCity->getBaseYieldRateModifier(eY)); // full % == 100 + sum%
				// MODIFIER BREAKDOWN (getBaseYieldRateModifier components, CvCity.cpp:11217) -- so the emulator
				// attributes the percent gap to the missing source (bonus/power/area/capital/player-trait), since
				// StoneBase only sums building + civic %.
				e["modBonus"]    = picojson::value((double)pCity->getBonusYieldRateModifier(eY));
				e["modBuilding"] = picojson::value((double)pCity->getBuildingYieldModifier(eY));
				e["modPlayer"]   = picojson::value((double)kPlayer.getYieldRateModifier(eY));
				e["modEvent"]    = picojson::value((double)pCity->getYieldRateModifier(eY));
				e["modPower"]    = picojson::value((double)(pCity->isPower() ? pCity->getPowerYieldRateModifier(eY) : 0));
				e["modArea"]     = picojson::value((double)(pCity->area() != NULL ? pCity->area()->getYieldRateModifier(pCity->getOwner(), eY) : 0));
				e["modCapital"]  = picojson::value((double)(pCity->isCapital() ? kPlayer.getCapitalYieldRateModifier(eY) : 0));
				// modPlayer/modCapital PER-SOURCE (CvPlayer 7457/18063/28600) -- so a modifier divergence attributes to a
				// NAMED civic/trait/building, no guessing. modPlayer = Σ civic.getYieldModifier + Σ trait.getYieldModifier
				// + Σ building.getGlobalYieldModifier × countNumBuildings (PER INSTANCE -- the multi-instance accumulation
				// the cascade's distinct-set undercounts); modCapital = Σ civic/trait getCapitalYieldModifier (capital only).
				{
					picojson::value::array civA, trA, bgA;
					for (int co = 0; co < GC.getNumCivicOptionInfos(); ++co)
					{
						const CivicTypes eC = kPlayer.getCivics((CivicOptionTypes)co);
						if (eC == NO_CIVIC) continue;
						const CvCivicInfo& kC = GC.getCivicInfo(eC);
						const int m = kC.getYieldModifier(eY), cm = kC.getCapitalYieldModifier(eY);
						if (m || cm) { picojson::value::object o; o["type"]=picojson::value(std::string(kC.getType())); if(m)o["mod"]=picojson::value((double)m); if(cm)o["capMod"]=picojson::value((double)cm); civA.push_back(picojson::value(o)); }
					}
					for (int tr = 0; tr < GC.getNumTraitInfos(); ++tr)
					{
						if (!kPlayer.hasTrait((TraitTypes)tr)) continue;
						const CvTraitInfo& kTr = GC.getTraitInfo((TraitTypes)tr);
						const int m = kTr.getYieldModifier(eY), cm = kTr.getCapitalYieldModifier(eY);
						if (m || cm) { picojson::value::object o; o["type"]=picojson::value(std::string(kTr.getType())); if(m)o["mod"]=picojson::value((double)m); if(cm)o["capMod"]=picojson::value((double)cm); trA.push_back(picojson::value(o)); }
					}
					for (int bb = 0; bb < GC.getNumBuildingInfos(); ++bb)
					{
						const CvBuildingInfo& kB = GC.getBuildingInfo((BuildingTypes)bb);
						const int g = kB.getGlobalYieldModifier(eY);
						if (!g) continue;
						const int cnt = kPlayer.countNumBuildings((BuildingTypes)bb);
						if (!cnt) continue;
						picojson::value::object o; o["type"]=picojson::value(std::string(kB.getType())); o["globalMod"]=picojson::value((double)g); o["count"]=picojson::value((double)cnt); o["total"]=picojson::value((double)(g*cnt));
						bgA.push_back(picojson::value(o));
					}
					picojson::value::object ms; ms["civics"]=picojson::value(civA); ms["traits"]=picojson::value(trA); ms["buildingGlobals"]=picojson::value(bgA);
					e["modPlayerSources"] = picojson::value(ms);
				}
				// per-TRAIT specialist yield (which held trait gives a specialist its perType -- getSpecialistYieldChange,
				// CvPlayer 28582) so the player-4 ARTIST gap attributes to a named trait, no store-vs-engine guessing.
				{
					picojson::value::array stA;
					for (int tr = 0; tr < GC.getNumTraitInfos(); ++tr)
					{
						if (!kPlayer.hasTrait((TraitTypes)tr)) continue;
						const CvTraitInfo& kTr = GC.getTraitInfo((TraitTypes)tr);
						picojson::value::object specs;
						for (int s = 0; s < GC.getNumSpecialistInfos(); ++s)
						{
							const int cnt = pCity->getSpecialistCount((SpecialistTypes)s)+pCity->getFreeSpecialistCount((SpecialistTypes)s);
							if (cnt <= 0) continue;
							const int v = kTr.getSpecialistYieldChange(s, eY);
							if (v) specs[GC.getSpecialistInfo((SpecialistTypes)s).getType()] = picojson::value((double)v);
						}
						if (!specs.empty()) { picojson::value::object o; o["type"]=picojson::value(std::string(kTr.getType())); o["specialists"]=picojson::value(specs); stA.push_back(picojson::value(o)); }
					}
					e["specialistTraitDetail"] = picojson::value(stA);
				}
				e["extraYield"]    = picojson::value((double)pCity->getExtraYield(eY));    // x1 TRUNCATED -- the term the formula uses
				e["extraYield100"] = picojson::value((double)pCity->getExtraYield100(eY)); // untruncated, for decompose
				// BASE decomposition (getBaseYieldRate, CvCity.cpp:22906) -- the additive base's named sub-sources:
				e["basePlotYield"]      = picojson::value((double)pCity->getPlotYield(eY));        // getPlotYield now PULLs Σ worked-plot getYield (the fresh plot cache) -- the real engine base yield
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
				// getPlotYield (basePlotYield) decomposed into its named CvPlot::calculateYield addends over worked
				// plots, so the calculator localises a divergence to ONE source and the shadow knows what to replace.
				e["plotDecomp"] = picojson::value(workedPlotYieldDecomposition(pCity, eY));
				kYields.push_back(picojson::value(e));
			}
			o["yields"] = picojson::value(kYields);

			// Per-trait PRODUCTION yield-threshold payload over the owner's HELD traits — maps a player
			// extra/less-threshold accumulator (updateExtra/LessYieldThreshold = MIN positive over hasTrait)
			// to its SOURCE trait, so a curated trait value can be diffed against the engine's trait payload.
			{
				const CvPlayer& kThP = GET_PLAYER(pCity->getOwner());
				picojson::value::array traitThresh;
				for (int t = 0; t < GC.getNumTraitInfos(); ++t)
				{
					if (!kThP.hasTrait((TraitTypes)t)) continue;
					const CvTraitInfo& kTr = GC.getTraitInfo((TraitTypes)t);
					const int iLes = kTr.getLessYieldThreshold(YIELD_PRODUCTION);
					const int iExt = kTr.getExtraYieldThreshold(YIELD_PRODUCTION);
					if (iLes == 0 && iExt == 0) continue;       // only traits carrying a production threshold
					picojson::value::object te;
					te["trait"]     = picojson::value(std::string(kTr.getType()));
					te["lessProd"]  = picojson::value((double)iLes);
					te["extraProd"] = picojson::value((double)iExt);
					te["negative"]  = picojson::value(kTr.isNegativeTrait());
					traitThresh.push_back(picojson::value(te));
				}
				o["ownerTraitThresholds"] = picojson::value(traitThresh);
			}

			// PER-BUILDING legacy yield decomposition (calc-emulator-spec §5): each ACTIVE building's flat
			// contribution (getBaseYieldRateFromBuilding100, x100 = YieldChange*100 + perPop*pop + techChange +
			// dynamic) and its static percent (getYieldModifier) per yield, so StoneBase ATTRIBUTES the
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
						if (f == 1) {   // PRODUCTION flat COMPONENT split (debug: static/dynamic/perPop/tech/bonusVic of a building-flat divergence)
							e["prodStatic100"]  = picojson::value((double)(bi.getYieldChange(eY) * 100));
							e["prodDynamic100"] = picojson::value((double)(pCity->getBuildingYieldChange((BuildingTypes)b, eY) * 100));
							e["prodPerPop100"]  = picojson::value((double)(bi.getYieldPerPopChange(eY) * iPopBY));
							e["prodTech100"]    = picojson::value((double)GET_TEAM(eTeamBY).getBuildingYieldTechChange(eY, (BuildingTypes)b));
							// the building's BONUS/VICINITY holder contribution (the squirrelBanana m_aiBuildingBonusVicinityYield100,
							// getBuildingExtraYield100) -- the conditional flat the cascade curates as {bonus|vicinity} production.city.flat.
							// REALIZED per-building flat = prodStatic+prodDynamic+prodPerPop+prodTech+prodBonusVic; without this the
							// per-building view omits exactly the term a vicinity over-count rides on (the player-5 +100).
							int iBV = 0;
							const bool bHasBon = bi.getBonusYieldChanges(NO_BONUS, NO_YIELD) != 0;
							const bool bHasVic = bi.getVicinityBonusYieldChanges(NO_BONUS, NO_YIELD) != 0;
							if (bHasBon || bHasVic)
								for (int iB = 0; iB < GC.getNumBonusInfos(); ++iB)
								{
									if (bHasBon && pCity->hasBonus((BonusTypes)iB))         iBV += bi.getBonusYieldChanges((BonusTypes)iB, eY);
									if (bHasVic && pCity->hasVicinityBonus((BonusTypes)iB)) iBV += bi.getVicinityBonusYieldChanges((BonusTypes)iB, eY);
								}
							e["prodBonusVic100"] = picojson::value((double)(iBV * 100));
						}
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
			// Emitted so StoneBase can reconstruct shrine commerce (the count is engine state, not in JSON).
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

		// /computed availability oracles (no type): the engine's per-player canResearch / canDoCivics / build-unlock
		// sets -- drycalc TARGETS, so they live here (pulled out of the raw /state dump).
		if (strcmp(szAction, "availableTechs") == 0)
		{
			picojson::value::array a;
			for (int iT = 0; iT < GC.getNumTechInfos(); ++iT)
				if (kPlayer.canResearch((TechTypes)iT))
					a.push_back(picojson::value(std::string(GC.getTechInfo((TechTypes)iT).getType())));
			o["availableTechs"] = picojson::value(a);
			return CvString(picojson::value(o).serialize().c_str());
		}
		if (strcmp(szAction, "availableCivics") == 0)
		{
			picojson::value::array a;
			for (int iC = 0; iC < GC.getNumCivicInfos(); ++iC)
				if (kPlayer.canDoCivics((CivicTypes)iC))
					a.push_back(picojson::value(std::string(GC.getCivicInfo((CivicTypes)iC).getType())));
			o["availableCivics"] = picojson::value(a);
			return CvString(picojson::value(o).serialize().c_str());
		}
		if (strcmp(szAction, "availableBuilds") == 0)
		{
			picojson::value::array a;
			for (int iB = 0; iB < GC.getNumBuildInfos(); ++iB)
			{
				const TechTypes ePrereq = (TechTypes)GC.getBuildInfo((BuildTypes)iB).getTechPrereq();
				if (ePrereq == NO_TECH || GET_TEAM(kPlayer.getTeam()).isHasTech(ePrereq))
					a.push_back(picojson::value(std::string(GC.getBuildInfo((BuildTypes)iB).getType())));
			}
			o["availableBuilds"] = picojson::value(a);
			return CvString(picojson::value(o).serialize().c_str());
		}

		const int iIdx = GC.getInfoTypeForString(szType, true);
		if (iIdx < 0)
		{
			o["error"] = picojson::value(std::string("type not loaded this game"));
			return CvString(picojson::value(o).serialize().c_str());
		}

		std::string sNotes;

		if (strcmp(szAction, "canConstruct") == 0)
		{
			o["city"] = picojson::value((double)iCityId);
			o["legacy"] = (pCity != NULL) ? picojson::value(pCity->canConstruct((BuildingTypes)iIdx)) : picojson::value();
			o["legacyReason"] = (pCity != NULL) ? picojson::value(std::string(legacyBlockReason(pCity, (BuildingTypes)iIdx))) : picojson::value();
		}
		else if (strcmp(szAction, "canTrain") == 0)
		{
			o["city"] = picojson::value((double)iCityId);
			o["legacy"] = (pCity != NULL) ? picojson::value(pCity->canTrain((UnitTypes)iIdx)) : picojson::value();
		}
		else if (strcmp(szAction, "canResearch") == 0)
		{
			o["legacy"] = picojson::value(kPlayer.canResearch((TechTypes)iIdx));
		}
		else if (strcmp(szAction, "canDoCivics") == 0)
		{
			o["legacy"] = picojson::value(kPlayer.canDoCivics((CivicTypes)iIdx));
		}
		else if (strcmp(szAction, "canCreate") == 0)
		{
			o["legacy"] = picojson::value(kPlayer.canCreate((ProjectTypes)iIdx));
		}
		else if (strcmp(szAction, "canMaintain") == 0)
		{
			o["legacy"] = picojson::value(kPlayer.canMaintain((ProcessTypes)iIdx));
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
			// Engine COUNT ground-truth for this BUILDING_/UNIT_: the per-empire live count + the world lifetime-created
			// count, plus (buildings) the engine's own maxed-out flags at empire/team/world -- the cap-gate inputs.
			const bool bUnit = (strncmp(szType, "UNIT_", 5) == 0);
			picojson::value::object t;
			if (bUnit)
			{
				t["empire"]       = picojson::value((double)kPlayer.getUnitCount((UnitTypes)iIdx));
				t["worldCreated"] = picojson::value((double)GC.getGame().getUnitCreatedCount((UnitTypes)iIdx));
			}
			else
			{
				t["empire"]       = picojson::value((double)kPlayer.getBuildingCount((BuildingTypes)iIdx));
				t["worldCreated"] = picojson::value((double)GC.getGame().getBuildingCreatedCount((BuildingTypes)iIdx));
				t["maxedEmpire"]  = picojson::value(kPlayer.isBuildingMaxedOut((BuildingTypes)iIdx));
				t["maxedTeam"]    = picojson::value(GET_TEAM((TeamTypes)iTeam).isBuildingMaxedOut((BuildingTypes)iIdx));
				t["maxedWorld"]   = picojson::value(GC.getGame().isBuildingMaxedOut((BuildingTypes)iIdx));
			}
			o["domain"] = picojson::value(std::string(bUnit ? "unit" : "building"));
			o["tally"] = picojson::value(t);
			sNotes = "engine counts: empire live count + world lifetime-created (+ building maxed-out flags)";
		}
		else
		{
			o["error"] = picojson::value(std::string("unknown computed action"));
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
		static const Route ROUTES[] = {
			{ "/state/all",     "gamestate",    "the entire raw state in one document (world/players/cities/plots)" },
			{ "/state/techs",   "stateTechs",   "every player's completed techs" },
			{ "/state/players", "statePlayers", "raw player facts (era/civics/traits/counts; no computed rates)" },
			{ "/state/cities",  "stateCities",  "raw city substrate: plots/buildings/specialists/bonuses (no yields)" },
			{ "/state/plots",   "statePlots",   "every map plot by global index: contents/facts + workingCity link (no yields)" },
			{ "/state/units",   "stateUnits",   "raw unit facts (type/ai/pos/group/damage/level/promotions)" },
			{ "/computed/cities/yields",   "cityInput",      "getYieldRate100 per channel + full per-source decomposition" },
			{ "/computed/cities/buildable","cityBuildable",  "the engine's canConstruct TRUE-set for the city (the buildable oracle)" },
			{ "/computed/cities/trainable","cityTrainable",  "the engine's canTrain TRUE-set for the city (the trainable oracle)" },
			{ "/computed/players",         "playerInput",    "empire economy: gold/science/upkeep/inflation/demographics" },
			{ "/computed/canConstruct",    "canConstruct",   "engine buildability verdict (type=BUILDING_X[&city=M])" },
			{ "/computed/canTrain",        "canTrain",       "engine trainability verdict (type=UNIT_X[&city=M])" },
			{ "/computed/canResearch",     "canResearch",    "engine canResearch verdict (type=TECH_X)" },
			{ "/computed/canDoCivics",     "canDoCivics",    "engine canDoCivics verdict (type=CIVIC_X)" },
			{ "/computed/canCreate",       "canCreate",      "engine canCreate verdict (type=PROJECT_X)" },
			{ "/computed/canMaintain",     "canMaintain",    "engine canMaintain verdict (type=PROCESS_X)" },
			{ "/computed/availableTechs",  "availableTechs", "engine canResearch set for the player" },
			{ "/computed/availableCivics", "availableCivics","engine canDoCivics set for the player" },
			{ "/computed/availableBuilds", "availableBuilds","engine build-unlock set for the player" },
			{ "/computed/tally",           "tally",          "engine counts (type=BUILDING_X|UNIT_X)" },
			{ "/computed/whyNot",          "whyNot",         "canTrain decision inputs (type=UNIT_X)" },
			{ "/computed/game",            "game",           "turn / game-over / winner / victory countdowns" },
		};
		const int iNumRoutes = (int)(sizeof(ROUTES) / sizeof(ROUTES[0]));

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
			char* szTok = szQuery;
			while (szTok != NULL && *szTok != '\0')
			{
				char* szNext = strchr(szTok, '&');
				if (szNext != NULL) { *szNext = '\0'; ++szNext; }
				if (strncmp(szTok, "type=", 5) == 0) { strncpy(szType, szTok + 5, sizeof(szType)); szType[sizeof(szType) - 1] = '\0'; }
				else if (strncmp(szTok, "player=", 7) == 0) iPlayer = atoi(szTok + 7);
				else if (strncmp(szTok, "city=", 5) == 0) iCity = atoi(szTok + 5);
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
			if (evalRequestBlocking(ROUTES[i].szAction, szType, iPlayer, iCity, szAnswer))
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

	// Refresh the published header (turn + gameId) for the server thread's response metadata and the
	// /events hello. No bulk walk: /state and /computed are served on this thread via the mailbox.
	bst::shared_ptr<GameSnapshot> pNew(new GameSnapshot());
	pNew->iTurn = GC.getGame().getGameTurn();
	pNew->szGameId = GC.getGame().getGameId();

	EnterCriticalSection(&g_snapshotLock);
	g_pSnapshot = pNew;
	LeaveCriticalSection(&g_snapshotLock);
}
