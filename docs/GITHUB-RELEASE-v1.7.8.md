## English

- Addressed late RaceMenu full rescans that could reapply registered high-heel height during SexLab scenes, while respecting the existing heel-removal setting and actual footwear stripping state.
- Temporarily suppress native game and ENB depth of field while the SFS UI is open to reduce character/background blur. Normal behavior returns when the UI closes, and temporary ENB values are restored before preset saves.
- Reduced unnecessary checks while heel synchronization is pending or has failed, and reduced temporary allocations in stable scenes. Stable checks perform no additional inventory queries, Papyrus calls or height recalculations.
- Improved node-reference and queued-task cleanup during actor unload and save transitions so old work cannot block height monitoring in the next scene.
- Preserved existing BodyMorph, visibility, strip/redress, dye and native/DAV/DAVE paths. No new mandatory dependency; existing settings, kits, saves and optional patches remain compatible.

Validation: SE/AE-exclusive build, 45 regression executables and source-boundary checks passed. In-game FPS and the reporter's exact animation setup remain unverified. Independent ReShade blur is outside this adapter's scope.

Update: Only SFSCore.dll changes. Keep settings, kits and saves. Requirements, supported game runtimes, scripts, helper ESL and optional patches are unchanged. OStim / OStim Standalone patches remain v1.0.0; use only one matching patch, with no reinstall required for existing users.

## 한국어

- SexLab 장면 중 RaceMenu의 뒤늦은 전체 재계산으로 등록 하이힐 높이가 다시 적용되는 경로를 보완했습니다. 기존 하이힐 제거 옵션과 실제 발 장비 탈의 상태를 반영합니다.
- SFS UI를 열었을 때 캐릭터·배경이 흐려지는 현상을 줄이도록 기본 게임과 ENB의 심도 효과를 일시 억제합니다. UI를 닫으면 원래 동작으로 돌아가며, ENB 프리셋에 임시 값이 저장되지 않도록 처리했습니다.
- 하이힐 보정 대기·실패 상태의 불필요한 반복 검사와 안정된 장면의 임시 메모리 할당을 줄였습니다. 안정 상태에서는 추가 인벤토리 조회·Papyrus 호출·높이 재계산을 하지 않습니다.
- 액터 언로드·세이브 전환 시 노드 참조와 예약 작업 정리를 보완해, 이전 작업이 다음 장면의 높이 감시를 막지 않도록 했습니다.
- 기존 바디모프·숨김·탈의/재착의·염색·native/DAV/DAVE 경로를 유지합니다. 새 필수 선행 모드는 없으며 설정·키트·세이브와 기존 선택 패치는 그대로 사용할 수 있습니다.

검증: SE/AE 전용 빌드, 회귀 실행 파일 45개와 소스 경계 검사를 통과했습니다. 인게임 FPS와 제보자의 정확한 애니메이션 구성은 아직 미검증이며 독립적인 ReShade 블러는 제어 범위에 포함되지 않습니다.

업데이트: SFSCore.dll만 변경됩니다. 설정·키트·세이브를 유지하세요. 선행 모드·지원 게임 버전·스크립트·보조 ESL·선택 패치는 그대로입니다. OStim·OStim Standalone 패치는 1.0.0을 유지하며 맞는 하나만 사용하고, 기존 설치자는 다시 설치할 필요가 없습니다.
