#pragma once
namespace sfs::native::ostim {
// Optional official interface, not an engine hook. Nothing is registered when
// OStim is absent. The Papyrus patch owns the actual strip/redress calls.
void InitializeSceneListeners();
void EnablePatch();
}
