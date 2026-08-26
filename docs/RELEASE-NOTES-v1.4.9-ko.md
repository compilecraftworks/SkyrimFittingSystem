# Skyrim Fitting System v1.4.9

## 런타임 레이아웃 안전성

- 공통 컴파일 값에 기대던 훅 위치를 Skyrim SE 1.5.97과 Skyrim AE의
  명시적이고 검증된 런타임 프로필로 분리했습니다.
- UI·입력 및 native 방어구 훅은 실행 중인 Skyrim 버전의 프로필에서
  위치를 선택하고 명령 형식을 확인한 뒤 설치합니다. 검증되지 않은
  런타임에서는 잘못된 주소에 훅을 설치하지 않고 안전하게 비활성화합니다.
- Papyrus VM/native 함수 슬롯 계약을 한곳으로 통합하고, 최초 지원
  SE/AE 버전·AE 1.6.629 경계·현재 Steam/GOG 버전·미지원/VR 거부를
  확인하는 경계 테스트를 추가했습니다.

## Immersive Equipment Displays 호환

- IED custom-skin 호환 경계를 실제 SE·AE `VisitWornItems` 호출 지점에
  연결했습니다.
- 해당 지점을 IED가 먼저 훅한 경우 SFS의 필터 visitor를 IED의 구체
  visitor 훅에 전달하지 않습니다. 원래 엔진 visitor 경로로 필터링한 뒤
  안전한 IED 액터 갱신만 요청합니다.
- 등록 외형과 실제 장비 숨김이 있는 상태에서 Alternative Perspective로
  헬겐 요새에 진입하는 등 장비 3D가 재구성되는 셀·시나리오 전환의
  재현 CTD를 방지하면서 액터별 숨김 상태는 그대로 유지합니다.

## UI 캐릭터 배치

- 왼쪽·오른쪽 캐릭터 표시를 동일한 절댓값으로 조금 더 바깥쪽에
  배치했습니다. 캐릭터 각도와 높이는 변경하지 않았습니다.

모드 설정 슬롯 연동, 바닐라 슬롯 자동 연동, 직접 슬롯 편집,
외부 탈의·재착의 트랜잭션, 조건, 키트 생성기, Grid Inventory Costume
연동과 DAVE/DAV/native 액터별 상태의 기존 동작은 유지됩니다.
