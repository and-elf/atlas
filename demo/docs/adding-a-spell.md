# How to add a new spell/attack today

This is a practical, step-by-step guide to adding a new attack, spell, or ability to `demo/` as it exists right
now — no new capability, no manifest, no build step. It follows the pattern `demo/tests/fireball_test.cpp`
already proves end-to-end; see `demo/README.md`'s "Damage over time, and Fireball" section for the full
narrative and reasoning behind it. If you haven't read that section, read it first — this document only covers
the mechanical steps, not why they work.

A more powerful, fully data-driven version of this (zero hand-written code per spell, moddable numbers) is
proposed in issue #144 and not built yet. Everything below is what actually works today.

## Before you start: is this really "just content"?

Ask first whether your new spell needs a genuinely new *mechanism* or just new *numbers* fed into mechanisms
that already exist. Concretely, check whether it's already covered by:

- A direct hit → `cast_time_attack::BeginCast` (has a wind-up) or `auto_attack::TryAutoAttack`/an instant
  `health::ApplyDamage` dispatch (no wind-up).
- A damage-over-time or heal-over-time component → `damage_over_time::ApplyDotEffect` (a negative
  `damage_per_tick` heals, the same "healing is signed damage" convention `health` already uses).
- A range-based buff/debuff that only applies while a target stays close → the `aura`/`haste` pattern
  (`ActivateAura`/`RefreshAuraEffect`, or the `CastSpeed`-flavored equivalent).

If your spell is some *combination* of these, you don't need a new capability — you need what Fireball needed:
one small piece of glue code reacting to an existing event. If it genuinely needs new mechanics no existing
capability provides (a new composition strategy, a new kind of property, a new request shape), that's a new
capability, and belongs in `modules/` with its own manifest — a much bigger step than this guide covers, and
worth a design discussion first (this repo's issue-first workflow: open an issue describing the mechanism before
writing it).

## Step 1: pick your primary request

Decide which existing request your spell casts, and gather the plain values (not properties, not resources —
literal numbers and IDs) it needs:

- `cast_time_attack::BeginCast { caster, target, obstacle, min_range, max_range, damage, cast_time_ticks, requires_stationary, animation }`
- `auto_attack::TryAutoAttack { attacker, target, obstacle, delta_ticks }` (only if your weapon-attack state is
  already seeded on the entity — see `auto_attack`'s own manifest)
- A direct `health::ApplyDamage { target, amount }` (an instant, no-wind-up hit — see `auto_attack`'s own
  "instant attack" shape, `InstantAttackBypassesTheAutoAttackCooldownEntirely`)

`min_range`/`max_range` are whole units (`int32`), not fractional — see `demo/README.md`'s "Auto-attack" section
for why. `animation` is a `ResourceId` naming content that doesn't exist in this repo yet (no rendering is
built) — a placeholder identity is fine for a test.

## Step 2: decide what happens when it lands

If your spell is *only* the direct hit, you're done after step 1 — dispatch the request from a test, done.

If something else should happen once it lands (a burn, a slow, a knockback), you need one subscription, written
once, at whatever composes your host (a `SimulatedHost`-style test fixture today — see
`demo/tests/simulated_host.hpp`):

```cpp
ctx.subscribe<cast_time_attack::CastLanded>([&](const cast_time_attack::CastLanded& landed) {
    damage_over_time::on_apply_dot_effect(ctx,
        damage_over_time::ApplyDotEffect{
            .target = landed.target,
            .damage_per_tick = landed.damage / 5,   // your spell's own numbers, not a lookup
            .tick_interval_ticks = 180,              // 3 seconds at 60 ticks/second
            .total_applications = 3,
        });
});
```

This is the entire cost of "this spell also does X on landing" — a lambda reacting to an already-generic event
(`CastLanded`, `AutoAttackLanded`, `DotEffectTicked`, `PathTargetReached` — whichever fits), dispatching another
already-existing generic request with your spell's own constants. No manifest, no `PropertyStore`, no build
step. See `demo/README.md`'s "Fireball itself: zero new types, one subscription" for the worked original.

**Know this pattern's real limit before relying on it for something bigger:** `damage_over_time` today has a
single `DotEffect` slot per target — a second `ApplyDotEffect` against an already-affected target discards
whatever was left of the first (`ApplyDotEffectRefreshesAnAlreadyActiveEffect`). If your spell needs to coexist
with *other* DoTs already active on the same target (not replace them), that's not supported today — see #144.

## Step 3: write the test

Follow `fireball_test.cpp`'s shape: dispatch your primary request with your spell's authored constants, drive
`AdvanceCast`/`AdvanceDotEffect`/whatever ticking is needed for exactly as many simulated ticks as your numbers
require, and assert the total effect (final `Health.current`, whatever else your spell touches). Compute your
tick counts from `core::Time::ticks_per_second` (`60`) explicitly in the test, the same way Fireball's `180`
ticks = 3 seconds is derived, not a magic number.

## What you get for free, and what you don't

You get: request validation and rejection (§6), property composition on anything your spell reads (`Armor`
mitigation, `CastSpeed` haste), replication of whatever properties it mutates, and cancellation
(`interruption::ActionInterrupted`, movement-cancel) if your primary request is `BeginCast`/`TryAutoAttack`.

You don't get, without further work: a `damage_type` field (no enum type in the manifest system yet), a random
damage range (`atlas::core::Random` exists but nothing wires it into a resolve step), multiple animation states,
or numbers moddable by talents/gear you didn't anticipate at the time you wrote the subscription lambda. See
`demo/README.md`'s "Damage over time, and Fireball" closing notes for the first three, and issue #144 for the
last one.
