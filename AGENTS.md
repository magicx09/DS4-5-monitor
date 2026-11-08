# DS4/5-monitor development

Read README.md, docs/BUILD.md, docs/TECHNICAL.md and docs/REFERENCES.md first. Reply in Simplified Chinese unless requested otherwise.

- Current release version is in VERSION. Published firmware is Pico 2 W / Chinese / prop16 only.
- Keep USB controller-compatible identity, flash addresses, pairing records and working DS4/DS5 audio behavior stable.
- Keep Opus and WDL as pinned Git submodules. Preserve all upstream copyright and license notices; see THIRD_PARTY_NOTICES.md.
- Use the Fusion Pixel 12px Proportional font and player-facing Chinese terminology. KEY0+KEY1: 2 seconds flips the display, 5 seconds reboots.
- Run host logic tests and both prop14/prop16 renderer tests for UI changes. Firmware must pass reserved-flash, RAM-residency and XIP checks.
- Keep local tools, caches and outputs in .local/ or a configured DS45_DEV_HOME, not tracked source.
- Do not claim host tests prove hardware behavior. No hardware flashing, publishing or commits without task authorization.
- Do not reset or clean working changes. This project began from a recovered upstream snapshot plus uncommitted handoff changes.
