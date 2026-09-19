# Skyrim Fitting System v1.6.9 SE-AE

## English

- Preserve conditional, NPC/global and locked appearances when Grid replaces player base appearances.
- Make condition-action drops atomic: rejected drops preserve source and target.
- Prevent old equipment/condition tasks and SOS callbacks from overwriting replacement work after loading.
- Restore only SFS-owned gameplay controls on UI close; respect existing and current stored-control locks.
- Validate kit write destinations while retaining nested/Unicode collections and normal overwrites.
- Reject conflicting condition IDs/counter overflow before publication; avoid refreshes during import staging.
- Restore missing Korean/Chinese tooltips and add localized path errors.
- 28 regression executables passed in release and releasedbg. No in-game test or measured SkyUI/GPU improvement claim.

Keep existing settings, kits, saves and optional patches. API ABI, dependencies and supported runtimes are unchanged.

## 한국어

- Grid가 플레이어 기본 외형을 교체할 때 조건부 카드·NPC/전역·잠금 외형을 보존합니다.
- 조건 액션 드롭을 함께 반영해 거부된 이동의 출발점·도착점 상태를 보존합니다.
- 로드 이전 장비/조건 작업과 SOS 콜백이 새 작업·결과를 덮어쓰지 못하게 수정했습니다.
- UI 종료 시 SFS가 잠근 조작만 복구하고 기존 잠금·현재 저장된 조작 잠금을 존중합니다.
- 정상 하위/한글 폴더·덮어쓰기를 유지하면서 키트 저장 경로의 외부 이탈을 방지합니다.
- 조건 ID 충돌·카운터 오버플로를 반영 전에 거부하고 가져오기 준비 중 조기 갱신을 제거했습니다.
- 한글·중문 설명 누락을 보완하고 잘못된 저장 경로 안내를 추가했습니다.
- release·releasedbg 모두 회귀 검사 28개 통과. 인게임·SkyUI 시간·GPU 메모리는 실측하지 않았습니다.

기존 설정·키트·세이브·선택형 패치를 유지하세요. API ABI·선행 모드·지원 게임 버전은 변경하지 않았습니다.

See the bundled bilingual release notes for detailed scope and limits.
