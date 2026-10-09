SFS - OStim / OStim Standalone Strip Link Patch
Official release: 1.0.0, 2026-10-09

REQUIRES: SFS v1.7.6 or later and the matching OStim framework.
SFS v1.7.5 and earlier do not contain SFSOStimBridge natives.
OStim is optional for SFS itself; install these patches only if you use it.

Update SFS to v1.7.6 or later, then install exactly ONE patch:
- OStim: archived OStim / OUndress Extends Quest contract.
- Standalone: OStim Standalone / OUndress static-script contract.
Both contain OUndress.pex and SFSOStimBridge.pex. Do NOT enable both.
The selected patch must win OUndress.pex conflicts in MO2/Vortex.
Do not install compile-only imports, or replace OStim.dll.
Reload the save/restart the game after enabling or removing the patch:
OStim reads UsePapyrusUndressing once per game load.
If another mod replaces OUndress, its changes require a merged patch.

Behavior:
- Mod Settings strip link: OStim's current MCM slot mask and partial strip
  mask are applied to SFS's existing control-slot mapping. Registered
  appearances hide/restore through the existing SFS ownership tickets.
- Vanilla Slots strip link: the actual OStim equipment calls still drive
  SFS's existing actual-slot linking. No second appearance owner is created.
- Both Direct Edit bases retain their existing SFS mapping/protection rules.
- Actual worn gear and weapons remain managed by OStim. No SFS appearance
  or virtual token is returned to OStim's actual-equipment cache/inventory.
- NoStrip is read from the original registered armor's keywords. Wigs use
  slot 31 + HairTint evidence. Standalone follows its UndressWigs setting;
  the archived OStim version has no such setting and retains wig protection.
- Ordinary, partial and animated redress preserve OStim's original order,
  animation names, dress points, dead-actor checks and weapon handling.
- Cosmetic redress is scoped to actor/session. Other frameworks' tickets
  and manual appearance visibility changes are not cleared globally.
- Standalone's official start/stop interface handles continuing actors on
  scene migration without handing tokens to its native freeFast equip path.
  Removed NPCs retain their own delayed animated-redress session.
- No world/inventory polling, runtime instruction patches, new ESP or quest.
  Existing DAVE/DAV/native renderer and RaceMenu ABI adapters are unchanged.

Validation:
Both PEX variants and the SE/AE-exclusive SFS DLL compile. 42 regression
executables and existing source-boundary checks pass. New production-slice
tests cover NoStrip/wigs, custom mappings, multi-slot partial restore,
separate ownership, 128 scene transitions, reused thread ID 0, stale
appearance generations, rebased transaction IDs and nested caller evidence.
The complete public scene adapter is also tested for message/interface names,
absent OStim, inactive patches, missing actors, exact int32 thread IDs and
128 repeated transitions without duplicate listener registration.
This is NOT proof of in-game behavior for every OStim binary/add-on/version.
No in-game scenes, actual animation timing or save-load during a scene have
been exercised. See docs/OStim-Strip-Link-Implementation-2026-10-09.md for
the exact upstream snapshots and remaining runtime checks.

한국어 설치 안내:
SFS 1.7.6 이상이 필요합니다. 구형 OStim용 또는 Standalone용 패치 중 하나만
설치하세요. 1.7.5 이하의 SFSCore.dll에는 필요한 브리지 함수가 없습니다.
패치의 OUndress.pex가 충돌에서 이겨야 하며 적용 후 게임/세이브를 다시 로드하세요.
실제 장비 처리는 OStim이 그대로 담당하고 SFS는 등록 외형의 숨김/복원만 연결합니다.
모드 설정 연동·바닐라 슬롯 연동·직접 편집의 기존 설정과 보호 규칙을 유지합니다.
정식 1.0.0 배포본입니다. 컴파일·회귀·호출 규약 검증을 통과했으며,
실제 인게임 장면·애니메이션 타이밍·진행 중 세이브 로드는 별도 미검증입니다.

Sources / license:
Modified 2026-10-09 under GNU GPL v3; provided without warranty.
OUndress is derived from VersuchDrei/OStim and VersuchDrei/OStimNG.
Pinned source contracts (not floating downloads):
OStim: 4f56819a8281e7a0ea4e0c56569f82ead023532d
OStimNG: 3954683bbdfcd34b2f9157012ed0c446d6eefd1e
Each patch includes its modified PSC, bridge PSC and GPL license.
Full updated SFS corresponding source is provided as a separate source ZIP.
