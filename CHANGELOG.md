## [2.0.0] - 2026-02-17

### Added
- Attention system with explicit reasons: low hunger, low happiness, poop, sickness, lights-on during sleep, tantrum.
- Care-mistake tracking with 15-minute unresolved trigger and 30-minute per-reason cooldown.
- Tantrum system persisted in save state:
    - Random trigger every 3-6 awake hours.
    - 10-minute response window.
    - Failure applies happiness penalty, care mistake, cooldown, and reschedule.
- Developer easter egg:
    - Hidden sequence on Home (`A, A, C, B, C, B, A` within 5s).
    - Debug overlay toggle from Status after unlock.
    - Overlay shows care mistakes, active alerts mask/count, tantrum timing, and raw stats.
- New persistent state fields for care-class scoring, sickness multipliers, medicine guarantee tracking, attention timers, and drift accumulators.

### Changed
- Core simulation now runs at 1-minute resolution using accumulators (no integer-truncation dead zones).
- Sleep is now RTC schedule-based by stage:
    - Baby 20:00-07:00, Child 21:00-07:00, Teen 22:00-08:00, Adult 23:00-08:00, Elder 21:30-07:30, Egg no sleep schedule.
- Action rebalance:
    - Meal: Hunger +30, Weight +2.
    - Snack: Hunger +12, Happiness +18, Weight +4.
    - Play: Happiness +18, Hunger -6.
    - Clean: Poop reset, Cleanliness +25 (capped).
    - Medicine: 85% cure chance; second dose within 30 minutes is guaranteed.
    - Train replaced by Scold (+discipline/-happiness, tantrum-aware behavior).
- Health/sickness model updated:
    - Health -12/hour when 2+ alerts active.
    - Health -20/hour when sick + any additional alert.
    - Health +4/hour under good-care recovery conditions.
    - Sickness chance rules and cap implemented, with care-class multipliers.
- Evolution timing updated to classic-like thresholds:
    - Egg 0-15m, Baby 15m-24h, Child 24h-72h, Teen 72h-144h, Adult 144h-288h, Elder 288h+.
    - Stage transitions apply care-class outcomes (Excellent/Good/Poor/Neglected).

### UI/UX
- Energy removed from player-facing status bars.
- Home and Status now show `!` when tantrum is active.
- Menu wording updated (`Sleep` -> `Light`, `Train` -> `Scold`).
- Helper text updated to match new controls/legend.

### Breaking Changes
- Save format changed (`STATE_VERSION=2`): previous save data is not load-compatible.
- Gameplay loop is materially rebalanced (sleep, alerts, sickness, evolution, actions).
