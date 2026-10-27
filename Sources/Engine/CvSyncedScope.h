#pragma once

#ifndef CV_SYNCED_SCOPE_H
#define CV_SYNCED_SCOPE_H

class CvRandom;

///<summary>
/// Marks a root of SYNCHRONIZED execution: code every client runs in lockstep (turn processing, a network
/// command's Execute, an EXE-called game-mechanics entry point). A draw on the synchronized RNG streams outside
/// every such scope, once the game is final-initialized, is reported as [SYNC/draw] in Sync.log: it runs on one
/// client only and desyncs multiplayer (docs/reference/engine.md, the synchronized RNG is shared state).
///</summary>
class CvSyncedScope
{
public:
	CvSyncedScope();
	~CvSyncedScope();

	static bool isActive();

	///<summary>
	/// Called from CvRandom::getFrom before each draw. Reports the draw when it is on a synchronized stream,
	/// outside every synced scope, after final initialization; each calling site is reported once.
	///</summary>
	static void checkDraw(const CvRandom* pStream, const char* szLabel, const void* pCaller);

	///<summary>
	/// Whether this machine decides the game-wide rules: the network host, the PitBoss server, or the only machine.
	///</summary>
	static bool isRulesHost();

	///<summary>
	/// Writes [SYNC/host] -- what each host query answered on this machine -- so two players' Sync.log show
	/// which machine took the host's role.
	///</summary>
	static void reportHost();

private:
	static int s_iDepth;
};

#endif
