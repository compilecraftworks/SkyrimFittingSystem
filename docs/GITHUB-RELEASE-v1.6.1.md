# Skyrim Fitting System v1.6.1 — SE/AE

## Downloads / 다운로드

- **SE-AE.zip**: installable runtime package / 설치용 파일
- **Source.zip**: corresponding source, pinned dependencies, tests and build scripts / 대응 소스·고정 의존성·테스트·빌드 스크립트
- **SHA256SUMS-v1.6.1.txt**: archive checksums / ZIP 무결성 확인
- **nexus-changelog-v1.6.1-en.txt / -ko.txt**: English / 한국어 체인지로그

Save/settings formats and prerequisites are unchanged. Existing optional
compatibility patches retain their current versions; they are not bundled
in the main runtime ZIP. Grid Inventory Costume and Helmet Toggle 2 integration
remain built into SFS, not separate optional patches.

저장·설정 형식과 선행 모드는 그대로입니다. 기존 선택 호환 패치의 버전은
변경되지 않았으며 본체 ZIP에 포함되지 않습니다. Grid Inventory Costume과
Helmet Toggle 2 연동은 별도 패치가 아닌 SFS 본체 기능입니다.

## English changelog

- Fixed a v1.6.0 path that skipped registered wig/clothing attachment when an existing custom-skin CALL hook had unrecognized ownership.
- Preserved the original engine visitor and added scoped hidden actual-equipment filtering for opaque CALL chains, without changing inventory, worn flags, object identity or BodyMorph ownership. Existing engine/IED routes are retained.
- Fixed long ENDBR64-prefixed and readable page-end trampoline detection.
- RaceMenu now retries missing interfaces/hooks after partial initialization, preserving successful connections. Higher BodyMorph, NiTransform and ActorUpdateManager interface versions use the last compatible public prefix with explicit assumption logs, rather than an upper-version block.
- DAVE retries early API negotiation after all DataLoaded listeners before choosing fallback ownership. A failed public refresh rebuilds the actor through existing engine hooks and retains dye/pose/high-heel follow-ups.
- Late registered attachments re-arm saved dye restoration independently of BodyMorph availability.
- Grid Costume accepts its known message prefix with appended fields/unfamiliar ABI numbers while retaining malformed-input and save-restoration protections.
- OAR retries failed API/condition registration at later startup fences without repeating successful registrations.
- All 12 fast regression executables and associated contract checks passed, including 40 production SE/AE custom-skin routing cases. RaceMenu source history audit covers 19 relevant change snapshots.

## 한국어 체인지로그

- 기존 스킨 CALL 훅을 식별하지 못하면 등록 가발·의상 부착까지 중단하던 v1.6.0 회귀를 수정했습니다.
- 원래 엔진 객체를 유지하면서 미식별 CALL 경로에도 실제 장비 숨김 필터를 적용합니다. 인벤토리·착용 플래그·객체 식별·BodyMorph 소유 상태는 변경하지 않으며 기존 엔진·IED 경로도 유지합니다.
- 긴 ENDBR64 중계 코드와 읽기 가능한 페이지 끝의 중계 코드 판별을 보완했습니다.
- RaceMenu 일부 연결만 준비돼도 초기화를 완료하던 문제를 수정했습니다. 성공한 연결은 유지하고 빠진 연결·훅만 재시도합니다. 상위 인터페이스 버전도 마지막 호환 API 구간으로 연결하며 가정을 로그에 남깁니다.
- DAVE 초기 API 실패는 모든 DataLoaded 처리 후 다시 확인합니다. 공개 갱신 실패 시에도 액터별 엔진 재구성과 염색·포즈·하이힐 복원을 유지합니다.
- 늦게 부착된 등록 외형의 염색 복원을 BodyMorph 가용 여부와 독립시켰습니다.
- Grid Costume의 추가 필드·미확인 ABI 번호도 기존 메시지 구간으로 처리합니다. 잘못된 입력 검사와 세이브 복원 보호는 유지합니다.
- OAR 초기 API·조건 등록 실패를 재시도하고 성공한 등록은 반복하지 않습니다.
- 회귀 테스트 실행 파일 12개와 부가 검사를 통과했습니다. 실제 SE·AE 스킨 훅 경로 40가지와 RaceMenu 소스 변경 시점 19개를 점검했습니다.

## Verification limits / 검증 범위

Higher-version prefix compatibility is an explicit assumption, not proof of
future ABI semantics. Not every historical RaceMenu ZIP has been binary-audited.
The original reporter's environment and in-game outcome remain unverified;
automated tests do not establish regression-free behavior in every game/mod
combination. Foreign rendering that bypasses the engine visitor is outside
the callback filter's scope.

상위 버전의 하위 호환 연결은 명시적인 가정이며 미래 ABI 의미까지 보장하지는
않습니다. 모든 과거 RaceMenu ZIP을 개별 바이너리 검증한 것은 아닙니다.
원 피드백 환경과 해당 사용자의 인게임 해결 여부는 미확인이며, 자동 테스트가
모든 게임·모드 조합의 무회귀를 보장하지는 않습니다. 엔진 visitor를 우회하는
외부 렌더링은 이번 콜백 필터의 범위 밖입니다.

[Detailed display/compatibility audit](https://github.com/compilecraftworks/SkyrimFittingSystem/blob/v1.6.1/docs/V1.6.1-DISPLAY-REGRESSION-AUDIT.md) · [RaceMenu version families and evidence gaps](https://github.com/compilecraftworks/SkyrimFittingSystem/blob/v1.6.1/docs/RaceMenu-Version-Compatibility-v1.6.1.md)
