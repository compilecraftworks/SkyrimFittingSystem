#pragma once

#include <imgui.h>
#include <string>

namespace sfs::ui::condition_editor {
inline bool DrawTextClauseValueEditor(const char *a_id, std::string &a_value,
                                     float a_width) {
  ImGui::SetNextItemWidth(a_width);
  // Resize the actual string on paste/typing: no fixed buffer truncation and no
  // numeric filtering/normalization of variable names such as "0012".
  return ImGui::InputText(
      a_id, a_value.data(), a_value.capacity() + 1,
      ImGuiInputTextFlags_CallbackResize,
      [](ImGuiInputTextCallbackData *a_data) {
        auto &text = *static_cast<std::string *>(a_data->UserData);
        text.resize(static_cast<std::size_t>(a_data->BufTextLen));
        a_data->Buf = text.data();
        return 0;
      },
      &a_value);
}
} // namespace sfs::ui::condition_editor
