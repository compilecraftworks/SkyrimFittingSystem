## English

Refreshed on 2026-10-25. If you downloaded the earlier v1.8.0 package, download it again to receive these follow-up fixes. Optional patches are unchanged.

- Fixed SDTA-only registered high heels being omitted from height selection and SexLab scene correction. HH_OFFSET and SDTA now share metadata selection while preserving SDTA X/Y values.
- Fixed high-heel synchronization retries ending before a registered appearance finished attaching. Legacy and public RaceMenu routes now retry on later frames within the existing retry limit.
- Fixed missed BodyMorph updates on registered appearances that attach later. Current morph values are reapplied when the nodes become ready.
- Fixed saved dye restoration giving up before the actor's 3D became ready.
- Fixed temporary SFS transform values remaining after cancelled legacy RaceMenu synchronization. Pending work is cleared on actor unload and save reset.

Validation: SE/AE-exclusive DLL build, 45 regression executables and source-boundary checks passed. Coverage includes HH_OFFSET/SDTA, SexLab option ON/OFF, strip/redress, BodyMorph, native/DAV/DAVE paths, 128 unload/reload cycles and idle-work checks. These are production-source tests with recording providers, not confirmation from the reporter's game.

Update: Only SFSCore.dll changes from v1.7.9. Keep existing settings, kits and saves. Requirements, supported game runtimes, scripts, helper ESL, public API and save schema are unchanged. Optional OStim / OStim Standalone patches remain v1.0.0; existing patch users do not need to reinstall them.

## 한국어

2026-10-25 수정본으로 갱신했습니다. 이전 1.8.0 패키지를 받았다면 다시 내려받아 추가 수정 사항을 적용하세요. 선택형 패치는 그대로입니다.

- SDTA 형식만 사용하는 등록 하이힐이 높이 계산과 SexLab 장면 보정에서 누락되던 문제를 수정했습니다. HH_OFFSET·SDTA 판독을 통합하고 SDTA의 X/Y 값은 보존합니다.
- 등록 외형이 부착되기 전에 하이힐 동기화 재시도가 끝나던 문제를 수정했습니다. 구형·신형 RaceMenu 모두 기존 재시도 횟수 안에서 다음 프레임에 다시 확인합니다.
- 늦게 부착되는 등록 외형에서 바디모프 갱신이 누락되던 문제를 수정했습니다. 노드가 준비되면 현재 모프값을 다시 반영합니다.
- 액터의 3D가 준비되기 전에 저장된 염색의 복원 재시도가 끝나던 문제를 수정했습니다.
- 구형 RaceMenu 동기화가 취소될 때 SFS 임시 변환값이 남던 문제를 수정했습니다. 액터 언로드와 세이브 초기화 시 대기 작업도 정리합니다.

검증: SE/AE 전용 DLL 빌드, 회귀 실행 파일 45개와 소스 경계 검사를 통과했습니다. HH_OFFSET·SDTA, SexLab 옵션 ON/OFF, 탈의·재착의, 바디모프, native/DAV/DAVE 경로, 128회 언로드·재로드와 유휴 작업 검사를 포함합니다. 실제 소스와 모의 제공자를 이용한 검사이며 제보자의 인게임 해결 확인은 아닙니다.

업데이트: v1.7.9 대비 런타임 파일은 SFSCore.dll만 변경됩니다. 기존 설정·키트·세이브를 유지하세요. 필수 선행 모드·지원 게임 버전·스크립트·보조 ESL·공개 API·저장 형식은 그대로입니다. 선택형 OStim·OStim Standalone 패치는 1.0.0을 유지하며, 기존 패치 사용자는 재설치할 필요가 없습니다.
