# Skyrim Fitting System v1.4.4

## Open Animation Replacer 표시 장비 조건

- OAR가 설치된 경우에만 OAR 공개 API로 읽기 전용 사용자 조건 네 개를
  등록합니다. 기술적으로 착용한 장비만이 아니라 액터별 최종 표시 외형을
  기준으로 판정합니다.
- `SFS_IsShownArmorEquipped`, `SFS_ShownArmorHasKeyword`,
  `SFS_IsShownArmorInSlotHasKeyword`, `SFS_IsShownBodyNaked`를 추가했습니다.
  OAR 애니메이션 모드와 작은
  `config.json` 호환 패치에서 사용할 수 있습니다.
- 최종 표시 장비에는 보이는 실제 장비와 표시 중인 등록 외형이 함께
  포함됩니다. SFS가 숨긴 실제 장비와 숨긴 등록 외형은 제외하므로, 숨긴
  32번 장비에 표시 중인 32번 등록 외형도 없으면 올바르게 나체로
  판정합니다.
- SFS는 OAR의 `GetWornArmor`를 후킹하지 않고, ARMO 키워드·실제 장비를
  바꾸지 않으며, OAR를 강제로 불러오지도 않습니다. OAR 또는 Conditions
  API가 없으면 조건 등록만 건너뛰고 기존 SFS 기능은 그대로 동작합니다.

조건 이름과 OAR JSON 예시는 소스 배포본의 `OpenAnimationReplacer-Conditions.md`를 참고하십시오.

## Dynamic Feminine Female Modesty Animations OAR 호환 패치

- DFFMA OAR 4.30용 별도 FOMOD 패치를 추가했습니다. 설치한 Player/NPC/GS
  Hovering 구성에 맞는 `config.json`의 슬롯별 표시 장비 조건만 교체합니다.
- 원래의 슬롯·키워드·부정 조건과 그 밖의 OAR 조건은 그대로 유지합니다.
  DFFMA의 애니메이션·메시·스크립트·DLL·플러그인은 포함하거나 교체하지
  않습니다.

## 별도 호환 패치

- 검증한 Grid Inventory 원본 리비전용 Costume 교체 DLL 패치를 추가했습니다.
  선택한 Costume의 방어구 레이아웃만 플레이어 SFS 기본 등록 외형으로
  반영하며, Costume을 끄면 해당 플레이어 등록 외형을 비웁니다. SFS가
  없으면 Grid Inventory는 원래 Costume 동작을 그대로 유지합니다.
- Helmet Toggle 2 스크립트 패치를 보완했습니다. HT2가 최종 관리하는
  머리 장비 배열을 기준으로 해당 액터의 30번 Head, 31번 Hair, 42번
  Circlet, 44번 Beard/Mask, 플레이어의 경우 55번 Face/Mask만 SFS에
  전달합니다. 다시 표시될 때는 HT2가 만든 임시 SFS 억제만 지우며,
  사용자가 저장한 눈 상태는 바꾸지 않습니다.
- HT2 컨트롤러 슬롯은 액터가 선택한 모드 설정 슬롯 연동(가상 토큰),
  바닐라 슬롯 자동 연동, 직접 편집 예외를 통해 해석합니다. 실제 장비
  소유권과 DAVE·DAV·Skyrim native 표시 경로는 건드리지 않습니다.
- Dynamic Footprints SKSE BASE v3.0용 패치를 추가했습니다. Dynamic
  Footprints 자신의 신발 분류에서만 보이는 SFS 37번 신발 외형을 읽고,
  해당 외형이 없으면 실제 신발 조회로 되돌아갑니다. 검증한 Dynamic
  Footprints DLL 해시와 코드 서명이 정확히 일치하지 않으면 자동으로
  비활성화됩니다.

## 최종 점검 범위

- v1.4.4 본체는 전체 `releasedbg` 빌드와 Kit Generator 로직 회귀 테스트를
  통과했습니다. 정적 점검으로 모드 설정 연동·바닐라 연동·직접 편집의
  액터별 트랜잭션, 임시 표시, 미리보기, 갱신 상태를 확인했습니다.
- DAVE, DAV, Skyrim native는 같은 액터별 상태를 공유하지만, 설치한 각
  백엔드의 실제 인게임 확인은 별도로 필요합니다. 선택형 호환 패치도
  공개 전에는 해당 원본 모드 환경에서 짧은 인게임 점검이 필요합니다.
