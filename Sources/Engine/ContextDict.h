#pragma once
#ifndef CV_CONTEXT_DICT_H
#define CV_CONTEXT_DICT_H

//
//	ContextDict -- the ONE uniform keyed dictionary TYPE every derived POSSESSION store shares (CityContext /
//	EmpireContext / ...): a SEMIBOOLEAN state, read as a bool and stored as a COUNT ([DEC-keyed-accumulator]).
//	A dictionary, not a fixed struct, so each family's key set is OPEN -- a new key, never a reshape.
//
//	⛔ THE TYPE IS SHARED; THE INSTANCES ARE NOT. One dict PER AREA OF RESPONSIBILITY -- never one catch-all per
//	   object (owner). CityContext carries plotAttrs and the five vicinity tiers as SEPARATE named members, and
//	   CvCity owns the amenity dictionary itself -- that is the shape; it does not get "tidied" into a
//	   dicts[category] map. Four
//	   reasons, none stylistic: the key spaces are DISJOINT REGISTRIES (CASC_PRED_* / AMENITY_* / BONUS_*), and a
//	   merged store re-opens the cross-registry id collision the CLS_ prefix closed by construction; a different
//	   FACT maintains each, so a merged store has no single delta source; the SEMANTICS differ (a bit-fold over
//	   member plots, a grantor refcount, a tier membership); and a catch-all invites "just add a key here" for
//	   values that belong elsewhere. It is json.md's own ruling for the authoring blocks, applied to the stores:
//	   nukeImmune is ONE key naming TWO mechanics on two carriers, separable only because the blocks are distinct.
//
//	THE CONTRACT -- TWO consumer questions, and the GETTER'S NAME STATES WHICH ONE YOU ARE ASKING:
//	  has(id)      -- a GATE. "Does this city have power?" Presence, nothing more.
//	  count(id)    -- a SCALER. "How many river plots?" A keyed or plots-target deposit's output is
//	                  flat x count(id), so this is a first-class read TODAY, not a volumetric reserve.
//	  add(id, +-1) -- THE maintenance verb: a grantor started or stopped participating.
//	  clear()      -- zero at owner reset. A delta store is correct ONLY from a known zero, and a CvCity is
//	                  recycled out of an FFreeListTrashArray, so a reused slot would inherit counts that no
//	                  later delta can ever correct.
//
//	⚠ THE DISCIPLINE IS TO ASK THE QUESTION YOU MEAN, never to hide the int. A GATE reading count() is the
//	   defect -- that is where `== 1` / `> 2` appear and the refcount's meaning leaks into logic that only
//	   wanted presence. A SCALER reading has() is the mirror error, silently collapsing a magnitude to 1.
//
//	⛔ THERE IS DELIBERATELY NO set(). It overwrites a refcount, which is how a city holding TWO grantors and
//	   losing ONE drops a state the survivor still justifies (the workable-radius override is that bug in the
//	   wild). The verb is absent rather than forbidden so the mistake is UNSAYABLE, not merely banned.
//	⛔ A BITSET IS NEVER THE RIGHT STORAGE HERE, and not only because some keys have several grantors: the
//	   classification registries are OPEN, so a one-grantor key gains a second when DATA is authored, with no
//	   engine change -- a bitset would break silently on a data edit. The int also makes volumetric a change of
//	   MEANING rather than a reshape, which is why consumers must read has() and never the number.
//

//
//	⚖ IT IS A BASE, NOT A MEMBER -- a dictionary family IS a ContextDict and does not HOLD one (owner,
//	[DEC-dict-is-a-consumer]). A family that wraps this type instead of inheriting it becomes a bespoke class per
//	store, which is the hand-named-per-store shape [DEC-uniform-cache-shape] calls a defect: "every store is the
//	SAME OBJECT TYPE everywhere, and they ALL MAINTAIN the SAME WAY".
//
//	THE CONSUMER CONTRACT a family adds on top, and it is the same three parts every time:
//	  static bool wantsEvent(int)             -- THE DECLARED INTEREST SET: the facts that maintain this store,
//	                                             stated AT the store. A fact absent from it does not reach the
//	                                             dictionary, and that is readable here rather than inferable from
//	                                             a router elsewhere.
//	  static void onSpineEvent(const CvSpineEvent&)  -- resolve the owner from the fact, apply the delta.
//	  void <apply>(...)                       -- the delta itself: add(id, +-N). Never a recount, never a refill.
//	⛔ The dispatch is STATIC because a consumer is per FAMILY, never per INSTANCE: the per-owner dictionaries are
//	   resolved FROM the fact. Registering each instance would put one consumer per city on the spine, and giving
//	   this type a vtable would buy a per-instance dispatch that never happens.
//	⛔ A family REGISTERS inside the CONTEXTS band of contexts -> enabler -> modifier -> triggers -- the enabler's
//	   load-end gate pass evaluates THROUGH these stores, so order is a property of the band, never of which
//	   translation unit initialized first.
//	⚑ A dictionary that is not yet its own consumer simply declares nothing and is maintained from elsewhere --
//	   an honest statement of what is left to convert, not a shape to settle for.
//

#include <map>

struct ContextDict
{
	std::map<int, int> m;
	int  count(int id) const { std::map<int, int>::const_iterator it = m.find(id); return it != m.end() ? it->second : 0; }
	bool has(int id) const   { return count(id) > 0; }
	void add(int id, int d)  { m[id] += d; }
	void clear()             { m.clear(); }
	bool empty() const       { return m.empty(); }
};

#endif // CV_CONTEXT_DICT_H
