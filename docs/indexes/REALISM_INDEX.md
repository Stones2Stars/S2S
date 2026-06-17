# The S2S Realism Index™

> A rigorously unscientific catalogue of the game's most **super realistic** mechanics,
> ordered by the distance between what the mechanic claims to depict and anything that has
> ever happened on Earth. Distance is measured in **centibows (cb)** — the SI unit of
> verisimilitude, calibrated against an archer on a standard-size map providing effective
> fire support to a battle roughly **three hundred kilometers away**.
>
> Sibling publication of the [Despair Index](DESPAIR_INDEX.md). The Despair Index is for
> bugs — things the code does wrong. This index is for *features* — things the code does
> exactly as designed, which is somehow worse. This document is not holy writ. It is an
> outlet. No process shall ever cite it.

---

## 🥇 1. The Clandestine Battering Ram — 113 cb *(first over the cap)*

A battering ram cannot be ordered to batter anything. It has no bombard command, no
button, no siege verb of any kind. Its entire reason for existing — reducing city
defenses — is implemented as a **hidden dice roll** (`iBreakdownChance`, 25%) that procs
per combat round *only while the ram is personally assaulting the garrison*, an
engagement a strength-4 wooden frame conducts approximately once. The walls are chipped,
if they are chipped, as an invisible side effect of the ram's death, at a rate scaled
down by the defenses it is trying to chip.

Nobody is told any of this. The mechanic's documentation lives on a fifteen-year-old
forum thread; the in-code comments reveal it was *originally* a pure no-combat "siege
event" that was later converted to real combat with a note that the rams' strength
should probably be reduced "when I get into the H2H/Distance mechanism" — a mechanism
the code is still waiting for. The AI, for its part, values breakdown chance like a
bombard rate it could aim, overbuilds the units, and then discovers the only way to cash
the stat is to die holding it.

At best, stealthy. At worst, the committee's final verdict: **the only weapon in military
history that files its battle report as an obituary** — damage nobody ordered, through a
mechanism nobody documented, from a unit nobody will see again.

The mechanic appears in none of the public documentation, so nobody ever figured it
out — they just learned the practical consequence: these units are wildly useless unless
you either **merge them to infinity** under Size Matters, so a battering ram becomes
statistically able to survive assaulting a fully fortified city, or **produce them to
hell**, so you can afford to waste a few dozen as ammunition. A siege weapon whose two
viable doctrines are "singularity ram" and "human-wave the woodwork."

Which is its own realism citation: the historical battering ram was a thing you schlepped
for miles, hid behind, and could never actually lose — a roofed, mantleted, reusable piece
of capital equipment that outlived the men swinging it. It was not something you built in
the hundreds and fed to a wall like crackers.

Per the calibration note, readings above 100 are reserved for mechanics that violate not
merely military history but causality — and this is the first entry to qualify: a weapon
whose **effect requires the destruction of its cause**. The longer you think about it,
the crazier it gets. It is bad game design elevated to an artform, and the committee
(the owner, thinking about it some more) has ruled it off the scale.

*Status: working as designed, allegedly. The army-lifecycle work (#410) owns the
question of what early-era siege should actually be.*

---

## 🥈 2. The 300-Kilometer Longbow — 100 cb *(definitional)*

On a standard map a tile spans something on the order of hundreds of kilometers. The
ranged-attack mechanic let **every ranged unit** fire into the neighboring tile — an
archer loosing a volley at six orders of magnitude beyond the effective range of a
longbow, from Oslo, hitting a man standing in Bergen.

The masterstroke is what the volley accomplished: until roughly the tank era, **nothing**.
The damage numbers were so small that entire armies camped outside cities for centuries,
performing the ritual potshot as institutional proof of effort — the *"yay, we did
something at least"* clause, fired across three hundred kilometers, dealing zero.

*Status: turned off by the owner in self-defense. Scheduled to return as a siege-only
mechanic (#410), where "bombarding the adjacent tile" abstracts to conducting siege
operations against fortifications you are actually encamped at — the one reading where
the fiction and the mechanics agree.*

---

## 🥉 3. Warfare by Appointment Only — 85 cb

For most of the early game, every unit on the map has exactly **1 movement point** —
raider and farmer, wolf and warlord, all marching in perfect lockstep. Consequence: in
the open field, **nothing can ever catch anything**. Every engagement is strictly
consensual; the enemy you can see will be precisely one tile further away tomorrow,
forever, like a horizon with a grudge.

A horde of sixty raiders can therefore be thinned by exactly two mechanisms: you find
and raze their home town, or they spontaneously decide to have a go. Until one of those
occurs, both armies coexist on the steppe in an eternal, mutually visible, perfectly
safe stalemate — history's first cold war, conducted at walking pace.

*Status: structural. Partially relieved barb-side by horde courage (#409 — they now
decide to have a go); the civ-side army lifecycle is #410.*

---

## 4. The Hundred-Year Picnic — 75 cb

An AI stack assembles, marches to an enemy city, encamps — so far, textbook Vegetius —
and then discovers its composition is entirely ranged units, which cannot assault
anything. Lacking both an assault component and any concept of "this is not working,"
the army settles in: taking passive damage from the defenses, returning fire with the
zero-damage 300-kilometer volleys of entry №1, generation after generation, a siege
with all the military significance of a family reunion in hostile weather.

No commitment, no completion, no retreat. Just vibes, attrition, and the unshakeable
institutional conviction that the potshots will, eventually, maybe, do something.

*Status: the acceptance test for the #410 army model — that stack must either requisition
assault troops or go home.*

---

## 5. The Grid Before the Generator — 72 cb

You discover Electricity. Eager to enter the modern age, you order a power plant. You
cannot. First your city must build **Power Lines** — a complete electrical distribution
network, erected pole by pole, for electricity that *does not exist anywhere on the
planet* — and only once the dead copper is humming with anticipation does the game
permit you to construct something that generates power to put in it. `BUILDING_POWER_LINES`
is the gateway prerequisite for the entire electric economy (45 references downstream),
making "wiring the city for nothing" the mandatory first act of every electrical age.

And every city repeats the whole ritual independently — poles, then plant, in each one —
because power is strictly municipal (see the issue trying to make electricity empire-wide).

*Status: working exactly as designed. Empire-wide power generation/consumption is the
productive direction (#411).*

---

## 6. The 326 Artisanal Supply Chains — 68 cb

The game tracks **481 distinct bonus resources**, of which **326 are manufactured goods**
— each a discrete, named, individually-plumbed commodity flowing through the trade
network. This is not an abstraction of an industrial economy; it is an attempt at a
full-relational inventory of one, in a game whose map cannot decide how long a tile is
(see entry №1).

Special mention to the energy aisle, where the seventeen-plus distinct power plant types
include "Osmotic", "Jetstream", "Quantum Vacuum", and — for the late game — "Cosmic
Expansion", a power plant fueled by the universe getting bigger.

*Status: working exactly as designed, at O(bonuses) cost in every prereq sweep the
constructibility work (#195) keeps having to index around.*

---

## 7. You Are What You Build — 48 cb

A civilization's character is not chosen, inherited, or earned over centuries — it is
*constructed*. Finish the right National Wonder and your people acquire an innate trait, the
same indelible kind a leader is born with; raze that building and the trait evaporates, the
national character politely reverting as the rubble settles. Identity is a function of your
current building inventory, recomputed on every completion and demolition, and the game
applies it with total sincerity.

The realism is impeccable in its own way: monuments *do* shape who a people believe
themselves to be — so the engine models it literally, as a wire running from a foundation to
the soul. Build the war-monument, become Aggressive. Build the observatory and read the
stars into your blood. Knock it down, forget who you were. No metaphor survives contact with
the implementation; here the metaphor *is* the implementation.

*Status: working exactly as designed — a building grants a `bCivilizationTrait` via its
`FreeTraitTypes`, applied through the very same `setHasTrait`/`processTrait` path a leader's
own traits use. Its bug-catapulting twin holds court in the
[Despair Index](DESPAIR_INDEX.md). No issue yet dares propose making national identity less
load-bearing on masonry.*

---

## 8. Cultural Exports — 44 cb

A civilization's culture is, the game maintains, a tradeable commodity. Found the national
wonder for a Culture and you receive a `BONUS_*` representing it; because that bonus carries
`TechCityTrade = NONE`, it is tradeable from the first turn, no enabling technology required.
Your culture now sits in the diplomacy screen between Iron and Incense — available to sell,
gift, or demand at the negotiating table.

The realism is, in its way, unimpeachable: cultures *are* exported, exchanged, and absorbed
between nations. The game simply renders the metaphor as freight — the Abbasid identity, FOB
your capital, three gold a turn, cancel anytime. There is no mechanism for the buyer to
*become* more Abbasid, nor for you to become less; the commodity changes hands and means
nothing, which is itself a fairly honest model of the international culture trade.

Underneath, this rides a `building → bonus → building` indirection: 410 of the game's 907
bonuses — roughly 45% — are these `BONUSCLASS_CULTURE` phantoms, each existing only to relay a
wonder's existence to the buildings it unlocks. It is not a bug, and it catapults nothing at
anyone; it is merely dumb, and it quietly taxes every load and every resource sweep for the
privilege.

*Status: working exactly as designed — a `BONUSCLASS_CULTURE` resource granted by a wonder,
tradeable by default. The whole tier is slated for collapse to a direct building→building
enable in #428's purge backlog, which retires both the phantom resources and the cultural
commodities exchange.*

---

## 9. The Net Present Value of the Cosmos — 9 cb

Space is not, it turns out, *permanently* barren — it is barren **until prepared**. Space
terrain (`TERRAIN_NONE`) ships with no yields at all — zero food, zero production, zero
commerce — but the game models the obvious real constraint, that you cannot farm a vacuum
until you have made it habitable, as a **preparation step**: a terraform-class worker build
that converts the dead plot into terrain that actually yields. This part is genuinely
realistic, and it works exactly as designed. You make the ground first; only then does it
give.

The catch is *where that knowledge lives*. The worker AI and the city AI both understand
the preparation step completely — hand them a space city and its automated workers will
dutifully terraform its tiles into productivity, no different from draining a marsh. The
**founding** AI understands none of it. `AI_foundValue` scores a candidate site by the raw
nature yield of the tiles around it — the unprepared, pre-terraform zero — so the entire
cosmos reads as worthless gravel and the AI never plants the flag. The hands know exactly
how to make space bloom; the part of the brain that decides *where* to build a city has
never been introduced to the hands.

The settlers, in effect, simply cannot be bothered to step outside and check whether
anywhere else might be worth the trouble. Home is best, and all that. They glance at the
void, see nothing growing, and conclude — correctly, by their own arithmetic — that there
is no reason to go.

And here is the exquisite cruelty: had the AI ever, by accident or by the mercy of a stray
script, managed to place a single city in space, that one city would have *worked*. Its
automated workers would have prepared its tiles and it would have hummed along like any
other. But it would have remained an island of exactly one — because every *additional*
space city faces the identical founding gate the first one only slipped through by fluke.
The capability is real, reachable, and one missing heuristic away — gated forever behind a
settler who would simply rather stay home.

The deepest stratum of realism here is not the vacuum but the **promise**. Somewhere back
in the C2C era, well over a decade ago, someone shipped the space maps on the assumption
that teaching the AI to understand them would be the easy part, a tidy follow-up for some
near future. That follow-up has since outlived the assumption, the original codebase, and a
good fraction of the player base. It survives today as Phase **B7** of the multimap→zone
rework — "make space extend Earth's gradients so existing AI carries over" — the same
decade-plus IOU, reissued in fresh ink. The cosmos has been "coming next version" for
longer than most software lives.

*Status: working as designed where it counts (terraform + worker/city AI both understand
preparation), blind where it matters (`AI_foundValue` scores the raw, pre-terraform yield,
so the AI never founds the one city that would teach itself the rest). AI space awareness is
explicitly deferred to Phase B7 of the multimap→zone rework
(`docs/dev/plans/multimap-zone-rework.md`), and the founding blindness is now tracked as
issue #433. ETA: historically, "next version."*

---

*Scale calibration note: 100 cb = 1 full Longbow. Readings above 100 are reserved for
mechanics that violate not merely military history but causality.*

*Contribution policy (owner-sanctioned): when play or rework surfaces a mechanic that is
working exactly as designed and the design describes a physically impossible world — a
"super realistic" mechanic — it may be added here, with a score, a name, and a citation
to whichever issue is trying to make it less realistic in the productive direction.*

*Fresh realism may be reported, contested, or savored on the
[S2S Discord](https://discord.gg/R8Uejx6uaK).*

*Sibling publications: the [Despair Index](DESPAIR_INDEX.md) (things the code does wrong) and
the [Complexity Index](COMPLEXITY_INDEX.md) (one entry, and it is everything).*
