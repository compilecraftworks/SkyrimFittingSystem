# Skyrim Fitting System v1.6.4 SE-AE

## Changes

- Reduced repeated stripping-link work by collecting each actor's active appearances once per refresh and reusing the result across all 32 biped slots. Existing refresh timing and slot/identity rules are preserved.
- Skipped unnecessary body-keyword outfit collection for actors without an active SFS display override, and reduced unrelated dye render-pass bookkeeping while preserving nested-pass isolation.
- Fixed individual/global appearance show controls after external stripping without redress, including SexLab redress OFF. Appearances remain hidden until a manual action or valid recovery; actual redress still uses the existing automatic recovery. SFS does not equip real gear for this fix.
- Restricted manual/delete stripping-ticket invalidation to the selected actor, preserving other actors' same-slot states.
- Form-based condition Argument 1 now displays EditorID first, with the original identifier as fallback. Actor/reference labels retain a secondary FormID. Viewing or accepting an unchanged label does not rewrite the stored reference; Argument 2 and non-form values are unchanged.
- Added regression coverage for slot-result equivalence, dye pass isolation, repeated strip/show cycles, actor isolation, and EditorID input/selection. All 17 fast regression executables and production-source checks passed.

## Update notes

No changes to game/RaceMenu support, save/settings formats, BodyMorph ownership, DAVE/DAV/native refresh dispatch, or the three optional compatibility patches. Keep existing settings and presets.

The performance changes address redundant work identified during the city-stutter investigation. FPS gains and resolution in every reported environment have not been verified in-game. Automated tests do not replace live gameplay validation.

---

## 변경 사항

- 탈의연동 갱신 시 액터의 활성 외형을 한 번 계산해 32개 바디 슬롯 조회에 재사용하도록 중복 연산을 줄였습니다. 기존 갱신 시점과 슬롯·외형 식별 규칙은 유지합니다.
- SFS 외형 변경이 활성화되지 않은 액터의 불필요한 몸통 키워드용 외형 수집을 생략하고, 중첩 염색 격리를 유지하면서 무관한 렌더 패스의 기록 작업을 줄였습니다.
- 섹랩 재착의 OFF 등 외부 탈의 후 재착의하지 않은 상황에서 개별·전체 외형 표시 버튼이 동작하지 않던 문제를 수정했습니다. 수동 조작이나 유효한 복구 전까지 외형은 숨겨진 채 유지하며, 실제 재착의 시에는 기존 자동 복구가 유지됩니다. 이 수정으로 SFS가 실제 장비를 입히지는 않습니다.
- 수동 조작·삭제에 따른 탈의 기록 해제를 대상 액터로 한정하여 다른 액터의 같은 슬롯 상태를 보존합니다.
- 폼을 참조하는 조건의 인수1에 EditorID를 우선 표시하고, 없으면 기존 식별값을 표시합니다. 액터·참조 항목은 FormID도 보조 표시합니다. 단순 열람·동일 값 확정으로 저장된 참조를 바꾸지 않으며, 인수2와 폼이 아닌 값은 그대로 유지합니다.
- 슬롯 결과 동등성, 염색 패스 격리, 반복 탈의·표시, 액터 간 격리와 EditorID 입력·선택 회귀 검사를 추가했습니다. 빠른 회귀 테스트 17개와 실제 코드 연결 검사를 모두 통과했습니다.

## 업데이트 안내

게임·RaceMenu 지원 범위, 세이브·설정 형식, 바디모프 상태 관리, DAVE/DAV/네이티브 갱신 분기와 선택형 호환 패치 3종은 변경하지 않았습니다. 기존 설정과 프리셋을 유지하세요.

성능 변경은 도시 버벅임 조사에서 발견한 중복 연산을 줄인 것입니다. 실제 FPS 개선과 모든 제보 환경에서의 해결을 인게임에서 확인한 것은 아니며, 자동 검사가 실제 플레이 검증을 대신하지는 않습니다.
