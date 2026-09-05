# Skyrim Fitting System v1.5.7

- AE→SE RaceMenu 역포트와 DAVE를 함께 사용할 때 표시 중인 등록 외형의
  RaceMenu 실시간 BodyMorph·OBody 변경이 반영되지 않던 문제를 수정했습니다.
  해당 역포트는 ActorUpdateManager 버전을 0으로 보고하지만 신형 공개 vtable을
  사용하므로, 기존 버전 번호만 보는 경로가 잘못된 함수를 호출해 SFS의 늦은
  외형 부착 observer를 등록하지 못했습니다.
- 현재 지원하는 RaceMenu 경로를 실제 레이아웃별로 읽기 전용 검증합니다.
  구형 SE v0, 신형 공개 레이아웃을 사용하는 v0 역포트, 정식 공개 v1/v2가
  각자 검증된 AddInterface 슬롯만 사용하며, 알 수 없는 배열은 호출하지 않고
  native 장면 캡처만 유지합니다.
- BodyMorph v4/v5 호출 구조와 DAVE·DAV·native의 액터별 실시간 모프 추적은
  그대로 유지합니다. 주기적 폴링, 전역 액터 검색, 강제 Update3D 보조 처리나
  바디 계열별 예외를 추가하지 않았습니다.
- 실패했던 DAVE 강제 갱신 실험 코드를 제거했습니다. 피팅 염색, Helmet
  Toggle 2, SexLab P+, SOS/TNG, 조건, 키트, 외형 잠금, 하이힐과 저장 데이터는
  v1.5.6 동작을 유지합니다.

런타임은 Skyrim SE/AE 공용 `SFSCore.dll` 하나입니다. PDB는 런타임과 소스
패키지에서 모두 제외합니다. 독립 ABI 테스트는 구형 v0, 신형 레이아웃 v0,
공개 v1/v2, BodyMorph v4/v5, 콜백 인수, 멱등 재시도와 미확인 레이아웃 차단을
검사하며 전체 빠른 회귀 테스트로 DAVE·DAV·native 추적과 기존 기능 경계도
함께 확인했습니다.
