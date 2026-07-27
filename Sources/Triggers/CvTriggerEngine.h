#pragma once
#ifndef CV_TRIGGER_ENGINE_H
#define CV_TRIGGER_ENGINE_H

#include "Spine/CvEventSpine.h"

//
//	CvTriggerEngine -- the #430 PAYLOAD plane, and the ONE machine that applies it. An IEventConsumer on the event
//	spine: a DOMAIN state-change arrives, the source entity's payload is resolved off its CvJson<X>Info in the
//	InfoRepo, and the appliers hand it over.
//
//	⚖ TRIGGER IS THE TOP-LEVEL CONCEPT; A GRANT IS A TRIGGER WITH A NULL CONDITION (owner) -- which is why ONE
//	machine and ONE spine domain (SD_TRIGGERS, the [TRIGGERS/*] tags) serve both. A `triggers` entry is the general
//	form (a happening, odds, an action); a `grants` block is its degenerate case, where the happening is implicit
//	(the source's own considered action), there is no condition and no roll, so the action simply applies.
//	`grants` stays a first-class AUTHORING shape (json.md §5) -- the split is about how a modder writes it, never
//	about two runtime mechanisms.
//
//	⛔ Being on the spine is the point, not an implementation detail: every evaluation ANNOUNCES itself, so a
//	payload that fires -- or fails to -- is visible. An off-loop path that rolls dice in silence is invisible on
//	both axes at once, unexercised AND uninstrumented, which is what makes that class the worst legacy to leave
//	standing. Anything the plane cannot land reports through the ONE load census (jsonNoteUnconsumed).
//
class CvTriggerEngine : public IEventConsumer
{
public:
	int wantedKinds() const { return (1 << EVENTKIND_DOMAIN); }
	void onEvent(const CvSpineEvent& kEvent);
};

//	Register the trigger consumer + its [TRIGGERS] spine domain (idempotent). Called from spineRegisterConsumers.
void triggerRegisterConsumer();

#endif // CV_TRIGGER_ENGINE_H
