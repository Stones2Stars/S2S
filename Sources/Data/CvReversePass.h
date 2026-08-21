#pragma once
#ifndef CV_REVERSE_PASS_H
#define CV_REVERSE_PASS_H

//
//	reversePass -- the ONE general reverse pass over the COMPILED info surfaces (docs/cascade.md §1 (reverse lookups are populated once, at load);
//	modifier.md §1/§4; enabler.md §2/§7.1). Runs once per loadJson full pass, after every entity is mapped and
//	its sections/deposits are compiled, and produces the complete reverse view in four sub-passes:
//
//	  (1) FORWARD COMPAT RECONSTRUCTIONS -- the store-inverted authored views un-inverted back onto the legacy
//	      forward getters the consumers still read (route<-bonus prereqs, the
//	      tech-FK un-inversions onto bonus/build/corp/religion/process/promotion/promotionLine/project,
//	      special-building techPrereq, project<-project needs), plus the tech-side forward obsoletion views
//	      (EDGEF_OBSOLETES onto the techs for buildings/processes -- the enabler's O(delta) tech application),
//	      plus the religion SHRINE-BUILDING registry feed (each building's §9 `shrine` FK ->
//	      CvReligionInfo::addShrineBuilding; the legacy self-register path died in the cutover).
//	  (2) EDGEF_RELATED -- the display/pedia candidate SUPERSET: every FK an info's compiled surface references
//	      (edge buckets, requires-tree atoms/predicates, deposit target FKs + condition trees + per-scalers,
//	      grants lists, provides, triggers) lands the referencing info on the referenced info's RELATED bucket.
//	      Edge references land BOTH directions (the store-inversion means either side may carry the authored
//	      edge; symmetry is what makes "author either side" true for display). A referenced info that composes
//	      no CvEdges (the plot substrate, unitcombats, properties) silently lands nothing -- same receiver set
//	      as the retired bespoke pass; a source KIND with no EnEdgeBucket cannot be landed and is skipped.
//	  (3) EDGEF_REQUIRED_BY -- the enabler's requires-reverse-index (enabler.md §7.1 step 2): every FK-resolved
//	      atom/predicate in every dependent's requiresBuild/requiresOperate trees + its dormant triggers lands
//	      the dependent under its kind bucket on the referenced HAVE-axis info.
//	  (4) THE OWN-OUTPUT REVERSE LANDING (modifier.md §4; roadmap § the GENERAL modifier own-output reverse-map):
//	      a source's target-keyed own-output deposit is reverse-landed on the TARGET as a compiled conditioned
//	      own-output entry ("+X while the source is present"), so a modder may author either side and the
//	      relationship is landed on the other programmatically. Two landing classes: a yield-channel flat keyed
//	      to a plot-substrate target (improvements/terrains/features/routes) from a building/civic/tech source
//	      lands PLOT-scope; a buildings-keyed output-channel deposit (the nine channels: gold/culture/research/
//	      espionage/commerce/food/production/happiness/health) from a building/civic/tech source lands on the
//	      target BUILDING at CITY scope (§2a building output / §2b wellbeing), presence-gated at the AUTHORED
//	      deposit's scope axis, an authored condition composed in (info-rebuild.md ruling 19).
//	      Governing-deliverer keyed maps STAY source-side (buildRate keyed targets; every TRAIT keyed deposit
//	      per the §4 per-set carve-out; route-keyed improvement yields per the §4 exemplar -- the ROUTE's own
//	      compiled keyed entries ARE the data, no improvement-side row is written (the wave-B improvement mirror
//	      is deleted); civic<-features happiness per the §2b one-term bundling).
//	  (5) THE UNIT-PLANE POST-MAP DERIVATION (json.md §9 sizeMatters: the unit's quality/group/size RANK is
//	      DERIVED at load, never stored): once the full registry is mapped, every CvUnitInfo recompute-assigns
//	      its load-derived members -- the SM base-rank/strength/cargo sums over its combat classes, the first
//	      prereq TECH atom's era (classified by the resolved atom id through the ONE type dispatch), the
//	      can-acquire-experience verdict, and the direct-upgrade transitive closure (the upgrade chain).
//
//	After this pass every info ALREADY CARRIES its reverse lookups + load-derived values; no consumer builds its
//	own scan or side index, and no getter derives lazily. LOAD-ONLY: called by loadJson only, inside the
//	write-once-at-load window.
//

// The pass's observability counters (emitted by loadJson as RJE_REVERSE_DONE + the Loading.log reverse-view line).
struct ReversePassCounts
{
	int relatedAdds;        // raw EDGEF_RELATED inversions (pre-dedup)
	int requiredByAdds;     // raw EDGEF_REQUIRED_BY inversions (pre-dedup)
	int ownOutputLanded;    // synthesized own-output entries landed on their targets (plot-substrate + buildings-keyed)
	int ownOutputSkipped;   // landing-shaped entries left source-side, reported never guessed at (plot-substrate:
	                        // conditioned / non-flat / kinded; buildings-keyed: member-kinded)
	unsigned int milliseconds;
	ReversePassCounts() : relatedAdds(0), requiredByAdds(0), ownOutputLanded(0), ownOutputSkipped(0), milliseconds(0) {}
};

// Run the four sub-passes + the derived-list dedup. Idempotent per loadJson full pass (the repos are cleared
// and re-mapped before each).
void reversePassRun();

// The counters of the most recent reversePassRun.
const ReversePassCounts& reversePassCounts();

#endif // CV_REVERSE_PASS_H
