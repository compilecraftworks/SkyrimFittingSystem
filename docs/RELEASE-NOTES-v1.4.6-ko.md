# Skyrim Fitting System v1.4.6

## Grid Inventory Costume 외형 보존

- Grid Inventory v1.4.1 이상의 Costume 연동은 이제 플레이어가 실제로 소유한
  방어구 FormID가 하나 이상 들어 있는 Costume일 때만 플레이어 SFS 등록 외형을
  갱신합니다.
- Costume 해제, 빈 Costume, 사용할 수 있는 방어구 FormID가 없는 구성은 저장된
  SFS 등록 외형을 지우거나 바꾸지 않습니다.
- 시작·세이브 로드·리버트 뒤 Grid가 보내는 첫 복원 상태는 계속 무시합니다. 이후
  사용자가 새로 바꾼 비어 있지 않은 Costume만 SFS를 갱신합니다.
- `SFSCore.dll`은 계속 Grid Inventory의 공개 SKSE 메시지만 사용합니다.
  `GridInventory.dll`, Grid 로드아웃·세이브·실제 장비를 교체·로드·후킹·수정하지
  않습니다.

## GPL-3 대응 소스

- v1.4.6부터 소스 다운로드에 이 릴리스와 대응하는 선호 소스 형태, 실제
  `xmake.lua`·lock 파일, 검증한 CommonLibSSE-NG 정확한 소스 리비전, 테스트 소스,
  제3자 라이선스 고지를 포함합니다.
- 런타임 ZIP에도 `LICENSE`와 `THIRD_PARTY_NOTICES.md`를 포함합니다.

## 회귀 방지 범위

- 모드 설정 슬롯 연동, 바닐라 슬롯 자동 연동, 직접 편집, 외부 탈의·재착의,
  DAVE/DAV/Skyrim native 표시는 기존 액터별 상태와 처리 경로를 유지합니다.
- v1.4.6 `releasedbg` 빌드와 내장 키트 생성기 로직 회귀 테스트를 통과했습니다.
