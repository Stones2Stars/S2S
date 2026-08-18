#pragma once
#ifndef CV_ALLOWED_H
#define CV_ALLOWED_H

//
//	CvAllowed -- the `allowed` caps as ONE composable unit (json.md §4.4): scope self-caps (world/team/empire =
//	"at most N of me at that scope") + the wonder-category per-city caps (worldWonders/teamWonders/nationalWonders,
//	on CultureLevel). Composed BY VALUE on the derived infos that author it (buildings, culturelevels, projects,
//	specialbuildings, techs, units). WRITE-ONCE AT LOAD. The enabler's cap gate compares count(me, scope) < cap.
//
//	The key axis is the CLOSED §4.4 vocabulary, interned to EnAllowedCap at PARSE
//	(docs/architecture/patterns.md §Materialize at mapFrom): the authored strings exist on the parse surface only; storage is a fixed
//	array and every read is a bare typed load.
//

namespace picojson { class value; }

// The closed §4.4 cap-kind vocabulary (census over Assets/Data: world/team/empire + the three authored
// wonder-category caps; totalWonders is spec-reserved, zero-authored).
enum EnAllowedCap
{
	ALLOWEDCAP_WORLD,             // self-cap: at most N of me anywhere
	ALLOWEDCAP_TEAM,              // self-cap: at most N per team
	ALLOWEDCAP_EMPIRE,            // self-cap: at most N per player
	ALLOWEDCAP_WORLD_WONDERS,     // category count-cap per city (CultureLevel)
	ALLOWEDCAP_TEAM_WONDERS,      // category count-cap per city (CultureLevel)
	ALLOWEDCAP_NATIONAL_WONDERS,  // category count-cap per city (CultureLevel)
	ALLOWEDCAP_TOTAL_WONDERS,     // reserved by json.md §4.4; zero-authored
	NUM_ALLOWEDCAP
};

class CvAllowed
{
public:
	CvAllowed();

	// The unit's single load-time writer: parse the `allowed` object ({capKind: N}); the authored string keys
	// intern to EnAllowedCap here and nowhere else.
	void parse(const picojson::value& v);

	// The cap at eKind, or -1 = uncapped/absent. A bare array load -- no per-call string or map walk.
	int cap(EnAllowedCap eKind) const { return m_aiCaps[eKind]; }
	int authoredCount() const;   // how many cap kinds are authored (the load census read)
	bool isEmpty() const { return authoredCount() == 0; }
	void clearParsed();   // the clear-first half of the full-registry section re-map

private:
	int m_aiCaps[NUM_ALLOWEDCAP];             // -1 = uncapped/absent
	CvAllowed(const CvAllowed&);              // noncopyable (held by-value on the noncopyable info)
	CvAllowed& operator=(const CvAllowed&);
};

#endif // CV_ALLOWED_H
