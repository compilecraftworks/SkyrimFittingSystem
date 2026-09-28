// Included inside namespace RE by the generated recording-provider fixture.
// Count scene ownership without deleting stack-owned test nodes.
template<class T> struct NiPointer {
  T* value{};
  NiPointer(T* p = nullptr) : value(p) { if (value) ++value->references; }
  NiPointer(const NiPointer& p) : NiPointer(p.value) {}
  NiPointer(NiPointer&& p) noexcept : value(std::exchange(p.value, nullptr)) {}
  ~NiPointer() { if (value) --value->references; }
  NiPointer& operator=(NiPointer p) noexcept { std::swap(value, p.value); return *this; }
  T* get() const { return value; }
  explicit operator bool() const { return value != nullptr; }
};
using BSFixedString = std::string;
struct NiExtraData { virtual ~NiExtraData() = default; };
struct NiFloatExtraData : NiExtraData { float value{}; };
struct NiStringExtraData : NiExtraData { const char* value{}; };
struct NiAVObject {
  bool hasHeel{};
  unsigned references{};
  NiFloatExtraData heel;
  std::string sdta;
  NiStringExtraData transformData;
  NiAVObject* parent{};
  std::vector<NiPointer<NiAVObject>> children;
  NiAVObject() = default;
  explicit NiAVObject(bool h) : hasHeel(h) {}
  NiAVObject* AsNode() { return this; }
  const auto& GetChildren() const { return children; }
  NiExtraData* GetExtraData(const BSFixedString& name) {
    if (hasHeel && name == "HH_OFFSET") return &heel;
    if (!sdta.empty() && name == "SDTA") { transformData.value = sdta.c_str(); return &transformData; }
    return nullptr;
  }
};
using NiNode = NiAVObject;
