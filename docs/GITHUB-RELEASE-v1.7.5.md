## English

- Fixed registered high-heel height remaining during SexLab animations after actual footwear was stripped.
- Coordinate SFS-managed heel height with SexLab's RemoveHeelEffect setting and existing correction, avoiding missing or double compensation without changing SexLab's transform key.
- Clear hidden registered height even when old display nodes detach late; preserve visible actual-footwear HH_OFFSET/SDTA and other mods' named transforms.
- Use the same policy through legacy NiOverride/public NiTransform and native/DAV/DAVE. Preserve footwear-strip OFF, flat footwear, redress OFF and manual visibility behavior.
- Keep actual equipment, BodyMorph, dye, scripts, save data and the public API unchanged. No polling or inventory-menu scan was added.

Validation: SE/AE-only build and 38 regression executables passed. New-DLL in-game animation testing remains unverified.
Update: Keep settings, kits and saves. Requirements, supported game-runtime list and optional patches are unchanged; only SFSCore.dll changes in the runtime package.

## 한국어

- SexLab이 실제 신발을 탈의한 뒤 등록 하이힐 높이가 남아 애니메이션 위치가 어긋날 수 있던 경로를 수정했습니다.
- SexLab의 RemoveHeelEffect 설정과 기존 높이 보정에 SFS 하이힐 처리를 맞춰 보정 누락·이중 적용을 방지합니다. SexLab 자체의 변환 키는 변경하지 않습니다.
- 외형 노드 제거가 늦어도 숨긴 등록 외형의 높이를 해제하며, 실제 표시 신발의 HH_OFFSET/SDTA와 다른 모드의 변환은 보존합니다.
- 구형 NiOverride·신형 NiTransform 및 native·DAV·DAVE에 같은 정책을 적용합니다. 발 장비 탈의 OFF·일반 신발·재착의 OFF·수동 표시 동작을 유지합니다.
- 실제 장비·바디모프·염색·스크립트·저장 데이터·공개 API는 유지하며, 폴링이나 인벤토리 메뉴 스캔을 추가하지 않았습니다.

검증: SE/AE 전용 빌드와 회귀 검사 38개를 통과했습니다. 새 DLL의 실제 인게임 애니메이션 테스트는 아직 미검증입니다.
업데이트: 설정·키트·세이브를 유지하세요. 선행 모드·지원 게임 버전 목록·선택 패치는 그대로이며, 런타임 패키지에서는 SFSCore.dll만 변경됩니다.
