# Skyrim Fitting System v1.7.2 SE-AE

## English

- Fixed premature loss of registered high-heel `HH_OFFSET` records before attachment or during temporary reattachment. Retained bounded retries and unload cleanup without equipping actual gear or overwriting other mods' named height transforms.
- Reduced repeated worn-equipment collection, visible-outfit list construction, condition UI-data copying and Papyrus inspection-cache collision rescans. Final-rendered nudity, strip/redress decisions, first-call installation, late binding and P+ recovery remain active; no stale cross-query display cache is substituted.
- Gear, Outfits and Kits now save **Favorites Only independently**, with one-time migration of the previous shared preference.
- Added list-only body-family filters to the catalogs and Kit Generator ESP/results lists. Filters preserve existing previews and registrations and do not restrict application. Catalog filter changes focus the first sorted result without automatically applying or previewing it.
- Added generated-result checkboxes: **only checked kits are created, merged or deleted**. Ctrl/Shift highlight rows for batch checking. Select All checks visible results; Clear All includes hidden checks. Checkbox clicks remain separate from double-click candidate navigation.
- Added Ctrl/Shift multiselection and an optional worn-model plugin filter to ESP selection. Hidden selections are preserved. Red grouping warnings now have compact, wrapping explanations.
- Grouped related Community Kit Hub color, length and numbered variants as candidates under one kit while preserving each composition and plugin boundary. Improved reference-based and generic outfit grouping, accessory preservation and coordinated color/style candidates within the existing 256-profile limit.
- Integrated the cumulative Community Kit Grouping Custom Patch 4 and its reference tables into the full mod. No workbook, photos, Modex or Python is required at runtime.
- Prevented SFS list-navigation keys from also reaching downstream mod hotkey event handlers. New key presses pass normally after closing SFS; mods polling keys directly or handling them before SFS still need an in-game check.

**Updating:** Install the full package. Disable the old separate Community Kit Grouping Custom Patches 1–4 so they cannot overwrite the new DLL/locales. Preserve settings, saved kits and saves. Rescan ESPs for the new grouping; saved kits are not rewritten. Wet Function, Dynamic Footprints and Modesty compatibility patches are unchanged. Dependencies, game-version support and API ABI are unchanged.

**Verification:** The SE/AE-only releasedbg build, 35 native regression executables, source checks and 11 Python tests passed. These are not in-game timing/visual tests. The reporter's approximately four-second SkyUI delay and exact high-heel setup remain unverified in game; this release fixes confirmed code defects and redundant work, not a claim that every reported symptom is resolved. Body-family labels are metadata-based, not a guarantee of mesh fit.

## 한국어

- 등록 하이힐의 부착 전·일시적 재부착 중 `HH_OFFSET` 기록을 성급하게 삭제하던 문제를 수정했습니다. 제한된 재시도와 언로드 정리를 유지하며 실제 장비 착용이나 다른 모드의 고유 높이 보정은 변경하지 않습니다.
- 착용 장비 수집·표시 외형 목록 생성·조건 UI 데이터 복사·스크립트 연결 캐시 충돌의 중복 작업을 줄였습니다. 최종 렌더 기반 누드 판정, 탈의·재착의, 최초 연결·늦은 등록·P+ 복구는 유지하고 오래된 표시 결과 캐시로 대체하지 않습니다.
- 장비·의상·키트의 **즐겨찾기만 설정을 각각 독립 저장**합니다. 기존 공통 설정은 한 번 이어받습니다.
- 카탈로그와 키트 생성기 ESP/결과 목록에 체형 필터를 추가했습니다. 목록만 필터링하며 기존 미리보기·등록 외형을 유지하고 적용을 제한하지 않습니다. 카탈로그 필터 변경 후 첫 항목으로 포커스만 이동하고 자동 적용·미리보기는 실행하지 않습니다.
- 생성 결과에 체크박스를 추가하여 **체크한 키트만 생성·합치기·삭제**합니다. Ctrl/Shift로 행을 선택한 뒤 일괄 체크할 수 있습니다. 전체 선택은 보이는 결과, 선택 해제는 숨겨진 체크까지 적용합니다. 체크박스와 더블클릭 후보 진입은 구분합니다.
- ESP 목록에 Ctrl/Shift 다중 선택과 착용 모델이 있는 플러그인 필터를 추가했습니다. 숨겨진 선택은 유지하며 빨간색 그룹화 경고 설명에 폭 제한과 줄바꿈을 적용했습니다.
- 커뮤니티 허브의 같은 의상 색상·길이·번호 변형을 한 키트의 후보로 묶되 원래 구성과 플러그인 경계를 보존합니다. 참조 기반·범용 묶음, 부속품 보존, 색상·스타일 후보 생성을 기존 256개 한도 안에서 개선했습니다.
- 커뮤니티 키트 커스텀 패치 4의 누적 기능과 참조표를 본체에 통합했습니다. 실행 시 Excel·사진·Modex·Python이 필요하지 않습니다.
- SFS 목록 탐색 키가 다른 모드의 후속 단축키 이벤트로 중복 전달되지 않도록 수정했습니다. 창을 닫은 후 새 입력은 정상 전달합니다. 키 상태를 직접 조회하거나 SFS보다 먼저 처리하는 모드는 인게임 확인이 필요합니다.

**업데이트:** 본체 전체 패키지를 설치하세요. 이전 별도 커뮤니티 키트 커스텀 패치 1~4가 새 DLL·번역을 덮어쓰지 않도록 비활성화하세요. 설정·저장 키트·세이브는 유지합니다. 새 묶음은 ESP 재분석 시 반영되며 저장된 키트를 자동 변경하지 않습니다. Wet Function·Dynamic Footprints·Modesty 호환 패치, 선행 모드·게임 지원 범위·API ABI는 변경하지 않았습니다.

**검증:** SE/AE 전용 releasedbg 빌드, 자동 회귀 검사 35개와 소스 검사, Python 검사 11개가 통과했습니다. 인게임 시간·화면 검증은 아닙니다. 제보자의 약 4초 SkyUI 지연과 정확한 하이힐 환경은 인게임 미확인이므로 모든 증상의 완전 해결로 단정하지 않습니다. 체형 분류 역시 메타데이터 기반으로 실제 메시 호환성을 보장하지 않습니다.
