## English

- Fixed a path where hiding actual equipment could bypass vanilla same-slot equipment conflicts.
- Preserved Skyrim's own slot checks, quest restrictions and equipment replacement; no separate SFS equip/unequip rules or inventory changes.
- Applied the engine-input correction to native/DAV paths and preserved DAVE's existing conflict hook.
- Kept registered appearances, RaceMenu/BodyMorph, high heels, dye, strip/redress and IED behavior unchanged.

Validation: SE/AE-only build and 37 regression executables passed. Relevant engine code was inspected read-only on 1.5.97 and 1.6.1170. New-DLL gameplay replacement remains unverified.
Update: Keep settings, kits and saves. Requirements and optional patches are unchanged. Previously stacked equipment is not automatically cleaned up; unequip existing duplicates once.

## 한국어

- 실제 장비 숨김으로 바닐라의 동일 슬롯 착용 충돌 검사가 누락되던 경로를 수정했습니다.
- 슬롯 판정·퀘스트 제한·장비 교체는 바닐라가 처리하며, SFS의 별도 착용/해제 규칙이나 인벤토리 변경은 추가하지 않았습니다.
- 네이티브/DAV에 엔진 입력 보정을 적용하고 DAVE의 기존 착용 충돌 훅은 유지합니다.
- 등록 외형·RaceMenu/바디모프·하이힐·염색·탈의/재착의·IED의 기존 처리는 유지합니다.

검증: SE/AE 전용 빌드와 회귀 검사 37개를 통과했습니다. 1.5.97·1.6.1170의 엔진 코드를 읽기 전용으로 확인했으며, 새 DLL의 실제 인게임 교체 착용은 아직 미검증입니다.
업데이트: 설정·키트·세이브를 유지하세요. 선행 모드·선택 패치는 그대로입니다. 기존 중복 착용은 자동 정리하지 않으므로 해당 장비를 한 번 벗어 주세요.
