# SkyUI 인벤토리 지연 확대 점검 — 2026-09-28~29

이 문서는 수정 전 진단 기록이다. 후속 1단계 수정과 최신 검증 결과는
[조회 비용 수정 기록](SKYUI-QUERY-COST-FIX-2026-09-29.md)을 참고한다.

## 결론과 검증 한계

기준 HEAD는 출시 1.7.1 `352ac888193536b9f060b5c3e007a524f987f774`다.
작업 폴더에는 별도의 1.7.2 UI/키트 변경이 있으나 이번 진단에서 제품 코드,
DLL, MO2, 배포 ZIP, 버전, GitHub는 수정하지 않았다. 검사 도구와 문서만 추가했다.

제보자의 약 4초 지연을 실측하거나 재현하지 않았다. 아래는 코드에서 확인한
반복 작업과 분리 검사 결과이며, 각각이 제보 지연에 기여한 시간은 모른다.
사용자 환경의 재현 여부도 미확인이다. 모든 외부 모드 조합을 검증했다는 뜻이 아니다.

직접적인 SFS의 SkyUI 목록 항목 확장/메뉴 열림 콜백은 찾지 못했다.
그러나 이것만으로 SFS의 간접 비용을 배제할 수 없다. 현재 우선 조사 경로는
아이템 능력치 산출 중의 몸통 조건 질의와 SFS 최종 표시 상태 재계산이다.
추가로 숨김 슬롯 조회 안의 중복 수집, 조건 조회의 표시용 데이터 복사,
Papyrus 타입 검사 캐시 충돌에 따른 재검사를 확인했다.

## WornHasKeyword와 누드 판정

`WornHasKeyword`는 일반적인 착용 장비 키워드 질의이며 전부 누드 판정인 것은 아니다.
SFS는 `ArmorCuirass`/`ClothingBody`에 대해서만 최종 표시 결과를 반영한다.
관련 코드는 `src/native/DisplayedBodyCondition.cpp:15`,
`src/native/ArmorSkinning.cpp:2999`,
`src/features/virtual_tokens/VirtualWornTokens.cpp:2223`이다.

실제 몸통 장비를 숨긴 경우와 등록 몸통 외형을 표시한 경우를 구분해야 하므로,
이 경로를 제거하거나 인벤토리/대화 중 바닐라 착용 결과만 반환하면 기능 회귀다.
또한 다른 모드의 모든 누드 판정 방식이 이 함수 하나로 통합되는 것은 아니다.

## 1. 아이템 수와 연결될 수 있는 주 경로

- SkyUI `ItemMenu.InitExtensions`는 SKSE 확장 데이터를 활성화한다.
- SKSE 목록 항목 생성은 확장 InventoryData를 호출한다.
- SKSE 2.0.20의 InventoryData는 방어구 항목의 `GetArmorValue`를 호출한다.
- 앞선 읽기 전용 Skyrim.esm 검사에서 WellFitted/CustomFit의 ModArmorRating
  조건에 `WornHasKeyword(ArmorCuirass)`가 존재함을 확인했다.
- 이 몸통 조건이 SFS에 들어오면, 현재 호출마다 새 요청용 착용 스냅샷을 만들고
  BuildDisplaySet을 실행한다. 한 질의 안의 스냅샷은 공유하지만 질의 간에는 재사용하지 않는다.

앞의 SKSE 호출과 게임 데이터는 확인된 사실이다. 제보자가 해당 perk/override를
사용하는지, 실제 메뉴 열기에서 이 연결을 몇 번 거치는지는 확인하지 못했다.
따라서 이 전체 연결을 제보 환경에서 실행 추적한 것으로 표현하면 안 된다.
아이템 수와 목록 행 수, 동일 아이템의 스택 수량도 같은 개념이 아니다.

기존 분리 검사에서 동일 상태 몸통 질의 100/120/1000회는 착용 수집 요청도
100/120/1000회였다. SFS 비활성 표시 상태의 관리 대상 플레이어도 조기 판정 전에
수집하는 경로가 있다. 비관리 NPC/무관한 키워드/재진입은 기존 빠른 반환을 유지한다.
이는 엔진이 실제로 방문한 전체 인벤토리 원소 수나 밀리초 측정이 아니다.

확인한 업스트림 원문:

- [SkyUI ItemMenu](https://github.com/schlangster/skyui/blob/master/src/ItemMenus/ItemMenu.as)
- [SKSE 목록 확장](https://raw.githubusercontent.com/ianpatt/skse64/master/skse64/Hooks_Scaleform.cpp)
- [SKSE 2.0.20 InventoryData](https://raw.githubusercontent.com/ianpatt/skse64/v2.0.20/skse64/ScaleformExtendedData.cpp)

## 2. 새로 분리 확인한 숨김 슬롯 중복 수집

`GetHiddenRealEquipmentSlotMask`는 BuildDisplaySet에서 착용 장비를 수집한 뒤,
`CollectHiddenWornSlotMask`에서 다시 수집한다.
관련 위치: `ArmorSkinning.cpp:2207`, `3083`.

현재 함수 본문을 그대로 추출하고 엔진/표시 경계만 기존 테스트 모형으로 대체했다.

| 검사 | 호출 수 | 착용 장비 수집 요청 수 |
| --- | ---: | ---: |
| IsRealEquipmentHiddenForActorSlots | 1,000 | 2,000 |
| 위 조회가 참인 뒤 GetDisplayedFittingSlotMask도 조회 | 1,000쌍 | 3,000 |
| 슬롯 마스크 0 대조 검사 | 1 | 0 |

두 번째 순서는 Wet Function 패치의 `SFSShouldOperateSlot`에 있다.
다만 해당 helper는 텍스처가 없고 alwaysOperate가 아닌 UpdateSN 경로에서만 호출된다.
패치의 갱신 주기는 설정에 따른 OnUpdate이며, SkyUI 열림 이벤트가 아니다.
제보자가 이 패치를 사용했는지와 실제 호출 횟수는 이번 SkyUI 제보에서 확인되지 않았다.

이 중복은 한 요청 안의 착용 스냅샷을 전달하는 좁은 개선 후보다.
다른 질의까지 영구 캐시하거나 실제 장착/해제 관찰을 생략할 필요가 없다.

## 3. 조건 조회 비용의 구성

`EvaluateDefinitionStatus`와 `MaterializeConditionById`에는 이미 캐시가 있다.
따라서 조건을 매번 파싱/컴파일한다는 주장은 틀리다.

하지만 `ConditionMaterializer.cpp:120`의 캐시 적중 반환도 다음 값을 복사한다.

- condition shared_ptr
- signature 문자열
- displayCnf (`vector<vector<string>>`, UI 표시용 설명)
- refreshTargets의 액터 ID 목록

`ArmorSkinning.cpp:1390` 및 DD의 실행 판정 호출자는 이 중 condition만 사용한다.
조건 폴링도 각 조건의 condition만 보관한다. 표시 설명까지 깊은 복사하는 것은
현재 소스에서 확인되는 추가 비용이다. 설명 길이에 따른 게임 지연은 측정하지 않았다.

또 BuildDisplaySet의 잠긴 슬롯 사전 루프와 실제 적용 루프가 같은 행의 조건을
각각 평가한다(`ArmorSkinning.cpp:1703`, `1730` 부근). 잠긴 항목이 없는 행도
사전 루프에서 조건을 확인한다. 단, 확률/상태 의존 조건은 평가 횟수를 바꾸면
결과가 달라질 수 있으므로 무조건 결과 캐시로 합치면 안 된다.

안전한 우선 개선 후보는 실행용 condition 소유권을 유지한 경량 조회이며,
기존 편집 UI용 반환값과 조건 무효화/문자열 수명을 보존해야 한다.

## 4. Papyrus 타입 검사 캐시 충돌

현재 캐시는 128칸 direct-mapped이며 타입 주소를 해싱한다.
같은 칸을 사용하는 두 타입은 서로를 퇴출한다.
`VirtualWornTokens.cpp:3682` 부근의 생산 함수 본문으로 실제 모형 객체 주소의
충돌 쌍을 골라 다음을 확인했다. 각 타입은 global 항목 12개다.

| 검사 | 타입 조회 | 즉시 함수 항목 검사 | 지연 함수 항목 검사 |
| --- | ---: | ---: | ---: |
| 이미 완료된 동일 타입 반복 | 128 | 0 | 0 |
| 충돌하는 두 타입 번갈아 조회 | 128 | 1,536 | 24 |

의도적으로 충돌을 유발한 대조 실험이다. 제보자의 VM 주소/조회 순서가 이와
같다는 증거는 없다. 검사 누락이나 무한 누수가 아니라 제한된 캐시의 재검사 비용이다.
메모리 제한, 타입 소유권, 최초 동기 검사, post-link 재검사, 늦은 native 등록,
P+ 지연 member 발견은 모두 유지해야 한다. 클래스 이름으로 영구 완료 처리하거나
일반 스크립트 관찰을 차단하는 해결책은 부적절하다.

## 5. 호출 경로별 확인 범위

| 범위 | 확인 결과와 남은 제한 |
| --- | --- |
| SkyUI/Scaleform 등록 | SFS 본체에 목록 항목 확장 등록 및 Inventory/Container 메뉴 열림 이벤트 구독 없음. 간접 engine/Papyrus 경로는 별개. |
| 전체 인벤토리 필터 | `PlayerInventory.cpp` GetInventory는 SFS 장비 목록 필터에서 사용. SFS Draw는 닫힌 상태에서 즉시 반환. 일반 SkyUI 열림과 직접 연결되지 않음. |
| 입력/Present | 입력 큐 처리, 메뉴 요청, Grid pending 처리, API pump, 500ms 조건 tick. 목록 생성 콜백이나 고정 수초 sleep 없음. |
| 조건 폴링 | SFS UI를 닫아도 동작. 조건이 지정된 행/규칙의 액터만 확인하고 변화 때 refresh. 전체 세계/인벤토리 목록 스캔은 아님. 게임 준비 여부만 보므로 메뉴 중에도 예약될 수 있고, 실제 실행 시각/경쟁은 미측정. |
| 조건 엔진 재진입 | 몸통 query는 BuildDisplaySet 깊이 가드가 있음. 재진입 때 바닐라 답 유지. 무한 body-query 재귀의 증거 없음. |
| Papyrus native | 선택한 장비/토큰/키워드 함수만 패치. 일반 non-token 키워드 호출의 caller 추적 생략은 현재 테스트 통과. 착용/변경/필터/DD/P+ 경로는 유지. |
| Papyrus 타입 조회 | 동일 타입 반복 최적화는 유효하나 위 캐시 충돌 시 재검사 가능. post-link 250ms 대기는 worker의 지연이며 메뉴 스레드의 고정 대기가 아님. |
| 외부 장비/HT2/P+/DD | 선택된 장비 변동 및 정확한 HT2 신호 처리. SkyUI 목록 읽기 자체를 실제 탈의/재착의로 다루는 등록 경로는 찾지 못함. 외부 모드의 추가 호출은 미측정. |
| 렌더/API/IED | API 유휴 pump 빠른 반환, IED는 published value를 읽음. 단 BuildDisplaySet의 producer는 중복 질의 때도 visible actual 구성, body flag 계산, Normalize/비교를 수행. 외부 콜백은 producer lock 안에서 직접 실행하지 않음. |
| OAR/Modesty | OAR용 네 조건은 IED와 달리 즉시 최종 표시 스냅샷을 계산. Modesty 설정에 SFS OAR 조건 존재. 조건 평가마다 비용이 발생할 수 있으나 메뉴와의 호출 상관은 미확인. |
| Wet Function | 위 숨김/표시 슬롯 중복 수집 경로 존재. 설정 기반 갱신이며 메뉴 열림 구독은 아님. |
| Dynamic Footprints | 별도 DLL의 특정 footwear 조회를 SFS 최종 표시 조회에 연결. SFS/SkyUI 목록 전체 확장 훅이 아님. 발걸음 중 최종 표시 계산 비용은 별개. |
| DAVE/DAV/native 스키닝 | 공유 BuildDisplaySet과 각 actor refresh/attachment 경로. 메뉴의 모든 소지품마다 스키닝한다는 증거 없음. 외부 모드가 메뉴 중 refresh를 유발할 가능성은 실측하지 못함. |
| RaceMenu 모프/하이힐 | actor attachment 및 morph 요청 경로, 예약 합치기/제한 재시도. 일반 item preview의 비액터 입력은 actor guard에서 반환. 앞선 HH pre-graft 기록 소실 결함은 별개이며 미수정. |
| 염색/3D 아이템 미리보기 | 전역 shader hook은 존재. tint 미사용이면 atomic fast path, 사용 시 geometry 주소 lookup; 모든 소지품의 DDS를 읽거나 GPU 텍스처를 매번 만드는 목록 경로는 없음. 외부 shader 경쟁 실측 없음. |
| SOS/TNG | BuildDisplaySet에서 분류/키워드 처리가 반복될 수 있음. genital resolver는 pending/attempted로 중복 dispatch 제한. 전역 armor migration은 로드 경계의 일회성 작업이지 매 메뉴 작업이 아님. |
| 파일 I/O/키트/설정 | 카탈로그 구축/키트 생성/설정 저장은 각각 데이터 준비와 SFS 사용자 동작 경로. 메뉴를 열 때마다 SFS가 JSON/DDS/전체 ESP 목록을 읽는 경로는 확인되지 않음. |
| ESL/스크립트 | 앞선 파싱에서 ESL은 토큰 ARMO 32개, VMAD 0개. 본체 PSC는 native 선언이며 자체 inventory-open/poll 스크립트가 아님. 구버전 ESL 부재만으로 원인을 확정할 수 없음. |
| 로그 | CommonLib logger는 NDEBUG 경계로 release info/debug 기준을 선택. debug 인수 계산 자체는 남을 수 있음. 제보자의 실제 로그량/파일 지연은 미확인. |

## 6. 이번 실행 검사

`output/skyui-coverage-audit-20260929/GenerateProbe.ps1`와 `build-probe.cmd`로
현재 생산 본문을 다시 추출하여 기존 MSVC로 아래 8개 실행 파일을 빌드/실행했다.
전부 종료 코드 0. 게임 DLL을 로드하지 않았고 설치 경로에 쓰지 않았다.

1. CollisionProbe: 위 타입 충돌 재검사 재현, 안정 단일 타입 대조.
2. HiddenQueryProbe: 숨김 슬롯 2회 수집 및 Wet helper 호출쌍 3회 수집.
3. PapyrusObserverInspectionTests: 최초 호출/늦은 등록/타입 교체/재시도/P+/소유권 제한/reset.
4. WornSnapshotRegressionTests: 숨긴 실제 몸통/등록 몸통/액세서리/비관리/재진입/다음 질의의 상태 변화.
5. CallerChainPerformanceTests: 일반 키워드 빠른 반환과 토큰/탈의/재착의/DD/P+ 추적 보존.
6. RenderedOutfitAPITests: 실제 provider와 모형 엔진, 유휴 10,000프레임 추가 task/actor lookup 0, 중복 refresh 1,000개는 task/lookup 각 1.
7. EquipmentRefreshQueueTests: 128 load 경계, stale/reentry/task 불가 경계.
8. FittingDyeRulesTests: tint 대상 및 유휴 renderer guard 규칙.

이 결과는 게임의 4초 지연 해결, 모든 RaceMenu 배포본 ABI 실행 검증,
모든 모드 조합의 무회귀를 의미하지 않는다. 조건 폴링 테스트는 dispatch/상태를
검사하며 실제 엔진 조건 비용은 모형화하지 않는다.

## 다음 수정/검증의 경계

우선 후보는 요청 내 중복 착용 수집과 사용하지 않는 조건 표시 데이터 복사다.
이는 누드 판정/탈의 기능을 끄거나 장기 상태 캐시를 도입하지 않고 줄일 수 있다.
타입 캐시 충돌 개선은 bounded ownership과 모든 설치 복구 조건을 별도 검증해야 한다.

주 지연을 확정하려면 실제 메뉴 한 번의 inclusive/exclusive 호출 시간,
몸통 query/VisitWornItems 횟수, 타입 캐시 hit/miss, 조건 폴링/refresh와의 중첩이 필요하다.
실제 엔진 trace 없이 수집 요청 수를 밀리초로 환산하거나 사후 해결을 선언하지 않는다.
성능 계측을 하더라도 호출마다 로그 파일을 flush하거나 영구 진단 코드를 배포하지 않는다.
현재 사용자 지시는 점검이므로 위 후보 수정은 아직 적용하지 않았다.
