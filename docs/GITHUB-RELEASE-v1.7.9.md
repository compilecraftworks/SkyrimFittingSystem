## English

- Fixed a path where a RaceMenu height overwrite immediately after initial heel correction could be accepted as the stable height during a SexLab scene.
- Verify the applied automatic heel offset, current SexLab correction and node state before accepting synchronization. If they change during application, resolve the current appearance and scene state again.
- Apply the same verification and bounded retries to legacy and public RaceMenu routes. Persistent failure cannot create an endless retry loop; later normal events can retry.
- Preserve strip/redress behavior, including redress OFF, scene exit and actor unload cleanup. Actual equipment, BodyMorph and native/DAV/DAVE ownership are unchanged.
- Run the additional verification only during height synchronization. Stable-frame checks add no inventory queries, Papyrus calls or height recalculations.

Validation: SE/AE-exclusive DLL build, 45 regression executables and source-boundary checks passed. This covers source/host regression scenarios, not an in-game FPS measurement or reproduction of the reporter's exact animation setup.

Update: Only SFSCore.dll changes from v1.7.8. Keep existing settings, kits and saves. Requirements, supported game runtimes, scripts, helper ESL, public API and save schema are unchanged. Optional OStim / OStim Standalone patches remain v1.0.0; install only the matching one, with no reinstall required for existing patch users.

## 한국어

- SexLab 장면에서 최초 하이힐 보정 직후 RaceMenu가 높이를 덮어쓰면, 잘못된 높이가 정상값으로 고정될 수 있던 경로를 수정했습니다.
- 높이 적용 후 자동 하이힐 보정값·현재 SexLab 보정값·노드 상태를 확인하도록 개선했습니다. 적용 도중 값이 바뀌면 현재 외형과 장면 상태를 기준으로 다시 맞춥니다.
- 구형·신형 RaceMenu 경로에 동일한 확인과 제한 재시도를 적용했습니다. 실패가 계속되어도 무한 재시도하지 않으며, 다음 정상 이벤트에서는 다시 시도할 수 있습니다.
- 재착의 OFF를 포함한 탈의·재착의, 장면 종료와 액터 언로드 정리를 유지했습니다. 실제 장비·바디모프·native/DAV/DAVE의 제어 방식은 변경하지 않았습니다.
- 추가 확인은 높이 동기화 시에만 수행합니다. 안정 상태의 프레임 검사에는 인벤토리 조회·Papyrus 호출·높이 재계산이 추가되지 않습니다.

검증: SE/AE 전용 DLL 빌드, 회귀 실행 파일 45개와 소스 경계 검사를 통과했습니다. 소스/호스트 회귀 검사 결과이며, 인게임 FPS 실측이나 제보자의 정확한 애니메이션 구성 재현을 뜻하지 않습니다.

업데이트: v1.7.8 대비 런타임 파일은 SFSCore.dll만 변경됩니다. 기존 설정·키트·세이브를 유지하세요. 필수 선행 모드·지원 게임 버전·스크립트·보조 ESL·공개 API·저장 형식은 그대로입니다. 선택형 OStim·OStim Standalone 패치는 1.0.0을 유지하며, 환경에 맞는 하나만 설치합니다. 기존 패치 사용자는 재설치할 필요가 없습니다.
