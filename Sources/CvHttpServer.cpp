#include "CvGameCoreDLL.h"
#include "CvHttpServer.h"
#include "CvBuildingInfo.h"
#include "CvCity.h"
#include "CvGameAI.h"
#include "CvGlobals.h"
#include "CvInfos.h"
#include "CvPlayerAI.h"
#include "CvSelectionGroup.h"
#include "CvTeamAI.h"
#include "CvUnit.h"

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
	// Backstop: drop new events beyond this. Sized for the #419 log stream's headline
	// bursts (hundreds of level-1 lines in a heavy turn; frames ~150-250B => worst case
	// a few hundred KB transient).
	const size_t EVENT_QUEUE_CAP = 2048;
	std::vector<SOCKET> g_sseClients;      // server thread only
	const size_t SSE_CLIENT_CAP = 8;

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
