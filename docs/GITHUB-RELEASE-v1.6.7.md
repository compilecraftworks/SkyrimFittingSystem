# Skyrim Fitting System v1.6.7 SE-AE

## Changes

- Fixed registered high-heel `HH_OFFSET` selection being overwritten by another scene branch's zero or different automatic height. The selected registered heel now supplies RaceMenu's automatic position without adding a second persistent height offset.
- Restored actor-local height synchronization after late ordinary-gear or non-heel appearance attachments. This addresses an attachment-order path that could briefly apply the heel height and then clear it.
- Applied the correction to both legacy NiOverride Papyrus calls and the public NiTransform interface. Existing RaceMenu version routing and forward-compatible prefix policy are unchanged; no new version whitelist was added.
- Reduced repeated worn-equipment collection in final-outfit/body-keyword queries and registered-armor membership checks. Snapshots are reused only within one query, so later equipment and condition changes remain visible.
- Removed unused caller tracing from Papyrus WornHasKeyword preparation and deduplicated repeated function identities in other caller chains. Strip/redress, virtual-token, DD and P+ caller tracking remains intact, including trusted callers deeper in the stack.
- Added regression coverage for late attachments, conflicting/zero heights, strip/hide and redress/manual restoration, cleanup, deferred changes, callback failures, and request-local inventory snapshots.

## Compatibility and update notes

DAVE/DAV/native backend ownership, live BodyMorph, Fitting Dye, all stripping-link modes, manual visibility, IED integration and rendered-outfit API v1 are preserved. No real inventory equip/unequip operation, periodic actor scan, new dependency or temporary diagnostic hook was added. Keep existing settings and kits. The three optional compatibility patches are unchanged and are not bundled in the core archive.

## Verification and limits

The SE/AE-only 1.6.7.0 DLL builds successfully. All 23 regression executables and source checks passed; the RaceMenu history audit covers 19 interface-change snapshots, including public transform slots and legacy position-call signatures. These are source/ABI/logic checks, not certification of every distributed RaceMenu binary or in-game mod combination.

The runtime package changes only SFSCore.dll compared with v1.6.6. Scripts, the helper ESL, locales and other runtime assets are unchanged. No in-game tests were performed for this release. The inventory-path changes remove verified redundant work, but the reported SkyUI 3–4-second opening delay has not been measured or confirmed eliminated.

See [the technical audit](https://github.com/compilecraftworks/SkyrimFittingSystem/blob/v1.6.7/docs/HH-OFFSET-AND-INVENTORY-AUDIT-2026-09-19.md) for evidence and boundaries.

---

# Skyrim Fitting System v1.6.7 SE-AE

## 변경 사항

- 등록 하이힐의 `HH_OFFSET`이 다른 장면 노드의 0 또는 다른 높이 값에 덮이는 경로를 수정했습니다. 선택된 등록 하이힐의 높이를 RaceMenu 자동 위치에 반영하며, 별도의 영구 높이 값을 더해 이중 적용하지 않습니다.
- 실제 장비나 하이힐이 아닌 등록 외형이 뒤늦게 부착된 뒤에도 해당 액터의 높이를 다시 동기화하도록 보완했습니다. 높이가 잠깐 적용됐다가 지워질 수 있는 부착 순서 문제에 대응합니다.
- 구버전 NiOverride Papyrus 경로와 공개 NiTransform 인터페이스 양쪽에 같은 보정을 적용했습니다. 기존 RaceMenu 버전 분기와 상위 버전 호환 정책은 유지하며, 새 버전 허용 목록은 추가하지 않았습니다.
- 최종 외형·몸통 키워드 조회와 등록 외형 포함 여부 확인에서 중복 착용 장비 탐색을 줄였습니다. 한 번의 조회 안에서만 결과를 재사용하므로 이후 장비·조건 변경은 다시 반영됩니다.
- Papyrus WornHasKeyword 준비 과정의 사용하지 않는 호출자 추적을 제거하고, 나머지 호출 경로의 반복 함수 식별 계산을 줄였습니다. 탈의·재착의, 가상 토큰, DD·P+의 호출자 추적과 깊은 위치의 신뢰된 호출자 탐색은 유지합니다.
- 후속 부착, 높이 충돌·0 값, 탈의·숨김과 재착의·수동 표시 복원, 정리, 지연 변경, 콜백 실패와 단일 조회 범위의 장비 스냅샷 검사를 추가했습니다.

## 호환·업데이트 안내

DAVE/DAV/네이티브 처리 주체, 실시간 바디모프, 염색, 모든 탈의연동 모드, 수동 숨김, IED 연동과 최종 외형 API v1을 유지합니다. 실제 인벤토리 착용·해제 작업, 주기적 액터 탐색, 새 선행 모드나 임시 진단 훅은 추가하지 않았습니다. 기존 설정과 키트를 유지하십시오. 선택형 호환 패치 3종은 변경하지 않았으며 본체 ZIP에 포함하지 않습니다.

## 검증과 한계

SE/AE 전용 1.6.7.0 DLL 빌드가 성공했습니다. 회귀 테스트 23개와 소스 검사를 통과했고, RaceMenu 인터페이스 변경 지점 19곳에서 공개 변환 함수 슬롯과 구버전 위치 함수 형식을 확인했습니다. 이는 소스·ABI·로직 검사이며 모든 RaceMenu 배포본이나 인게임 모드 조합의 검증을 뜻하지 않습니다.

런타임 ZIP은 1.6.6 대비 SFSCore.dll만 변경됩니다. 스크립트·보조 ESL·언어 파일·기타 런타임 자료는 그대로입니다. 이번 릴리즈에서는 인게임 테스트를 하지 않았습니다. 인벤토리 관련 경로의 확인된 중복 처리를 줄였지만, 제보된 SkyUI 열기 지연 3~4초가 실제로 해소됐는지는 측정하지 않았습니다.

근거와 검증 범위는 [상세 점검 기록](https://github.com/compilecraftworks/SkyrimFittingSystem/blob/v1.6.7/docs/HH-OFFSET-AND-INVENTORY-AUDIT-2026-09-19.md)을 참고하십시오.
