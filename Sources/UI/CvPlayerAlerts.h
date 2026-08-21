#pragma once

#ifndef CV_PLAYER_ALERTS_H
#define CV_PLAYER_ALERTS_H

// ⚖ PLAYER ALERTS ARE A SPINE CONSUMER (event-spine.md § PLAYER ALERTS ARE A SPINE CONSUMER, owner).
//
// The legacy notifications were emitted from INSIDE the setter that changed the state, which is why they die
// with every legacy mutator that gets cut -- and why they cannot simply be kept: the setter they lived in is
// the duplicate being removed. They come back HERE, as a consumer of the DOMAIN fact that already announces
// the change, exactly as logging and the /events stream are consumers.
//
// ⛔ An alert is NEVER re-inlined at a mutation site. Doing so makes a UI concern a condition of the state
// change, and it is invisible to anything else that wanted to know.
//
// ⚑ This is a GROWING list, not a one-off: each legacy mutator cut takes its alerts with it, and they are
// re-added together on the facts. What is still owed lives in the todo.

void playerAlertsRegisterConsumer();   // register on the event spine (from spineRegisterConsumers; idempotent)

#endif
