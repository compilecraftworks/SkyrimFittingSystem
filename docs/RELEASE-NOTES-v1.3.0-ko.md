# Skyrim Fitting System v1.3.0

## 주요 변경사항

- 모드별 탈의 브리지 대신 각 액터의 실제 최종 착용 상태를 기준으로 등록 외형을 임시 자동 숨김하는 공통 연동을 추가했습니다.
- 연동 안 함, 바닐라 자동 매칭, 모드 슬롯 포함 자동 매칭과 슬롯별 고정 예외를 제공하는 탈의 연동 팝업을 추가했습니다.
- 액터·세이브별 저장과 v1.2.2 co-save 레코드 순서 호환을 추가했습니다. 정식 `AEVS` v2는 `AEVS` 레코드가 없는 세이브에서 연동 안 함으로 시작하며 전역 `settings.json`이나 폐기된 개발용 `AEVS` v1을 이관하지 않습니다.
- 기본 50, 51, 61번의 특수 이펙트 슬롯 보호와 기본 OFF인 39번 방패 외형 옵션을 추가했습니다.
- OBody가 사용하는 RaceMenu `ApplyBodyMorphs` 경로는 검증된 기존 동작을 보존합니다. FHU·SGO 같은 NiOverride Papyrus 호출은 RaceMenu의 내부 지연 `UpdateModelWeight` 작업이 끝난 뒤 해당 액터의 SFS 외형만 DAVE·DAV·native 기존 표시 경로로 다시 구성합니다. 전역 액터 스캔이나 주기적 폴링은 사용하지 않습니다.
- 장비 Ctrl 다중 선택, 일괄 등록, 전체 높이 액터 목록, UTF-8 키트 경로와 잘못된 JSON 격리를 추가했습니다.
- 실제 장비 변경 뒤 DAVE, DAV, native 액터별 갱신을 보완하고 SOS/TNG 및 Helmet Toggle 2·Wet Function Redux 선택 패치 호환을 유지했습니다.
- 탈의 연동 실제 점유 슬롯을 ARMO BOD와 모든 ARMA 슬롯의 합집합으로 계산하고, 팝업 미리보기와 런타임 자동 숨김이 동일한 정규화 제어 마스크를 사용하도록 했습니다.
- 외부 메뉴 관리용 고정 C API `SkyrimFittingSystem_Open`, `SkyrimFittingSystem_Close`, `SkyrimFittingSystem_IsMenuOpen`, 런타임 전용 `SkyrimFittingSystem_SetHotkeyEnabled`를 추가했습니다. 네이티브 단축키 비활성화는 저장된 키 설정이나 다른 API에 영향을 주지 않으며 게임을 다시 실행하면 항상 활성화 상태로 시작합니다.
- 메인 ESP, SEQ와 폐기된 모드별 브리지 PEX/PSC를 제거했습니다. 메인 패키지는 ESP 없이 동작합니다.

## 업데이트 방법

- v1.2.2 메인 모드 폴더에 v1.3.0을 병합하지 말고 폴더를 교체하십시오. 그래야 폐기된 ESP, SEQ와 브리지 스크립트가 남지 않습니다.
- 개인 피팅 키트가 있는 `Interface/SkyrimFittingSystem/user` 폴더는 보존하거나 먼저 백업하십시오.
- Helmet Toggle 2와 Wet Function Redux 호환 패치는 계속 별도 선택 파일이며 사용하는 모드에 해당하는 패치만 설치하십시오.
- 모든 액터의 탈의 연동 기본값은 연동 안 함입니다. 전역 `settings.json`과 폐기된 개발용 `AEVS` v1은 마이그레이션 원본으로 사용하지 않습니다.

## 패키지·호환 참고

- 메인 패키지는 `SkyrimFittingSystem.dll`, UI 리소스와 `SkyrimFittingSystemNative.pex/psc` 중심으로 구성됩니다.
- SOS StorageUtil 동기화는 DLL 내부에서 처리하므로 폐기된 브리지 ESP나 PEX가 필요하지 않습니다.
- RaceMenu BodyMorph 호환을 위해 RaceMenu 소스를 패치할 필요가 없습니다.
- 새로운 탈의 연동 임시 상태는 기존 등록 외형, 원래 슬롯, 조건, 수동 표시 상태, 액터 소유권과 피팅 키트 JSON을 변경하지 않습니다.

## 검증 상태

- v1.3.0.0 개발 빌드, MO2 직접 설치형 압축 구조, 로케일 JSON 3종, UTF-8 파일, 외부 C export와 독립 API 소비자 경로는 정적·빌드 검증을 통과했습니다.
- 최종 DLL SHA-256은 `4821E5DE28B6D03D56AB0860723EF9073759C42D783DE617361403476902E681`입니다. MO2 개발 설치본은 payload 11개가 모두 해시와 일치했고 레거시 payload는 0개였으며 빈 `Seq` 폴더도 제거했습니다.
- 새 `AEVS` 레코드를 마지막에 추가하기 전에 공식 v1.2.2 co-save 레코드 prefix 순서를 직접 대조했습니다.
- 최종 인게임 회귀 검증은 별도 개발 세션에서 진행 중이며 이 문서는 완료를 단정하지 않습니다.

상세 변경사항은 별도의 v1.3.0 Nexus 업데이트 문서를 사용하십시오.
