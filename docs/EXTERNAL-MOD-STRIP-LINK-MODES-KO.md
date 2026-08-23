# 외부 모드 탈의 연동 방식표

| 연동 방식 | 연동 방법 | 연동 기준 |
|---|---|---|
| 모드 설정 슬롯 연동 | 등록 외형을 실제 장비처럼 조회할 수 있는 가상 착용 토큰으로 외부 모드에 전달하고, 토큰의 탈의·재착의 결과에 따라 외형을 숨기거나 복원합니다. 토큰은 실제 인벤토리에 추가되거나 착용되는 장비가 아닙니다. | SexLab MCM처럼 각 모드 자체에 설정된 탈의 슬롯, 제외 키워드, 재착의 설정을 따릅니다. |
| 바닐라 슬롯 자동 연동 | 등록 외형의 이름·EditorID·키워드를 분석하여 적합한 바닐라 장비 슬롯에 자동으로 연결합니다. | 자동으로 연결된 실제 장비 슬롯의 탈의·재착의 상태를 따릅니다. |
| 슬롯 연동 직접 편집 | 모드 설정 슬롯 연동 또는 바닐라 슬롯 자동 연동을 기반 방식으로 선택한 뒤, 필요한 외형 슬롯만 직접 편집합니다. 편집하지 않았거나 `자동 매칭`을 선택한 슬롯은 선택한 기반 방식의 자동 연동 로직을 계속 따릅니다. | 바닐라 기반은 바닐라 슬롯만, 모드 설정 기반은 바닐라·모드 확장 슬롯을 선택할 수 있습니다. 선택값에 따라 실제 장비 또는 가상 착용 토큰에 연결하며, `연동 안 함`도 선택할 수 있습니다. |

## 슬롯 연동 직접 편집 규칙

| 기반 방식 | `자동 매칭` 동작 | 직접 선택 가능한 대상 | 직접 선택한 값의 의미 |
|---|---|---|---|
| 모드 설정 슬롯 연동 | 각 외형 슬롯을 가상 착용 토큰으로 제공하고 외부 모드 자체의 MCM·슬롯·키워드·재착의 설정을 따릅니다. | `연동 안 함`, 바닐라 슬롯, 모드 확장 슬롯 | 바닐라 슬롯은 해당 실제 장비에 연결하고, 모드 확장 슬롯은 해당 슬롯의 가상 착용 토큰에 연결합니다. |
| 바닐라 슬롯 자동 연동 | 이름·EditorID·키워드 분석 결과에 따라 알맞은 바닐라 실제 장비 슬롯을 자동 계산합니다. | `연동 안 함`, 바닐라 슬롯만 | 선택한 바닐라 슬롯의 실제 장비에 연결합니다. 모드 확장 슬롯은 목록에 표시하지 않습니다. |

특수 이펙트 보호 슬롯은 직접 선택 목록에서 제외합니다. 39번 방패 슬롯은 방패 외형 슬롯 사용 옵션이 꺼져 있으면 표시하지 않습니다.

## 외부 모드 호환표

아래의 **호환**은 해당 모드 이름을 하드코딩했다는 뜻이 아니라, 모드가 사용하는 대표 호출 유형을 SFS 범용 관찰 계층이 처리한다는 뜻입니다. 검증 상태에 적지 않은 개별 모드는 실제 장면 추가 확인이 필요합니다.

아래 모드명은 지원 대상을 제한하는 목록이 아니라 각 동작 유형의 대표 예시입니다. SFS는 특정 모드 이름을 하드코딩하지 않고 공통 탈의·재착의 호출과 액터의 최종 착용 상태를 범용적으로 관찰합니다. 따라서 표에 없는 모드라도 일반적인 탈의·재착의·압수·복장 교체 기능을 사용하면 별도 SFS 패치나 해당 모드 업데이트 없이 대부분 호환될 수 있습니다.

| 동작 유형 | 대표 모드 | 호환성 | 검증 상태 |
|---|---|:---:|---|
| SexLab 탈의·재착의<br>`StripActor` · `GetWornForm`<br>`UnequipItemEx` · `EquipItemEx` | SexLab Framework SE · SexLab Utility Plus · Soulgem Oven / SGO4 Integration Fork · Fill Her Up Baka Edition · SexLab Defeat Bane · BaboDialogue · Bimbos of Skyrim · Balazar’s Bitch · Sexy Adventures · SexLab Approach Redux · SexLab Aroused Creatures · SexLab Dialogues · SexLab Solutions · SexLab Romance · SexLab Body Search | 범용 호환 | SexLab·SGO 검증 완료 |
| 슬롯 필터·개별 탈착<br>`AddAllEquippedItemsToArray`<br>`GetWornForm`<br>`UnequipItem/Slot/Ex`<br>`EquipItem/ByID` | Private Needs – Orgasm · Bathing in Skyrim – Renewed · Licenses – Player Oppression · SLHH Expansion · Trap Needs to Be Real Trap / TNTR · Simple Player Prostitution · Death Trap Chest · Soul Resurrection | 범용 호환 | PNO·Bathing 검증 완료 |
| DD 장치·Hider<br>`GetWornForm`<br>`EquipItem*` · `UnequipItem*`<br>`zadNativeFunctions.SyncSetting` | Devious Devices SE/NG · Devious Helpless Redux · Devious Interests · Devious Cidhna · Devious Curses NG · Unforgiving Devices · Laura’s Bondage Shop | DD 전용 호환 | DD 착용·해제 검증 완료 |
| 감옥·압수·구금<br>`RemoveAllItems` · `RemoveItem`<br>`SetOutfit`<br>`EquipItem/UnequipItem` | Pama Prison Alternative · Pama Orkish Bounty Hunters · Pama Punishment · Pama Bad Ends · Pama Sovngarde · Pama Deadly Furniture · Captured by the Thalmor · Captive Player · Dark Arena · Bandit Paradise · Follower Slavery Mod | 범용 호환 | Pama 검증 완료 |
| 노예·강제 복장 교체<br>`StripActor` · `UnequipAll`<br>`RemoveAllItems` · `SetOutfit`<br>`EquipItem*` | Public Whore · Sanguine Debauchery / SD+ · Simple Slavery Plus Plus / Rebuild · S.L.U.T.S. Resume · Submissive Lola · Dress Up Lover’s NPC Outfit Changer | 범용 호환 | 추가 검증 필요 |
| 시작·퀘스트 복장 교체<br>`UnequipAll` · `RemoveAllItems`<br>`SetOutfit` · `EquipItem` | Alternate Perspective · Deviant Start · Kidnapped Start · Adventurer’s Start · The Adventurer’s Guild | 범용 호환 | 추가 검증 필요 |
