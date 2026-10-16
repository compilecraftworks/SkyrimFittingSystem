## English

- Fixed additional registered high-heel alignment cases during SexLab scenes, including when RemoveHeelEffect is ON and actual footwear has been stripped.
- Reconcile late SexLab height-correction changes and footwear strip/redress without overwriting SexLab's transform key or other mods' named transforms.
- Fixed hidden actual helmets becoming visible again after redress until the SFS UI was reopened. Reapply the saved visibility state across native, DAV and DAVE.
- Preserve footwear-strip OFF, redress OFF, manual visibility, locked/protected appearances and existing SexLab, SLU+, P+, OStim and DD strip/redress behavior.
- No actual-equipment equip/unequip commands, periodic actor/inventory scans, replacement framework scripts or new mandatory dependencies were added.

Validation: SE/AE-exclusive build, 44 production-source/host regression executables and source-boundary checks passed. Ordinary SexLab 1.63, installed SLU+ 2024.4.7 and P+ 2.17.1 contracts were compared; the reporter's in-game animation setup and every external binary were not individually exercised.

Update: Keep settings, kits and saves. Only SFSCore.dll changes in the runtime package. Requirements, supported runtimes, scripts and helper ESL are unchanged. The optional OStim / OStim Standalone patches remain v1.0.0; use exactly one matching patch, and existing users need not reinstall it.

## 한국어

- SexLab 장면에서 실제 발 장비를 탈의한 뒤 등록 하이힐 높이가 남는 추가 경로를 수정했습니다. RemoveHeelEffect 옵션 ON인 경우도 포함합니다.
- 뒤늦게 변경되는 SexLab 높이 보정과 발 장비 탈의·재착의를 반영합니다. SexLab 변환 키와 다른 모드의 이름별 변환은 덮어쓰지 않습니다.
- 실제 투구를 숨겨도 재착의 후 다시 표시되어 SFS UI를 열어야 숨겨지던 경로를 수정했습니다. native·DAV·DAVE에서 저장된 표시 상태를 다시 적용합니다.
- 발 장비 탈의 OFF·재착의 OFF·수동 표시·잠긴 외형·보호 슬롯과 기존 SexLab·SLU+·P+·OStim·DD 탈의·재착의 처리를 유지합니다.
- 실제 장비 착용·해제 명령, 주기적 액터·인벤토리 스캔, 프레임워크 스크립트 교체나 새 필수 선행 모드는 추가하지 않았습니다.

검증: SE/AE 전용 빌드·생산 소스/호스트 회귀 실행 파일 44개·소스 경계 검사를 통과했습니다. 일반 SexLab 1.63·설치된 SLU+ 2024.4.7·P+ 2.17.1 호출 규약을 대조했으며, 제보자의 인게임 애니메이션 구성이나 모든 외부 바이너리를 개별 실행한 것은 아닙니다.

업데이트: 설정·키트·세이브를 유지하세요. 런타임 패키지는 SFSCore.dll만 변경합니다. 선행 모드·지원 게임 버전·스크립트·보조 ESL은 그대로입니다. 선택형 OStim·OStim Standalone 패치는 1.0.0을 유지하며 맞는 하나만 사용하세요. 기존 설치자는 패치를 다시 설치할 필요가 없습니다.
