# Skyrim Fitting System v1.6.6 SE-AE

## Changes

- Added `SkyrimFittingSystem_QueryRenderedOutfitOnGameTask` for external plugins querying SFS's final display state from actual SKSE AddTask callbacks. This avoids erroneous WrongThread rejection when a later serialized task batch runs on a different OS thread from the last SFS publication task, including when the provider is idle.
- Preserved the original query export and ABI v1 layouts, status values, signatures, and caller-owned buffers. Both query entries share argument, actor, readiness, 3D/root, suspension, dirty-state, epoch, revision, and scene validation; no stale-snapshot fallback was added.
- Added regression coverage for 128 actual-torso / registered-torso / managed-empty / no-torso-with-accessory transitions on migrated task threads, including effective slot masks and body-coverage flags. No visible torso is not assumed to mean zero visible items.
- Added checks for task-aware buffer bounds, validation before actor access, and repeated idle reads without forcing an extra SFS publication task.
- Audited temporary diagnostic and analysis code. No live probe or temporary diagnostic DLL was present in the SFS production path to remove. Required error logs, stripping recovery logic, regression tests, and backups were preserved. Added a guard against known analysis-probe code entering the production API/build target.

## Consumer/update notes

Consumers must resolve and prefer the new optional export to use the fix; the original export keeps its existing thread-ID guard for compatibility. The new entry does not schedule a task or validate task context on the caller's behalf. Call it only from an actual `SKSE::TaskInterface::AddTask` callback, never Present, an input callback, or an arbitrary worker. API version remains 1.

This is the SFS-side API fix handed off from BCNG integration testing, not a change to BCNG's ORefit coefficients, preset formulas, XML handling, or UI. BCNG is not bundled or updated by this SFS archive. Existing settings and presets should be kept.

Game/RaceMenu support, BodyMorph ownership, DAVE/DAV/native rendering, stripping-link modes, dye, IED BipedSlot integration, and the three optional compatibility-patch versions are unchanged. No new polling timer, periodic actor scan, or game-memory diagnostic hook was added.

## Verification

The SE/AE-only releasedbg DLL reports 1.6.6.0. All 20 regression targets and source checks passed, as did isolated helper/installer tests for both installed IED 1.7.4 distributions. Export inspection confirms 16 entries, including both query functions. The runtime archive changes only SFSCore.dll compared with v1.6.5; scripts, helper ESL, locales, and other runtime assets are unchanged.

The paired development hotfix was reported working by the user in TuLED. This numbered release has not received new in-game verification across all supported runtimes/mod combinations, and automated tests do not establish CTD-free behavior or measured FPS gains.

---

# Skyrim Fitting System v1.6.6 SE-AE

## 변경 사항

- 실제 SKSE AddTask 작업에서 SFS 최종 표시 상태를 조회하는 외부 플러그인을 위해 `SkyrimFittingSystem_QueryRenderedOutfitOnGameTask`를 추가했습니다. 후속 작업이 마지막 SFS 발행 작업과 다른 OS 스레드에서 실행될 때 정상 요청이 WrongThread로 거절되는 문제를 새 조회 경로에서 해결합니다. 공급자가 유휴 상태인 경우도 포함합니다.
- 기존 조회 함수와 ABI v1 구조체·상태값·호출 형식·호출자 소유 버퍼를 유지했습니다. 두 조회 함수는 인수·액터·준비 상태·3D/root·언로드·변경 대기·세이브 세대·리비전·장면 검증을 공유하며, 오래된 스냅샷으로 대체하는 경로는 추가하지 않았습니다.
- 실행 스레드가 바뀐 게임 작업에서 실제 몸통·등록 몸통·관리 중인 빈 외형·몸통 없이 액세서리만 남은 상태를 128회 전환하는 회귀 검사를 추가했습니다. 유효 슬롯과 몸통 가림 정보도 검사하며, 몸통이 안 보인다는 이유로 전체 항목이 비었다고 간주하지 않습니다.
- 새 조회 함수의 버퍼 범위, 액터 접근 전 인수 검증과 추가 발행 작업 없이 유휴 상태에서 반복 조회하는 검사를 보강했습니다.
- 임시 진단·분석 코드 유입 여부를 점검했습니다. SFS 제품 경로에는 제거할 임시 프로브나 진단 DLL이 없었습니다. 필요한 오류 로그·탈의 복구 코드·회귀 테스트·백업은 보존하고, 알려진 분석용 코드가 제품 API·빌드에 들어오지 않도록 검사를 추가했습니다.

## 외부 모드·업데이트 안내

이 수정을 사용하려면 외부 모드가 새 선택형 조회 함수를 우선 선택해야 합니다. 기존 함수의 스레드 ID 검사는 하위 호환을 위해 유지합니다. 새 함수가 작업을 대신 예약하거나 호출자의 게임 작업 실행 여부를 자동 확인하지는 않습니다. 반드시 실제 `SKSE::TaskInterface::AddTask` 콜백 안에서 호출하고, Present·입력 콜백·임의 작업 스레드에서는 호출하지 않아야 합니다. API 버전은 1을 유지합니다.

BCNG 연동 검사에서 인계받은 SFS 측 API 수정이며, BCNG의 ORefit 계수·프리셋 계산·XML 처리·UI 변경이 아닙니다. SFS 압축파일에 BCNG를 포함하거나 함께 업데이트하지 않습니다. 기존 설정과 프리셋을 유지하십시오.

게임·RaceMenu 지원 범위, 바디모프 상태 관리, DAVE/DAV/네이티브 렌더링, 탈의연동 모드, 염색, IED BipedSlot 연동과 선택형 호환 패치 3종의 버전은 변경하지 않았습니다. 새 폴링 타이머·주기적 액터 탐색·게임 메모리 진단 훅도 추가하지 않았습니다.

## 검증

SE/AE 전용 releasedbg DLL 버전은 1.6.6.0입니다. 회귀 테스트 20개와 소스 검사를 통과했으며, 두 설치 환경의 IED 1.7.4 배포본에 대한 격리된 보조 함수·연결 설치 검사도 통과했습니다. 공개 함수는 기존·신규 조회를 포함한 16개입니다. 릴리즈 압축파일은 1.6.5 대비 SFSCore.dll만 변경되며 스크립트·보조 ESL·언어 파일·기타 런타임 자료는 그대로입니다.

개발 중 적용한 연동 핫픽스는 사용자가 툴레드에서 정상 동작을 확인했습니다. 이번 번호의 릴리즈를 모든 지원 런타임·모드 조합에서 새로 인게임 검증한 것은 아니며, 자동 검사가 CTD 없음이나 실측 FPS 개선을 보증하지는 않습니다.
