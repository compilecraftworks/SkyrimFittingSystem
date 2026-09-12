# Skyrim Fitting System v1.6.5 SE-AE

## Changes

- Added the read-only Rendered Outfit API v1 for external SKSE plugins. It reports SFS's final display decision for visible actual equipment and registered appearances, effective slots, and body-coverage flags. Actor-local change messages distinguish display changes, scene changes, availability, and save/load epochs. Managed-empty is distinct from unmanaged/unavailable.
- Added a built-in IED 1.7.4 BipedSlot condition bridge for the verified pre-629 and post-629 distributions. Covered equipment and node-override conditions now read SFS's final-visible ARMO slots 30-61, including hidden real equipment and shown/hidden registered appearances. No separate SFS patch is required for this bridge.
- Kept IED's original form/keyword predicates within BipedSlot conditions and node match bookkeeping. Inventory, actual equipment, presets, scripts, and IED files are not replaced. Weapon/quiver/race-sentinel slots, explicit skin queries, and other condition families remain owned by IED.
- When IED is absent, no bridge hooks, observer, polling, or IED evaluation tasks are installed. Unrecognized or modified IED binaries retain original IED behavior and core SFS rendering, with a diagnostic rather than guessed hook offsets.
- Preserved the previous Helgen/custom-skin visitor safety route. IED reevaluation is actor-local and deduplicated; tasks resolve actors afresh, skip missing/deleted/disabled/unloaded actors, and are canceled across save/load transitions. Scene-only notifications do not trigger an IED reevaluation loop.
- Added LT + horizontal RS character/camera rotation through the existing right-mouse rotation path, with analog deadzone, frame-time-based speed, and focus/device reset. Existing paused camera-only behavior is retained.
- Added a right-aligned rotation hint beside the main window's close button in English, Korean, and Simplified Chinese.
- Keyboard and gamepad Cancel now follow the current game MenuMode mapping, including remapped keys. Existing one-level cancellation is retained: close the active popup/editor first, or close the main SFS window when none is active.
- Set first-run character position to Left. Pause Game remains unchecked by default. Existing saved settings and presets are preserved.
- Bounded API publication to changed actors, coalesced duplicate requests, and provided immutable ID-only views for IED workers without a periodic whole-actor scan.

## Update and integration notes

Game/RaceMenu support, save/settings formats, BodyMorph ownership, DAVE/DAV/native refresh dispatch, and the three optional compatibility patches are unchanged. IED is optional. This bridge does not convert every IED equipped-form/keyword/type or inventory condition; presets must use the covered BipedSlot conditions to read SFS final-display state.

For API consumers, use `extras/SkyrimFittingSystemRenderedOutfitAPI.h` and the included API documentation. Resolve the already-loaded SFSCore.dll, query on the SKSE game-task thread, and invalidate consumer caches on epoch changes. Ready describes SFS's display decision, not completion of all parallel 3D attachment work or pixel visibility. The API does not itself install or update a consumer such as BCNG.

## Verification

SE/AE-only 1.6.5.0 build, all 20 fast regression targets, and source/package checks passed. Both installed IED 1.7.4 distributions passed actual helper and installer checks in an isolated process without changing installed DLLs. Tests include the prior 40 SE/AE custom-skin routes, BodyMorph/strip/backend regressions, API lifecycle, unload/delete/load cancellation, and ImGui title/close behavior.

Live Helgen/cell-transition gameplay, physical-controller input, and matched city-route frame-time/memory comparisons have not been completed for this release. These tests do not establish CTD-free behavior, measured FPS gains, or compatibility with every mod combination.

---

# Skyrim Fitting System v1.6.5 SE-AE

## 변경 사항

- 외부 SKSE 플러그인용 읽기 전용 Rendered Outfit API v1을 추가했습니다. 표시 중인 실제 장비·등록 외형, 유효 슬롯과 몸통 가림 정보를 SFS 최종 표시 판정 기준으로 제공합니다. 액터별 변경 알림은 표시·장면·가용 상태·세이브 전환을 구분하며, 관리 중인 빈 외형과 비관리·준비 안 됨을 구별합니다.
- 검증된 구형·신형 런타임용 IED 1.7.4 배포본에 BipedSlot 조건 연동을 내장했습니다. 지원되는 장비·노드 조건은 실제 장비 숨김과 등록 외형 표시·숨김을 포함한 최종 ARMO 슬롯 30~61을 참조합니다. 이 연동을 위한 별도 SFS 패치는 필요 없습니다.
- BipedSlot 안의 폼·키워드 판정과 노드 일치 기록은 IED 원래 함수를 유지합니다. 인벤토리·실제 장비·프리셋·스크립트·IED 파일을 교체하지 않습니다. 무기·화살통·종족 전용 슬롯, 명시적 스킨 조회와 다른 조건 계열은 IED가 그대로 관리합니다.
- IED 미설치 시 새 연동 훅·알림 구독·주기적 확인·IED 재평가 작업이 설치되지 않습니다. 식별되지 않거나 수정된 IED 바이너리는 주소를 추측해 연결하지 않고, 진단 로그와 함께 원래 IED 동작 및 SFS 본체 렌더링을 유지합니다.
- 과거 헬겐·외부 스키닝 방문자 충돌 방어 경로를 유지했습니다. IED 재평가는 액터별로 중복을 합치며, 실행 직전 액터를 다시 조회하고 삭제·비활성·3D 언로드 상태를 건너뜁니다. 세이브 전환 시 이전 요청을 취소하고, 장면 변경 알림만으로 재평가가 반복되지 않게 했습니다.
- LT를 누른 채 RS를 좌우로 움직여 기존 마우스 우클릭과 같은 경로로 캐릭터·카메라를 회전할 수 있습니다. 아날로그 데드존, 프레임 시간 기반 속도와 포커스·장치 해제 초기화를 적용했습니다. 일시정지 중 카메라만 회전하는 기존 동작은 유지합니다.
- 본창 X 닫기 버튼 왼쪽에 우측 정렬 회전 안내를 추가했습니다. 영문·한글·중국어 간체를 지원합니다.
- 키보드·게임패드 취소 입력은 재지정을 포함한 게임의 현재 MenuMode Cancel 설정을 따릅니다. 기존 단계별 취소를 유지하여 팝업·편집기가 있으면 먼저 닫고, 없으면 SFS 본창을 닫습니다.
- 최초 실행 시 캐릭터 위치 기본값을 왼쪽으로 변경했습니다. 게임 일시정지 기본값은 체크 해제를 유지하며, 기존 저장 설정과 프리셋은 보존합니다.
- API 발행 대상을 변경된 액터로 제한하고 중복 요청을 통합했습니다. IED 작업 스레드에는 불변 ID 정보만 제공하며, 전체 액터를 주기적으로 탐색하지 않습니다.

## 업데이트·연동 안내

게임·RaceMenu 지원 범위, 세이브·설정 형식, 바디모프 상태 관리, DAVE/DAV/네이티브 갱신 분기와 선택형 호환 패치 3종은 변경하지 않았습니다. IED는 선택 사항입니다. 모든 IED 장비 폼·키워드·종류·인벤토리 조건이 전환되는 것은 아니며, SFS 최종 표시 상태를 읽으려면 지원되는 BipedSlot 조건을 사용해야 합니다.

API 소비자는 `extras/SkyrimFittingSystemRenderedOutfitAPI.h`와 동봉된 API 문서를 참고하십시오. 이미 로드된 SFSCore.dll을 조회하고 SKSE 게임 작업 스레드에서 호출하며, 세이브 전환 알림에 소비자 캐시를 무효화해야 합니다. Ready는 SFS 표시 판정이며 모든 병렬 3D 부착 완료나 픽셀 가시성을 보증하지 않습니다. API 추가만으로 BCNG 등 외부 소비자가 설치·업데이트되지는 않습니다.

## 검증

SE/AE 전용 1.6.5.0 빌드, 빠른 회귀 테스트 20개와 소스·패키지 검사를 통과했습니다. 두 설치 환경의 IED 1.7.4 배포본은 설치 DLL을 변경하지 않는 격리 프로세스에서 실제 보조 함수와 연결 설치 검사를 통과했습니다. 기존 SE/AE 스키닝 분기 40개, 바디모프·탈의·백엔드, API 수명, 언로드·삭제·로드 취소와 ImGui 타이틀·닫기 동작을 검사했습니다.

이번 릴리즈의 실제 헬겐·셀 이동 플레이, 실물 게임패드 입력과 동일 도시 경로의 프레임 시간·메모리 비교는 완료하지 않았습니다. 자동 검사가 CTD 없음, 실측 FPS 개선 또는 모든 모드 조합의 호환성을 보증하지는 않습니다.
