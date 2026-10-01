## English

- Fixed OCF/category keywords causing ordinary armor to receive an incorrect revealing classification. Upper-garment inference now uses item names and EditorIDs, with stricter matching for ambiguous words and prefixes.
- Preserved genuine upper garments, SOS manual choices, slot-49 lower garments and other mods' original keywords. Runtime keywords and virtual tokens use the same corrected result.
- Consolidated SOS API lookup with environment detection; SOS and TNG remain independently optional.
- Fixed DAV + Helmet Toggle 2 head/hair mask handling that could hide the face or hair again. DAV now preserves the variant-resolved result like DAVE, without changing actual equipment or inventory.

**Update:** Install the full package; keep settings, kits and saves. Requirements, runtime support, API ABI, helper ESL and optional compatibility patches are unchanged. OCF does not need to be removed.

**Validation:** SE/AE-only build, 36 regression executables and source checks passed, including 2,836 classification checks and 107,520 DAV/DAVE mask queries. Host tests are not in-game visual verification; the reporters' game configurations remain unverified.

## 한국어

- OCF 등 분류 키워드 때문에 일반 갑옷에 잘못된 노출 판정이 붙던 문제를 수정했습니다. 상의 추정은 장비 이름·EditorID만 사용하며 모호한 단어·접두어 오탐을 줄였습니다.
- 정상 상의 판정, SOS 수동 설정, 49번 하의 처리와 다른 모드의 원본 키워드를 보존합니다. 런타임 키워드와 가상토큰에 동일한 수정 결과를 적용합니다.
- SOS API 중복 탐색을 설치 감지 결과와 통합했습니다. SOS와 TNG는 각각 선택적으로 연동됩니다.
- DAV + Helmet Toggle 2에서 얼굴·머리카락을 다시 숨길 수 있던 마스크 처리를 수정했습니다. DAV도 DAVE처럼 변형 결과를 보존하며 실제 장비·인벤토리는 변경하지 않습니다.

**업데이트:** 본체 전체 패키지를 설치하고 설정·키트·세이브는 유지하세요. 선행 모드·게임 지원 범위·API ABI·보조 ESL·선택 호환 패치는 그대로이며 OCF를 제거할 필요는 없습니다.

**검증:** SE/AE 전용 빌드, 회귀 검사 36개와 소스 검사를 통과했습니다. 분류 검사 2,836건과 DAV/DAVE 마스크 조회 107,520건을 포함합니다. 호스트 검사이며 제보자 환경의 인게임 화면은 미검증입니다.
