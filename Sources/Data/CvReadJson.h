#pragma once
#ifndef CV_READJSON_H
#define CV_READJSON_H

//
//	readJson -- the ONE JSON reader ([DEC-one-json-reader]; patterns.md § The ONE reader). The single load-time
//	pipeline that feeds every JSON-fed info: it enumerates Assets/Data ONCE, parses each file ONCE into a
//	retained in-memory store, serves each category's entities to the per-category registration
//	(CvXMLLoadUtility::LoadGlobalClassInfoJson -- id assignment in `_order.json` manifest order at that
//	category's load point), and runs the full register->mapFrom->FK/reverse pass at the end of each XML load
//	phase. The store is freed completely when the postmenu pass ends -- after load, no JSON-shaped object
//	survives. Conditionals parse through the typed condition tree (cascadeParseCondition -> CvCondition);
//	each entity maps itself (virtual mapFrom) into the per-info-type InfoRepo.
//

#include <string>
#include <utility>
#include <vector>

namespace picojson { class value; }
class CvInfo;

// The XML load phases the reader runs at. The premenu/postmenu PHASING is LOAD-BEARING: premenu consumers
// need the premenu categories mapped before the menu, and the postmenu XML types (processes/votes/espionage/
// spawns) register late -- id resolution is REUSE-ONLY against the XML registry, so each pass maps exactly
// the types registered by then. The postmenu pass re-runs the idempotent mapFrom from the RETAINED store
// (never from disk), completing every cross-category FK edge, then frees the store.
enum JsonLoadPhase
{
	JSON_LOAD_PREMENU,
	JSON_LOAD_POSTMENU
};

//	The full pipeline pass: register every store entity's type->id (reuse-only), mapFrom each against the
//	complete registry, mint + resolve the classification registries, run the FK/reverse passes, and print the
//	UNCONDITIONAL fail-loud coverage summary (unresolved FKs / unconsumed sections / unknown keys) to
//	Loading.log. Runs at the END of LoadPreMenuGlobals and LoadPostMenuGlobals; the map is UNCONDITIONAL (no
//	gPlayerLogLevel dependency -- the [READJSON/*] survey rides the event spine, SD_READJSON).
void loadJson(JsonLoadPhase eLoadPhase);

//	The per-category registration feed: this category folder's parsed entities (type, parsed JSON) in
//	`_order.json` manifest order (absent-from-manifest sorts last, alphabetically), served straight from the
//	retained store -- no disk walk, no parse. The store builds on the FIRST call (the one disk read of
//	Assets/Data). The returned pointers stay valid until the postmenu pass frees the store.
void loadJsonCategory(const char* szDataFolder,
	std::vector<std::pair<std::string, const picojson::value*> >& aOutEntities);

//	The ONE INFOTYPE-prefix -> InfoRepo dispatch (naming.md routes by prefix): the mapped CvInfo for any
//	repo-homed type, or NULL (tokens, XML-only kinds). Serves the /state/info observability read + any
//	consumer needing an info by its type string.
CvInfo* rjInfoForType(const std::string& szType, int iId);

#endif // CV_READJSON_H
