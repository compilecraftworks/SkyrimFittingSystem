# Skyrim Fitting System v1.3.1 Development Notes

Version 1.3.1 was an interim development version and was not published as a final release. Version 1.3.0 was the previous public release. All work listed below, together with later stabilization, is included in v1.4.0. Use `RELEASE-NOTES-v1.4.0.md` as the authoritative release document.

## Work Merged into v1.4.0

- Removed the dedicated Helmet Toggle 2 Papyrus bridge, transient co-save state, workbench lock/banner, and optional patch source.
- Moved head, hair, and circlet appearances to the ordinary manual visibility, condition, and actual-equipment strip-link rules.
- Made automatic equipment suppression reach native, DAV, and DAVE rendered appearances as well as the UI state.
- Added slot 60 to the default protected special-effect set, now slots 50, 51, 60, and 61.
- Synchronizes only the affected actor's registered appearances after RaceMenu BodyMorph updates from systems including OBody, FHU, and SGO.
- Uses the common DAVE, DAV, and native paths without a global actor scan, periodic polling, or per-mod event lists.
- Applies the same final SOS/TNG slot-32 and slot-49 decision to actual gear and registered appearances.
- Preserves explicit SOS MCM choices and ESP/KID source keywords.

Install v1.4.0 rather than any v1.3.1 development file to use this work.
