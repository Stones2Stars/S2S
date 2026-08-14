# Espionage perk — SEE *FROM* a watched city (parked forward intent)

> Owner idea, parked un-killed. Not #430 work.

Espionage city visibility deliberately SEES THE CITY, never *from* it — the watcher registers the city plot
alone, and must not inherit the watched city's own observer budget ([vision.md](../../specs/vision.md)).

**The perk:** at EXTREMELY HIGH espionage against a player, that inversion becomes the reward — the watcher
gains the watched city's own eyes (*"see from the city"*: the city's full sight budget registered for the
spying team). A natural top-tier rung for the espionage ladder precisely because the base mechanic now
withholds it.

**Shape when taken up:** the sight the foreign leg registers becomes a function of the espionage tier instead
of the constant 0 — the one branch in `CvPlot::updateSight`'s city block. Nothing else moves; the brackets stay
symmetric as long as the tier read is stable across a register/deregister pair (re-bracket on the tier
crossing, exactly as other sight movers do).
