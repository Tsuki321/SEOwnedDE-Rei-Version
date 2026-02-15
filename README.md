# SEOwnedDE — x64 port and updates

SEOwnedDE is an x64 port and continuation of the original SEOwnedDE project. This repository tracks the porting work, compatibility fixes, and ongoing maintenance (sigs, offsets, hooks and feature fixes).

Based on [SEOwnedDE public](https://github.com/spook953/SEOwnedDE-public) and contributions from various forks.

---

## 🚀 What's new (summary)
- Major refactors and optimizations to features and utilities (Feb 2026).
- Ongoing compatibility fixes for recent game updates and signature adjustments.
- Stability fixes (crash fixes, hook hardening) and build improvements (x64 migration finished, non-AVX2 build available).
- Aimbot/signature fixes, interface fixes and selective feature re-enablement.

> This README highlights recent changes and provides a concise changelog for quick tracking. For the full history, see the Git commit log.

---

## 🧾 Recent changelog (selected entries)
- 2026-02-15 — Refactor and optimize various features and utilities (`48444b6`)
- 2025-10-28 — Fix for latest game update (`3a48b0d`)
- 2025-10-24 — Fix for latest game update (`8812b8d`)
- 2025-05-13 — Game update adjustments (`4cd2312`)
- 2025-02-19 — Fix crash while in jump while swimming (`35306b3`)
- 2024-10-11 — Update signatures and offsets (`5d49401`)
- 2024-08-02 — Finish remaining x64 porting; build targets updated (`6370482`, `89c95ee`)
- 2024-02-21 — Fixed projectile/melee aimbot signatures and related fixes (`09de254`, `032fc57`)
- 2024-02-14 — Multiple interface/signature fixes and re-enabled features (`9ad5b0b`, `ee36dfd`)
- 2024-01-07 — Hook cleanup and movement/render adjustments (`e58f77e`, `08e5681`)

(See `git log` for the full commit history and more granular entries.)

---

## 🔧 Build & usage notes
- Open `SEOwnedDE/SEOwnedDE.sln` in Visual Studio and set the platform to `x64`.
- Available build configurations include `ReleaseAVX2` and standard `Release` (non-AVX2 builds included).
- After building, the resulting `SEOwnedDE.dll` is placed per project output settings (see `build/` for recipes).

---

## ⚠️ Important notes / breaking changes
- The project has been migrated to **x64**; use 64-bit toolchain/targets.
- If you depend on specific offsets/signatures, re-run signature updates — many were changed to match recent game updates.

---

## 🤝 Contributing & reporting
- Please open issues for bugs or regressions and include reproduction steps and game version.
- Pull requests are welcome — follow the existing code style and include tests where appropriate.

---

## 🙏 Credits
- Original: SEOwnedDE public
- Forks & contributors: LNX and community contributors listed in the Git history

---

_Last updated: generated from recent commits (see `git log`)_
