#pragma once
#ifndef CV_CASCADE_GRANTS_H
#define CV_CASCADE_GRANTS_H

#include "CvEventSpine.h"

//
//	CvCascadeGrants -- the #430 GRANTS machine (the "provisions" consumer). An IEventConsumer on the event spine: on a
//	DOMAIN state-change it resolves the SOURCE entity's GENUINE grants (the CvJsonGrants unit composed on its
//	CvJson<X>Info in the InfoRepo, minus the deferred mission-keys) and emits a [GRANTS] shadow diagnostic of what it
//	WOULD hand out. It does NOT apply -- legacy still applies; this is the resolution + observability surface.
//
//	⏳ Slice-1 scope: the DOMAIN events the spine emits today -- building-built (CASCADE_EVT_BUILDING_COUNT, delta>0)
//	and unit-created (CASCADE_EVT_UNIT_COUNT, delta>0) -> the on-build / on-create grants. Un-run parity (owner: no
//	live parity until everything is in). Follow-on increments (grants-machine.md): the remaining triggers
//	(per-turn recurring, tech first-discover, civ/religion/civic/game-start -- each needs a new DOMAIN event), the
//	richer grant mapping (the per-type mapFrom / CvJsonGrants unit skips bool/dict grants + drops the repeatable interval/chance), and the true
//	diff-vs-legacy shadow. Registered at the composition root (cascadeRegisterConsumers -> cascadeRegisterGrants).
//
class CvCascadeGrants : public IEventConsumer
{
public:
	int wantedKinds() const { return (1 << EVENTKIND_DOMAIN); }
	void onEvent(const CvCascadeEvent& kEvent);
};

//	Register the grants consumer + its [GRANTS] spine domain (idempotent). Called from cascadeRegisterConsumers.
void cascadeRegisterGrants();

#endif // CV_CASCADE_GRANTS_H
