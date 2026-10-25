## English

- Fixed SDTA-only registered high heels being omitted from height selection and SexLab scene correction.
- Unified HH_OFFSET and SDTA handling, including mixed-source priority, child-node metadata, and valid zero or negative offsets. SDTA X/Y values are preserved while scene compensation adjusts height.
- Preserved existing strip/redress behavior, including redress OFF and manual visibility controls, across native/DAV/DAVE and legacy/public RaceMenu routes. Actual equipment and BodyMorph ownership are unchanged.
- Removed the redundant HH_OFFSET-only reader and reused the existing synchronization and cleanup paths. Stable-frame checks add no metadata parsing, inventory queries or Papyrus calls.

Validation: SE/AE-exclusive DLL build, 45 regression executables and source-boundary checks passed. The SDTA gap is verified in production-source tests; the reporter's exact footwear format is unknown, so this does not confirm resolution of that particular in-game report.

Update: Only SFSCore.dll changes from v1.7.9. Keep existing settings, kits and saves. Requirements, supported game runtimes, scripts, helper ESL, public API and save schema are unchanged. Optional OStim / OStim Standalone patches remain v1.0.0; existing patch users do not need to reinstall them.

## 한국어

- SDTA 형식만 사용하는 등록 하이힐이 높이 계산과 SexLab 장면 보정에서 누락되던 문제를 수정했습니다.
- HH_OFFSET·SDTA 처리를 통합해 혼합 형식의 우선순위, 하위 노드의 높이 정보, 0·음수 보정값을 일관되게 처리합니다. 장면 높이를 보정할 때 SDTA의 X/Y 값은 보존합니다.
- native/DAV/DAVE와 구형·신형 RaceMenu 경로에서 기존 탈의·재착의, 재착의 OFF, 수동 숨김·표시 동작을 유지합니다. 실제 장비와 바디모프의 제어 방식은 변경하지 않았습니다.
- HH_OFFSET 전용 중복 판독 코드를 정리하고 기존 동기화·정리 경로를 재사용합니다. 안정 상태의 프레임 검사에는 메타데이터 해석·인벤토리 조회·Papyrus 호출이 추가되지 않습니다.

검증: SE/AE 전용 DLL 빌드, 회귀 실행 파일 45개와 소스 경계 검사를 통과했습니다. SDTA 누락은 실제 소스 회귀 검사로 확인했지만, 제보자의 정확한 신발 형식은 알려지지 않아 해당 인게임 제보의 해결을 확정하는 것은 아닙니다.

업데이트: v1.7.9 대비 런타임 파일은 SFSCore.dll만 변경됩니다. 기존 설정·키트·세이브를 유지하세요. 필수 선행 모드·지원 게임 버전·스크립트·보조 ESL·공개 API·저장 형식은 그대로입니다. 선택형 OStim·OStim Standalone 패치는 1.0.0을 유지하며, 기존 패치 사용자는 재설치할 필요가 없습니다.
