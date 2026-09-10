# Skyrim Fitting System v1.6.2

## Changes

- Fixed GetInCell argument input returning to blank when CELL suggestions were missing. CELL collection now includes separately stored interiors and registered exterior/persistent cells.
- Form arguments accept EditorID, hexadecimal FormID (with or without 0x), and Plugin.esp|LocalFormID. The shared repair covers Location, Race, Faction, Keyword, Quest, Global and other supported form-parameter categories.
- Kept typed form input when suggestions are empty/loading or the field loses focus. Fixed stale ObjectRef selections restoring the previous value. Valid entries are checked against the expected form type on save; unresolved entries stay editable with a localized error.
- Distinguished placed Actor references from NPC ActorBase records. Preserved fixed-enum selection behavior and ActorValue name handling.
- Save valid form arguments using plugin/local identifiers when available, preserving the Player alias and compatibility with existing saved EditorIDs.
- Removed confirmed unreachable SFS-owned input handlers, alternate skinning helpers, unused condition/workbench helpers and the obsolete slot-catalog UI/state. Current slot editing, hidden-slot protection, shields and active feature paths remain.
- Made the already-enabled Virtual Token and Devious Devices production implementations unconditional; removed their unused disabled-build stubs.
- Added production form-resolver tests and real-ImGui dropdown tests. All 14 regression executables and additional source/options/package checks passed; the SE/AE DLL builds and retains all 13 public exports.

## Compatibility and installation

Existing SE/AE runtime support and prerequisites are unchanged; Skyrim 1.7.x and VR support are not added. The three optional compatibility patches are unchanged and are not bundled into the main runtime ZIP. Keep your existing SFS settings, presets and user kits when updating. The obsolete catalogShowAllSlots key is ignored and omitted on a later settings save.

## Known limitations

The original reporter's in-game outcome and every supported game/mod combination have not been verified. Automated tests are not an all-features in-game certification.

A separate audit found the existing string/variable-name argument (kChar) path routed through numeric handling. Its native string ownership/ABI contract still needs verification; this release does not claim to fix that path. The CELL/form-argument fixes above are separate from it.

---

# Skyrim Fitting System v1.6.2

## 변경 사항

- CELL 검색 결과가 누락되면 GetInCell 인수 입력이 빈칸으로 돌아가던 문제를 수정했습니다. 일반 배열 외에 별도로 저장된 실내 셀과 등록된 외부·영구 셀도 수집합니다.
- 폼 인수에서 EditorID, 16진수 FormID(0x 유무 모두), Plugin.esp|LocalFormID를 지원합니다. Location·Race·Faction·Keyword·Quest·Global 등 지원되는 공통 폼 인수 경로에도 적용했습니다.
- 목록이 비어 있거나 로딩 중이어도 입력을 유지하고, 포커스를 옮겨도 지우지 않습니다. ObjectRef 입력이 이전 선택값으로 돌아가던 문제도 수정했습니다. 저장 시 필요한 폼 유형을 확인하고, 잘못된 값은 입력을 보존한 채 오류로 안내합니다.
- 배치된 Actor 참조와 기본 NPC 데이터인 ActorBase를 분리했습니다. 성별·축 등의 고정 선택값과 ActorValue 이름 처리는 유지합니다.
- 저장 가능한 폼은 플러그인·로컬 ID로 기록해 로드 순서 변경에 대비합니다. Player 별칭과 기존 EditorID 기반 저장 데이터도 계속 읽습니다.
- 호출되지 않는 SFS 자체 입력 처리기·대체 스키닝 보조 경로·조건 및 작업대 보조 함수와 옛 슬롯 카탈로그 UI·상태를 정리했습니다. 현재 슬롯 편집·숨김 슬롯 보호·방패와 활성 기능 경로는 유지했습니다.
- 이미 활성화되어 있던 가상 토큰·Devious Devices 실제 구현을 항상 포함하도록 정리하고, 사용하지 않는 비활성 빌드용 빈 구현을 제거했습니다.
- 실제 폼 해석 코드와 실제 ImGui 입력 동작을 검사하는 테스트를 추가했습니다. 회귀 테스트 실행 파일 14개와 소스·옵션·패키지 부가 검사를 통과했고, SE/AE DLL 빌드 및 공개 함수 13개 유지도 확인했습니다.

## 호환성과 설치

기존 SE/AE 지원 범위와 선행 모드는 그대로이며, Skyrim 1.7.x·VR 지원은 추가하지 않았습니다. 선택 호환 패치 3종은 변경하지 않았고 본체 ZIP에도 포함하지 않습니다. 업데이트 시 기존 설정·프리셋·사용자 키트는 유지하세요. 옛 catalogShowAllSlots 설정은 무시하며 이후 설정 저장 시 생략합니다.

## 알려진 제한 사항

원 피드백 사용자의 인게임 해결 여부와 모든 지원 버전·모드 조합을 검증한 것은 아닙니다. 자동 테스트 통과가 모든 기능의 인게임 검증 완료를 뜻하지는 않습니다.

별도 점검에서 문자열·변수명 인수(kChar)가 숫자 처리 경로에 연결된 부분을 발견했습니다. 엔진의 문자열 수명·ABI 확인이 더 필요하며, 이번 릴리즈에서 이 경로까지 수정했다고 안내하지 않습니다. 위 CELL·폼 인수 수정과는 별개입니다.
