# Skyrim Fitting System v1.6.8 SE-AE

## English

- Fixed actor-local morph/high-heel scene references and dye GPU targets/previews remaining after actual unload or deletion.
- Preserved saved colors, live-morph reattachment eligibility, ordinary hide/show, preview and strip/redress behavior.
- Added unique work tickets and in-flight dye-build cancellation so old work cannot cancel or republish over replacement scene work. Other actors/components remain independent.
- No polling, world scan, forced 3D rebuild, real equipment operation, new dependency or API version change.
- All 24 regression executables and source checks passed, including 128 unload/reload cycles and concurrent build cancellation. No in-game testing or real GPU memory measurement was performed.

Only SFSCore.dll changes compared with v1.6.7. Keep existing settings, kits, saves and optional patches. The separately reported SkyUI opening delay has not been measured or confirmed eliminated.

## 한국어

- 실제 언로드·삭제 뒤 남던 액터별 모프·하이힐 씬 참조와 염색 GPU 타깃·미리보기 정리를 수정했습니다.
- 저장 색상, 재부착 시 실시간 모프 대상 정보, 일반 숨김·표시·프리뷰·탈의·재착의를 유지합니다.
- 작업 식별값과 진행 중인 염색 생성 취소를 추가해 이전 작업이 새 씬 작업을 취소하거나 낡은 결과를 재등록하지 못하게 했습니다. 다른 액터·컴포넌트는 유지합니다.
- 폴링, 전체 액터 탐색, 강제 3D 재생성, 실제 장비 조작, 새 선행 모드나 API 버전 변경은 없습니다.
- 128회 언로드·재로드와 동시 생성 취소 등을 포함해 회귀 검사 24개와 소스 검사를 통과했습니다. 인게임·실제 GPU 메모리 실측은 하지 않았습니다.

v1.6.7 대비 SFSCore.dll만 변경합니다. 기존 설정·키트·세이브·선택형 패치를 유지하세요. 별도 제보된 SkyUI 열기 지연의 완전 해소는 실측 확인하지 않았습니다.
