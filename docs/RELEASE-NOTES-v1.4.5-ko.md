# Skyrim Fitting System v1.4.5

## Grid Inventory Costume 내장 연동

- Grid Inventory v1.4.1 이상의 공개 Costume 상태 SKSE 메시지를 SFSCore가
  직접 받도록 추가했습니다. Grid Inventory에서 Costume을 착용·교체·해제할 때
  Grid가 보낸 현재 방어구 FormID만 이용해 플레이어의 등록 외형을 갱신합니다.
- 시작·세이브 로드·리버트 뒤 Grid가 보내는 첫 복원 상태는 의도적으로
  무시합니다. SFS는 저장된 등록 외형을 유지하고, 사용자가 Grid Inventory에서
  Costume을 다시 바꾼 뒤부터 연동합니다.
- 연동은 `SFSCore.dll` 안에 포함됩니다. 콜백 안에서는 빌려온 Grid 데이터를
  즉시 복사하고, SFS의 기존 안전 처리 지점에서 최신 상태를 반영합니다.
  `GridInventory.dll`을 교체·로드·후킹·import하지 않습니다.
- Costume 렌더링·로드아웃·세이브·실제 장비는 계속 Grid Inventory가 관리합니다.
  SFS는 실제 장비, 전역/NPC 행, 조건 정의, Grid 데이터를 바꾸지 않습니다.
- 업데이트할 때 v1.4.4의 **SFS Grid Inventory Costume Compatibility Patch**는
  제거하십시오. Grid Inventory v1.4.1 이상은 별도 SFS 패치가 필요 없습니다.

## Dynamic Footprints 호환 패치

- 별도 Dynamic Footprints 패치를 v1.4.5로 갱신했습니다. 로컬 트램폴린을
  검증한 Dynamic Footprints의 정확한 호출 지점 기준으로 할당해, 일부 DLL
  로드 배치에서 발생하던 CommonLib `displacement is out of range` 경계 오류를
  막습니다. 검증 DLL·바이트 서명·읽기 전용 Feet 37 경계는 그대로입니다.

## 회귀 방지 범위

- 모드 설정 슬롯 연동, 바닐라 슬롯 자동 연동, 직접 편집, 외부 탈의·재착의,
  DAVE/DAV/Skyrim native 표시는 기존의 액터별 상태와 처리 경로를 유지합니다.
- 전체 `releasedbg` 빌드와 Kit Generator 로직 회귀 테스트를 통과했습니다.
