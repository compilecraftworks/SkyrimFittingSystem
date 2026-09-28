#include "kit_generator/SheetGrouping.h"
#include <future>
#include <iostream>
#include <stdexcept>

namespace s = sfs::kit_generator::sheet;
void Require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
auto Resolve(std::string_view plugin, std::string_view name, std::string_view id = {}) {
  return s::Resolve(plugin, name, id);
}
int main() {
  try {
    const auto bdor = "DM BDOR Pack by Team TAL.esp";
    const auto durandal = Resolve(bdor, "BDOR 듀란달 갑옷");
    Require(durandal.has_value() && durandal == Resolve(bdor, "듀란달 장갑") &&
            durandal == Resolve(bdor, "", "00BDORDurandalBoots"), "Merged B span must link Korean aliases and CamelCase EDIDs");
    Require(durandal != Resolve(bdor, "벤슬라 장갑"), "Adjacent sets must retain their boundary");
    const auto nini = "[SunJeong] Ninirim Collection.esp";
    const auto crystal = Resolve(nini, "검은색 크리스탈 씨쓰루 상의");
    Require(crystal.has_value() && crystal == Resolve(nini, "크리스탈 시스루 신발") &&
            crystal == Resolve(nini, "", "CristalSeeThroughBoots"), "Color/translation aliases must form one family");
    Require(Resolve(nini, "300+ 시계침 왕관").has_value() &&
            Resolve(nini, "300+ 시계침 왕관") == Resolve(nini, "Gift Bless 상의") &&
            !Resolve(nini, "300 Another Outfit"), "An explicit numeric-plus family must keep its accessories and punctuation boundary");
    const auto tull = "TULLIUS COLLECTION 01.esp";
    const auto omake = Resolve(tull, "오마케 상의");
    Require(omake.has_value() && omake == Resolve(tull, "Goma's Armlets") &&
            omake == Resolve(tull, "Minibikini C-String"), "One C span must connect multiple part-specific BodySlide names");
    const auto fox = Resolve(tull, "FOX27 Boots");
    Require(fox.has_value() && fox == Resolve(tull, "FOX 27 Hand A") &&
            !Resolve(tull, "FOX270 Boots"), "Numeric suffixes must not match a different numbered outfit");
    Require(!Resolve("Unrelated.esp", "오마케 상의"), "Sheet identities must be plugin scoped");
    Require(Resolve(tull, "하은 비키니 블랙") == Resolve(tull, "하은 밴드") &&
            Resolve(tull, "하은 비키니 블랙") != Resolve(tull, "비키니 브라"),
            "A generic part/set word inside another outfit name cannot steal that outfit");
    Require(!Resolve(tull, "Unlisted Set 수영복") && !Resolve(tull, "Unlisted Set 헤드폰"),
            "Unlisted outfits must not be captured by a generic suffix");
    Require(Resolve(tull, "[Maker] 오마케 상의") == omake,
            "Leading bracketed maker tags may precede an explicit set root");
    Require(Resolve("tullius collection 01.ESP", "FOX 27 HAND A") == fox, "Delimited aliases and plugin names must be case insensitive");
    Require(!Resolve(tull, "Lisa's Swimsuit"), "Equal-strength aliases assigned to different sheet sets must fail closed");
    const auto modern = "Tullius Modern Outfits.esp";
    Require(Resolve(modern, "KBO 상의") && Resolve(modern, "아디다스 슈퍼스타 신발") &&
            Resolve(modern, "KBO 상의") != Resolve(modern, "아디다스 슈퍼스타 신발"), "A shared plugin cell cannot union distinct B/C families");
    Require(Resolve("Honoka Cosplay.esp", "호노카 코스프레 의상 A") ==
            Resolve("Honoka Cosplay.esp", "호노카 코스프레 장갑 G") &&
            Resolve("Honoka Cosplay.esp", "호노카 코스프레 의상 A").has_value(),
            "Corroborated multiword family labels must bring separate part rows together");
    Require(!Resolve(bdor, "남성용 갑옷") && !Resolve(bdor, "바슬 없음 신발"), "Generic sheet labels are not family identities");
    for (const auto &family : s::kSheetFamilies)
      Require(family.count && family.offset + family.count <= std::size(s::kSheetAliases), "Invalid generated alias span");
    std::vector<std::future<bool>> tasks;
    for (int i = 0; i < 8; ++i) tasks.push_back(std::async(std::launch::async, [&] { return Resolve(tull, "오마케 신발") == omake; }));
    for (auto &task : tasks) Require(task.get(), "Concurrent sheet matching must be deterministic");
    std::cout << "Sheet alias, merged-cell, part-family, numeric, ambiguity, plugin-boundary and concurrency tests passed\n";
    return 0;
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
