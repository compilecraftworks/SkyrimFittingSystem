# Skyrim Fitting System v1.7.0 SE-AE

## English

- Fixed skipped appearance/worn-mask hooks when an existing plugin had installed an E9 inline jump. This addresses the initialization path shown in the Skyrim 1.5.97 / SFS 1.6.6 report.
- Added SE/AE actor-selective inline handling for registered appearances, worn masks and actual armor hiding. Passive actors keep the previous route and machine state; active SFS actors use engine calls and SFS display rules.
- Preserved normal E8 provider routes and concrete visitors. IED evaluation uses the existing coalesced/load-canceled queue, only when IED is installed.
- Expanded production-stub tests to 2,600 SE/AE cases, including 128-cycle visibility changes and exact passive inline state preservation.
- All 28 regression executables passed in release and releasedbg. The SE/AE-only DLL builds as 1.7.0.0 and retains all 16 exports.

**Compatibility limit:** SFS takes display priority on active actors in this E9 fallback. Arbitrary behavior from the bypassed inline provider is not preserved for those actors. The reporter's exact mod combination has not been verified in game. Wet Function patch v1.2.0 is not established as the hook provider.

Keep settings, kits, saves and optional patches. Runtime support, dependencies, save format and API ABI are unchanged. Only SFSCore.dll changes in the runtime ZIP; the source ZIP's license notice also changes version.

## 한국어

- 기존 플러그인이 E9 점프를 설치한 경우 외형·착용 슬롯 훅을 건너뛰던 문제를 수정했습니다. Skyrim 1.5.97 / SFS 1.6.6 제보 로그의 초기화 실패 경로에 대응합니다.
- SE·AE의 등록 외형·착용 슬롯·실제 장비 숨김에 액터별 처리를 추가했습니다. 비활성 액터는 기존 경로·기계 상태를 유지하고, SFS 스키닝 활성 액터는 엔진 호출과 SFS 표시 규칙을 사용합니다.
- 기존 E8 연결과 구체 visitor 보호를 유지했습니다. IED 설치 시에만 기존 중복 병합·로드 취소 큐로 평가합니다.
- 128회 표시 전환과 비활성 인라인 경로의 스택·레지스터·플래그 보존을 포함해 프로덕션 훅 검사 2,600개 사례로 확장했습니다.
- 전체 회귀 검사 28개가 release·releasedbg에서 통과했습니다. SE/AE 전용 DLL의 1.7.0.0 빌드와 기존 내보내기 16개 유지를 확인했습니다.

**호환 한계:** E9 대체 경로의 활성 액터는 SFS 표시가 우선이며, 우회한 인라인 제공 모드의 모든 동작을 유지하는 방식은 아닙니다. 제보자의 실제 모드 조합으로 인게임 검증한 것은 아닙니다. Wet Function 패치 v1.2.0이 해당 훅의 원인이라고 확정하지 않습니다.

기존 설정·키트·세이브·선택형 패치를 유지하세요. 지원 버전·선행 모드·세이브 형식·API ABI는 변경하지 않았습니다. 런타임 ZIP에서는 SFSCore.dll만 변경되며, 소스 ZIP의 라이선스 안내 버전도 갱신했습니다.
