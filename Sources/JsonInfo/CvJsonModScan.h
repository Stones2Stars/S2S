#pragma once
#ifndef CV_JSON_MOD_SCAN_H
#define CV_JSON_MOD_SCAN_H

//
//	JsonModScan -- the ONE shared read surface over a poco's composed CvJsonModifiers (json.md §6 families), used to
//	MATERIALIZE typed members at mapFrom time ([DEC-single-implementation]; the per-file `civSum*` / `sumUnconditioned`
//	walkers this replaces were per-file duplicates of the same logic).
//
//	⛔ LOAD-TIME ONLY. Every call does string-address map lookups + a linear entry walk -- the exact per-call cost the
//	materialize-at-mapFrom ruling bans from getters ("remapping directly from a json read is a gigantic nono", owner
//	2026-07-16). A getter NEVER calls into this class: mapFrom scans ONCE into typed members / sparse caches, and the
//	getter is a bare member read. The cascade's own gated sums stay MMKernel's (DepositIndex-compiled, dirty-rebuild
//	cadence) -- this surface reads the RAW parsed families and exists only for the poco materialization pass.
//
//	The curator collapses a legacy scalar and its conditioned/qualified addends onto the SAME address: the plain
//	scalar carries no enabled/disabled/per/unitQual; conditioned entries carry an `enabled`; `unit:`-qualified ones a
//	unitQual; per-scaled ones a per. Each accessor below recovers exactly one of those shapes (pure DATA inspection --
//	never a runtime GC.getGame() read; [DEC-json-not-cascade] holds).
//

#include "CvJsonModifiers.h"
#include "CvJsonModEntry.h"
#include <string>
#include <vector>
#include <utility>

int jsonResolveId(const std::string& s);   // CvJsonParse -- FK type-string -> engine id (-1 unresolved)

class JsonModScan
{
public:
	// The plain legacy scalar at one family: unconditioned, non-`per`, un-qualified entries of `unit` only.
	static int familyUnconditioned100(const CvJsonModFamily* f, CvCascUnit unit);
	static int sum100(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit);
	static int sum(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit)
	{ return sum100(mods, address, unit) / 100; }
	static float sumF(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit)
	{ return sum100(mods, address, unit) / 100.0f; }

	// A target-KEYED read: the target's own GC type string is the address's trailing segment.
	static int sumKeyed(const CvJsonModifiers* mods, const std::string& baseAddr, const char* typeStr, CvCascUnit unit);

	// Does ANY family address under `prefix` exist? (plain map-key prefix scan)
	static bool hasPrefixedFamily(const CvJsonModifiers* mods, const std::string& prefix);

	// Sum EVERY entry of `unit` regardless of condition -- for a sub-scope authored ONLY as conditioned entries
	// (the landmark `plots` deposit), where unconditioned-only would wrongly read 0. Human (/100).
	static int sumAll(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit);

	// Sum entries carrying a `unit:` predicate qualifier (the IS_MILITARY-tagged per-unit entry on a shared leaf);
	// familyUnconditioned100 excludes these, so the two reads stay disjoint. Human (/100).
	static int sumUnitQualified(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit);

	// Sum entries whose `enabled` is exactly a bare predicate of kind `k` (IS_CAPITAL) -- recovers a legacy
	// capital-only value the curator merged onto the same leaf as the unconditioned one. Human (/100).
	static int sumEnabledPred(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit, CvCascPredKind k);

	// Sum entries whose `enabled` is a bare PRESENCE atom of the given type string (GAMEOPTION_MAP_PERSONALIZED) --
	// a bare "GAMEOPTION_*" string parses to a presence atom (cp_isTypeRef). Human (/100).
	static int sumEnabledPresence(const CvJsonModifiers* mods, const std::string& address, CvCascUnit unit, const char* typeStr);

	// Collect a target-keyed family prefix (happiness.empire.buildings.<B>, ...) into a sparse (id, value) vector --
	// the hot-path iteration form. The segment after `prefix` IS the target's type string (targets carry no dots).
	template <class IdT>
	static void collectKeyedSparse(const CvJsonModifiers* mods, const std::string& prefix, CvCascUnit unit,
	                               std::vector<std::pair<IdT, int> >& out)
	{
		if (!mods) return;
		const std::map<std::string, CvJsonModFamily*>& all = mods->all();
		for (std::map<std::string, CvJsonModFamily*>::const_iterator it = all.begin(); it != all.end(); ++it)
		{
			const std::string& key = it->first;
			if (key.size() <= prefix.size() || key.compare(0, prefix.size(), prefix) != 0) continue;
			const std::string typeStr = key.substr(prefix.size());
			if (typeStr.find('.') != std::string::npos) continue;   // only a direct target child, not a deeper member
			const int id = jsonResolveId(typeStr);
			if (id < 0) continue;
			const int v = familyUnconditioned100(it->second, unit) / 100;
			if (v != 0) out.push_back(std::make_pair((IdT)id, v));
		}
	}

	// Same collection into a setValue-style map (IDValueMap et al.) -- update-or-append per key, re-map idempotent.
	template <class KeyT, class MapT>
	static void collectKeyedMap(const CvJsonModifiers* mods, const std::string& prefix, CvCascUnit unit, MapT& out)
	{
		if (!mods) return;
		const std::map<std::string, CvJsonModFamily*>& all = mods->all();
		for (std::map<std::string, CvJsonModFamily*>::const_iterator it = all.begin(); it != all.end(); ++it)
		{
			const std::string& key = it->first;
			if (key.size() <= prefix.size() || key.compare(0, prefix.size(), prefix) != 0) continue;
			const std::string typeStr = key.substr(prefix.size());
			if (typeStr.find('.') != std::string::npos) continue;
			const int id = jsonResolveId(typeStr);
			if (id < 0) continue;
			const int v = familyUnconditioned100(it->second, unit) / 100;
			if (v != 0) out.setValue((KeyT)id, v);
		}
	}

private:
	JsonModScan();   // purely-organizational static-methods class -- never instantiated (patterns.md § DRY)
};

#endif // CV_JSON_MOD_SCAN_H
