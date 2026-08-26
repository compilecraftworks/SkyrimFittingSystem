# Skyrim Fitting System v1.4.9

![Skyrim Fitting System](https://ac-o.arca.live/20260814sac/8c528bceffd65d882143d3ae2ae345038c9c08934c5f9cc8d8ff1c046f9904c1.png?expires=1787378391&key=wSK-nG9UwFePxIQ1PlXhgQ&type=orig)

실제 장비의 방어력·인챈트·효과는 유지하면서, 캐릭터에게 보이는 외형만 따로 구성하는 SKSE 기반 외형 시스템입니다. 기본 UI 단축키는 **F6**이며 옵션에서 바꿀 수 있습니다.

플레이어·NPC·팔로워의 등록 외형, 조건, 실제 장비 숨김, 수동 표시 상태와 외부 모드 탈의 상태는 모두 액터별로 분리됩니다. SFS는 DAVE, 일반 DAV, 둘 다 없는 Skyrim native 환경을 자동으로 구분하면서 같은 작업대와 저장 데이터를 사용합니다.

## 주요 기능

- 실제 장비를 벗지 않는 시각적 숨김과 능력치에 영향을 주지 않는 등록 외형
- 액터별 기본 외형·조건 외형·표시 상태와 실제 장비/등록 외형의 개별·전역 눈 버튼
- 장비·의상·키트 일괄 등록, JSON 피팅 키트, 인게임 키트 생성기
- 모드 설정 슬롯 연동, 바닐라 슬롯 자동 연동, 두 방식을 기반으로 한 직접 슬롯 편집
- 외부 모드의 탈의·재착의·압수·복장 교체를 액터별로 관찰하는 범용 연동
- RaceMenu BodyMorph, SOS/TNG, DAVE·DAV·native 표시 환경 지원
- OAR 최종 표시 외형 조건, 내장 Grid Inventory Costume 연동, 선택형 DFFMA OAR·Helmet Toggle 2·Dynamic Footprints 호환 패치

## 설치와 업데이트

1. 개인 키트가 있다면 `Interface/SkyrimFittingSystem/user`를 백업합니다.
2. 이전 SFS 모드 폴더를 완전히 삭제합니다.
3. 이전 독립형 **SFS Kit Generator**, 기존 **SFS Helmet Toggle 2 호환 패치**, v1.4.4 **SFS Grid Inventory Costume Compatibility Patch**가 있다면 완전히 삭제합니다. 원본 모드는 삭제하지 않습니다.
4. v1.4.9 본체 ZIP을 새 모드로 설치하고 `SkyrimFittingSystem-VirtualTokens.esl`을 활성화합니다.
5. 필요하면 백업한 개인 키트만 복원합니다.
6. Grid Inventory v1.4.1 이상은 별도 패치 없이 Costume 연동을 사용합니다. Wet Function Redux, DFFMA OAR, Helmet Toggle 2, Dynamic Footprints는 맞는 **별도 SFS 호환 패치만** 원본 모드 뒤에 설치합니다.

`SkyrimFittingSystem.dll`은 v1.4.0부터 `SFSCore.dll`로 바뀐 기존 단일 본체 DLL입니다. 구 폴더 위에 덮어쓰면 구·신 DLL이 함께 로드되어 같은 훅을 두 번 설치할 수 있으므로, 반드시 이전 SFS 폴더를 지운 뒤 새로 설치하십시오. v1.2.x·v1.3.x의 등록 외형·조건·숨김 데이터는 유지됩니다.

## 빠른 사용법

1. F6으로 SFS를 열고 상단에서 플레이어 또는 NPC를 선택합니다.
2. 장비·의상·키트 탭에서 외형을 더블클릭하거나 드래그해 등록합니다.
3. 작업대 왼쪽 **실제 장비**, 오른쪽 **등록 외형**의 눈 버튼을 각각 조절합니다.
4. 조건 탭의 붙박이 조건 또는 사용자 정의 조건을 연결해 상황별 외형을 구성합니다.
5. 외부 모드 탈의에 외형이 반응해야 하면 옵션에서 연동 방식을 고릅니다.
6. 키트 생성기에서는 의상 ESP를 스캔하고 후보를 편집·미리보기한 뒤 키트로 저장할 수 있습니다.

실제 장비를 숨겨도 착용은 유지되므로 방어력·인챈트·장비 효과가 사라지지 않습니다. 등록 외형의 눈 버튼은 시각적 표시만 바꾸며 등록 정보는 삭제하지 않습니다.

## 작업대와 조건

작업대 기본 영역은 평소 사용할 실제 장비와 등록 외형을 보여 줍니다. 아래 조건 영역의 각 행은 왼쪽 **조건 설정**, 오른쪽 **동작 설정**으로 나뉘며, 실제 장비와 등록 외형을 조건이 참일 때 표시하거나 숨길 수 있습니다.

![SFS 작업대 기본·조건 영역](https://ac-o.arca.live/20260815sac/48570fbbed7302d0877801be27a290dc6007671c1f65daaeb216ec7309fdf3a6.png?expires=1787378391&key=3eNdI2V2cYW7YlhzROMMuw&type=orig)

- 조건 카드 더블클릭은 가장 위의 빈 조건 카드에 조건만 등록합니다.
- 같은 조건의 빈 동작 행이 있으면 새 행보다 먼저 재사용합니다.
- 기본 영역의 등록 외형을 조건 행으로 드래그할 수 있습니다.
- 여러 조건이 동시에 참이면 위쪽 행이 우선합니다.

붙박이 조건은 실내·실외·도시·마을·던전·홈, 전투·비전투, 낮·밤, 비·눈, 은신·무기 상태 등을 제공합니다. 사용자 정의 조건은 Skyrim 조건 함수와 AND/OR를 조합해 `도시 AND 비전투`, `밤 AND 은신` 같은 규칙을 만들 수 있습니다.

![SFS 조건 탭](https://ac-o.arca.live/20260815sac/920d64d6e2084030218c8fac5859c931e6970326b4163d3063a967b1af779f80.png?expires=1787378391&key=enLGynsmIVppTL5lqFhMTg&type=orig)

## 외부 모드 탈의 연동

특정 모드 이름만을 위한 전용 패치가 아니라 `GetWornForm`, `UnequipItem*`, `EquipItem*`, `RemoveAllItems`, `RemoveItem`, `SetOutfit` 등 공통 호출과 최종 착용 결과를 관찰하는 범용 기능입니다. 사용자가 인벤토리에서 직접 입고 벗는 조작은 외부 이벤트로 처리하지 않습니다.

연동 방식과 슬롯 매칭은 모든 액터에 공통으로 적용하지만, 실제 탈의·숨김·복원 상태는 액터 FormID별로 분리됩니다. NPC A의 이벤트가 NPC B나 플레이어의 외형을 바꾸지 않습니다.

### 모드 설정 슬롯 연동

**권장 기본값입니다.** SexLab 등 각 외부 모드가 MCM/INI에서 선택한 스트립 슬롯·제외 키워드·재착의 규칙을 그대로 따릅니다.

SFS는 실제 인벤토리에 추가되지 않는 고정 FormID의 **가상 착용 토큰**을 제공합니다. 외부 모드가 자기 설정으로 토큰을 탈의하면 연결된 등록 외형이 숨겨지고, 같은 흐름에서 실제 장비 재착의가 확인되면 사용자의 이전 표시 상태로 돌아갑니다. 토큰은 무게·능력치·장비 효과를 만들지 않습니다.

![모드 설정 슬롯 연동](https://ac-o.arca.live/20260815sac/8f09085865cb2c22cca32761a03706d4258743a34474138d7c5abd17fdb4e2ca.png?expires=1787378391&key=sWplAoGDlvh6MiQx_b9f-Q&type=orig)

### 바닐라 슬롯 자동 연동

등록 외형의 이름, EditorID, 키워드와 ARMO/ARMA 점유 슬롯을 분석해 투구·몸통·장갑·신발·장신구 같은 바닐라 장비 슬롯에 자동 연결합니다. 외부 모드의 MCM 설정을 직접 따르지는 않고, SFS가 계산한 바닐라 슬롯의 실제 장비 탈착 결과를 따르므로 모드 설정 슬롯 연동과 다른 결과가 날 수 있습니다.

![바닐라 슬롯 자동 연동](https://ac-o.arca.live/20260815sac/eb3519e11948d2888ffae0236679e5087860028acc42363844ac6b5b6ee512f2.png?expires=1787378391&key=V53HRWDDH135s4-JO2xPLQ&type=orig)

### 직접 슬롯 편집

모드 설정 또는 바닐라 자동 연동을 기반으로 필요한 슬롯만 예외 지정합니다. 완전 수동인 별도의 세 번째 기반은 없습니다.

- `자동 매칭 -> 슬롯`: 해당 예외를 지우고 기반 방식의 자동 계산 사용
- `연동안함`: 해당 외형 슬롯만 외부 연동에서 제외
- 구체적인 슬롯: 선택한 대상 슬롯에 고정 연결

건드리지 않은 슬롯과 나중에 새로 등록한 외형은 계속 선택한 기반 규칙을 따릅니다. 직접 매칭도 전체 액터 공통 설정이지만 이벤트 결과는 계속 액터별로 분리됩니다.

## 선택형 호환 패치

- **Wet Function Redux Visual Effect Patch v1.2.0:** 시각 효과 스크립트만 제공합니다. Wet Function MCM을 덮어쓰지 않으며 RaceMenu 경고는 Wet Function의 기존 자체 검사입니다.
- **DFFMA OAR 4.30:** 선택한 OAR `config.json`의 표시 장비 조건만 바꿉니다. DFFMA 애니메이션·OAR 규칙·세이브·충돌 자체를 고치지는 않습니다.
- **Grid Inventory Costume v1.4.1 이상:** SFSCore가 공개 Costume 상태 메시지를 직접 받습니다. 플레이어 소유 ARMO가 하나 이상인 Costume을 착용·교체할 때만 플레이어 등록 외형을 교체합니다. Costume 해제·빈 Costume·비장비 구성은 SFS 등록 외형을 건드리지 않으며, 시작·세이브 로드·리버트 뒤 Grid가 보내는 첫 복원 상태도 무시합니다. Grid DLL을 교체하지 않으며 Grid의 UI·렌더·로드아웃·세이브·실제 장비는 그대로 Grid가 관리합니다.
- **Helmet Toggle 2:** HT2를 설치한 경우에만 별도 패치를 원본 모드 뒤에 설치합니다. HT2가 관리하는 30 Head·31 Hair·42 Circlet·44 Beard/Mask와 플레이어의 55 Face/Mask만 해당 SFS 등록 외형에 전달합니다. 실제 장비와 HT2의 토글·MCM·모델·충돌 자체는 HT2가 계속 관리합니다.
- **Dynamic Footprints SKSE BASE v3:** Dynamic Footprints 자신의 발자국 분류에서만 표시 중인 SFS 37번 신발 외형을 읽습니다. 발자국 규칙·에셋·세이브·충돌 자체를 고치지는 않으며 검증한 정확한 DLL 빌드가 아니면 자동 비활성화됩니다.

## 기타 호환

- RaceMenu가 있으면 실제 모프가 바뀐 액터와 그 액터의 등록 외형만 다시 동기화합니다. 전역 액터 스캔과 주기적 폴링은 사용하지 않습니다.
- SOS/TNG는 실제 장비와 등록 외형의 최종 표시 상태를 함께 평가합니다. 원래 ESP·KID·다른 모드의 키워드는 삭제하지 않습니다.
- SFS는 DAVE 공개 갱신 API, 일반 DAV, Skyrim native 표시 경로를 분리해 사용합니다. 환경이 달라도 저장 형식과 액터 소유권은 같습니다.
