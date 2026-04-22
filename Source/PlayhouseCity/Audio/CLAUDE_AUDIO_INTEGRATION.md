# Audio System — Integration Notes for Claude (Cole's Side)

Tols's machine added an audio layer on top of Cole's `DevCole` work and pushed
it to `origin/tolsdev`. This doc tells you (Claude on Cole's machine) exactly
how to pull just the audio into Cole's `DevCole` branch without dragging
anything else along.

---

## TL;DR

```bash
git fetch origin
git checkout DevCole
git cherry-pick origin/tolsdev    # one commit, applies cleanly
```

That's it. The single commit is `Add audio system: ambient + jump/footstep
foley + weapon-fire stubs`. It's rebased on top of Cole's latest at the time
of the push, so it should apply with no conflicts unless Cole has since
re-edited `PHCCityCharacter.cpp`/`.h` or `PHCCitySubsystem.cpp` in the same
spots.

---

## What you get

**New files:**
- `Source/PlayhouseCity/Audio/PHCAudio.h` / `.cpp` — small helper namespace
  wrapping `UGameplayStatics::PlaySound2D` / `PlaySoundAtLocation`. Loads
  sounds by package path with a TWeakObjectPtr cache + AddToRoot. Logs a
  warning and no-ops if a path is missing — never crashes.
- `Content/Audio/` — Lyra-style starter pack restored: ambient (music, wind,
  birds), full Foley sneaker/hand/body sound bank, sound classes/mixes.
  These were originally on tolsdev but not on DevCole; the cherry-pick
  brings them back via LFS.

**Edits to existing files (3 hook points):**
- `PHCCitySubsystem::OnWorldBeginPlay` — 3 ambient loops (music, wind,
  birds) play immediately after `SpawnCityEnvironment()`.
- `PHCCityCharacter::HandleJumpStarted` — random sneaker-on-concrete grunt
  on the **grounded-jump branch only** (so wall-jump and slide don't trigger
  it; add separate calls if Cole wants those).
- `PHCCityCharacter::Tick` — footstep cadence (run 0.28s, walk 0.42s),
  random pick from 7 run + 5 walk samples. Gated to `bHasBeenGrounded` and
  `IsLocallyControlled()`. **Speed is computed from world-position delta**
  (not `GetVelocity()`) because the character moves via
  `AddActorWorldOffset`, so the physics velocity is always 0.
- `PHCCityCharacter::FireWeapon` — per-weapon-type SFX dispatch table at
  the top of the function. **The wav files don't exist yet** — see "Stubs"
  below.

**Two new members** in `PHCCityCharacter.h`:
- `float FootstepTimer` — countdown to next step
- `FVector LastFootstepLocation` — for delta-position speed calc

---

## Stubs that need wavs

`PHCCityCharacter::FireWeapon` references these paths. Until somebody
imports a wav at each one, you'll see `LogPlayhouseCity: Warning:
PHCAudio: Sound asset not found at '...'` once per missing weapon (cached
after first lookup, so no log spam).

Drop wavs at:
```
/Game/Audio/SFX/Weapons/Minigun_Fire.uasset    (CurrentWeaponType 0)
/Game/Audio/SFX/Weapons/RPG_Fire.uasset        (CurrentWeaponType 1)
/Game/Audio/SFX/Weapons/Sword_Swing.uasset     (CurrentWeaponType 2)
/Game/Audio/SFX/Weapons/Scythe_Swing.uasset    (CurrentWeaponType 3)
/Game/Audio/SFX/Weapons/Dagger_Stab.uasset     (CurrentWeaponType 4)
/Game/Audio/SFX/Weapons/AK47_Fire.uasset       (CurrentWeaponType 5)
/Game/Audio/SFX/Weapons/Sniper_Fire.uasset     (CurrentWeaponType 6)
```

Index in `FireSoundPaths[]` matches `CurrentWeaponType`. If Cole adds an
8th weapon type (e.g. flamethrower), append a path to that array — out-of-
range indices are bounds-checked and silently skipped.

---

## iOS / Mac safety

**No platform changes required.** All audio code uses cross-platform UE
APIs:
- `UGameplayStatics::PlaySound2D` / `SpawnSound2D` / `PlaySoundAtLocation`
- `LoadObject<USoundBase>`
- `UAudioComponent`

No editor-only data accessed at runtime. No platform `#if` guards. No
Slate. The Lyra ambient/foley assets cook fine for iOS automatically.

The `AudioMixer`, `AudioCapture`, and `Voice` modules are already in
`PlayhouseCity.Build.cs` — no Build.cs changes were pushed.

**One quirk worth knowing:** `PHCAudio::PlayAmbientLoop2D` mutates
`USoundWave::bLooping = true` at runtime so the Lyra raw wavs loop without
re-authoring as Cues. Works on iOS. The mutation is global per asset (any
later non-loop usage of the same wav would now loop), but for ambient
beds that's exactly the desired behavior. Long-term cleanup option:
convert each ambient `Starter_*.wav` to a USoundCue with Looping enabled
in editor and update the path in `PHCCitySubsystem::OnWorldBeginPlay`.

---

## What was deliberately NOT pushed

Tols's local checkout has one uncommitted change to `PlayhouseCity.Build.cs`
that disables Agora on Windows because the Windows Agora SDK DLLs aren't
shipped in the repo. **That change was kept local** and is not in this
commit — Cole's iOS/Mac/Win64 Agora setup is untouched. Don't add a Win64
disable on Cole's side.

---

## Workflow going forward

Tols plans to keep adding audio (vehicle engine loops, explosion SFX, UI
clicks, etc.) as **one focused commit per feature**, always rebased on
top of Cole's latest `DevCole`, then force-pushed to `origin/tolsdev`.

So the recipe stays the same each time:
```bash
git fetch origin
git checkout DevCole
git cherry-pick origin/tolsdev~N..origin/tolsdev    # N = how many new audio commits
```

Or to grab everything new on tolsdev that isn't on DevCole:
```bash
git cherry-pick DevCole..origin/tolsdev
```

If you only want some of them, `git log origin/tolsdev` to see the list
and cherry-pick by hash.

**Caveat:** Tols force-pushes `origin/tolsdev` each time. If Cole has a
local `tolsdev` checked out, it will diverge from origin — always
`git fetch` and cherry-pick from `origin/tolsdev`, never from a stale
local copy.

---

## Suggested next hooks (not yet wired)

If Cole wants more audio depth, the natural next places:
- **Vehicle engine loops** — `UAudioComponent` attached to each vehicle
  pawn, pitch by speed. Hook in vehicle pawn constructors + Tick.
- **Explosion SFX** — call `PHCAudio::PlayOneShotAtLocation` from
  `PHCNukeExplosion::BeginPlay`.
- **Weapon impact / hit markers** — already-hit code paths in `FireWeapon`
  trace results.
- **UI button clicks** — `PHCAudio::PlayOneShot2D` from `OnClicked`
  handlers in `PHCCityHUD`.
- **Wall-run / slide / parkour SFX** — separate one-shots in the parkour
  branches of `HandleJumpStarted` and the slide entry in `Tick`.
- **Land sound** — when `bHasBeenGrounded` flips false → true, play one
  of the `Foley_fs_1p_sneaker_concrete_land_*.uasset` samples.

---

## Questions / contact

Tols's branch is `origin/tolsdev`. Audio commit hash at time of writing:
see `git log --oneline origin/tolsdev -1`. If something looks off after
cherry-pick, check the commit message body — it has additional context.
