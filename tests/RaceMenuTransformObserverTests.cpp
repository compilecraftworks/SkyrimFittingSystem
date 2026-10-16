// Real native classifier/post-write observer + root/sync/lifetime functions.
// VM/named-transform providers are fakes, not reporter in-game reproduction.
#include "HighHeelRootFixture.inc"
#include <initializer_list>
namespace RE::BSScript {
struct TypeInfo {
 enum class RawType {kNone,kObject,kObjectArray,kBool,kInt,kIntArray,kString,kStringArray,kFloatArray};
 RawType value{}; RawType GetUnmangledRawType()const{return value;}
};
struct StackFrame {
 std::vector<Variable> args;
 unsigned GetPageForFrame()const{return 0;}
 const Variable& GetStackFrameVariable(unsigned n,unsigned)const{return args.at(n);}
};
struct Stack {StackFrame* top{};Variable returnValue;};
struct ErrorLogger {};
}
using Raw=RE::BSScript::TypeInfo::RawType;
struct NativeFunctionBase {
 std::string owner="NiOverride",name="AddNodeTransformPosition";
 bool isStatic=true; Raw returnType=Raw::kNone;
 std::vector<Raw> parameters{Raw::kObject,Raw::kBool,Raw::kBool,Raw::kString,Raw::kString,Raw::kFloatArray};
 const std::string& GetObjectTypeName()const{return owner;}
 const std::string& GetName()const{return name;}
 bool GetIsStatic()const{return isStatic;}
 RE::BSScript::TypeInfo GetReturnType()const{return{returnType};}
 std::size_t GetParamCount()const{return parameters.size();}
 void GetParam(unsigned n,RE::BSFixedString&,RE::BSScript::TypeInfo& out)const{out.value=parameters.at(n);}
};
#include "target-native.production.inc"
#include "native-classification.production.inc"
#include "native-argument.production.inc"
enum class NativeCallResult {kCompleted,kFailedAbort,kFailedRetry};
NativeCallResult nativeResult=NativeCallResult::kCompleted;
unsigned originalCalls{};
std::function<void()> insideOriginal;
NativeCallResult CallOriginalNative(NativeFunctionBase* fn,
 const RE::BSTSmartPointer<RE::BSScript::Stack>& stack,RE::BSScript::ErrorLogger*,
 RE::BSScript::Internal::VirtualMachine*,bool){
 ++originalCalls;
 if(nativeResult!=NativeCallResult::kCompleted)return nativeResult;
 auto& frame=*stack->top;
 auto* actor=static_cast<RE::Actor*>(ReadArgument<RE::TESObjectREFR*>(frame,0));
 const auto node=ReadArgument<std::string>(frame,3),key=ReadArgument<std::string>(frame,4);
 if(actor && node=="NPC" && key=="SexLab.esm"){
  if(fn->name=="AddNodeTransformPosition") actor->sexLabPosition=7.0F;
  else {stack->returnValue.value=actor->sexLabPosition.has_value();actor->sexLabPosition.reset();}
  skee::transform.UpdateNodeTransforms(actor,false,true,"NPC");
 }
 if(insideOriginal)std::exchange(insideOriginal,{})();
 return nativeResult;
}
#include "position-native.production.inc"
RE::BSScript::Variable Arg(std::any v){RE::BSScript::Variable r;r.argument=std::move(v);return r;}
void Attach(RE::Actor& a,RE::NiAVObject& n){n.parent=&a.root;a.root.children.emplace_back(&n);}
int main(){
 NativeFunctionBase add,remove;
 remove.name="RemoveNodeTransformPosition";remove.returnType=Raw::kBool;remove.parameters.pop_back();
 Check(ClassifyNativeUncached(&add)==TargetNative::RaceMenuAddPosition && ClassifyNativeUncached(&remove)==TargetNative::RaceMenuRemovePosition,"exact NiOverride static position contracts are selected");
 auto wrong=add;wrong.owner="OtherScript";Check(ClassifyNativeUncached(&wrong)==TargetNative::None,"unrelated script with same method is not hooked");
 wrong=add;wrong.parameters.back()=Raw::kObjectArray;Check(ClassifyNativeUncached(&wrong)==TargetNative::None,"different ABI is not treated as a position native");
 wrong=add;wrong.isStatic=false;Check(ClassifyNativeUncached(&wrong)==TargetNative::None,"member method is not treated as static NiOverride");
 wrong=remove;wrong.returnType=Raw::kNone;Check(ClassifyNativeUncached(&wrong)==TargetNative::None,"remove must retain bool contract");
 RE::Actor actor{0x14};RE::TESObjectARMO armor{0x910};RE::TESObjectARMA addon{0x911};
 RE::BSScript::StackFrame frame;auto stack=std::make_shared<RE::BSScript::Stack>();stack->top=&frame;
 auto arguments=[&](RE::Actor* a,bool first=false,const char* node="NPC",const char* key="SexLab.esm"){
  frame.args={Arg(static_cast<RE::TESObjectREFR*>(a)),Arg(first),Arg(true),Arg(std::string(node)),Arg(std::string(key)),Arg(std::vector<float>{0,0,7})};
 };
 auto call=[&](NativeFunctionBase& fn){return ObserveRaceMenuPositionNative(ClassifyNativeUncached(&fn),&fn,stack,nullptr,&RE::BSScript::Internal::vm,false);};
 for(auto version:{1U,2U,3U,4U,99U}){
  g_transformInterfaceVersion=version;g_transformInterface=version<3?nullptr:&skee::transform;
  Reset(actor);nativeResult=NativeCallResult::kCompleted;actor.displayed.insert(armor.id);actor.sceneOffsets={9};
  RE::NiAVObject heel{true};heel.heel.value=9;Attach(actor,heel);
  RegisteredAppearanceAttachmentObserver observer;observer.OnAttach(&actor,&armor,&addon,&heel,false,&actor.root,&actor.root);
  const std::array<RE::FormID,1> actors{actor.id};g_highHeelScenes.Update(0x120,actors);
  actor.sexLabPosition=-9.0F;DrainAll();Check(actor.displayedHeight==0,"initial ON reconciliation is aligned");arguments(&actor);
  CallOriginalNative(&remove,stack,nullptr,&RE::BSScript::Internal::vm,false);
  Check(actor.displayedHeight==9 && SKSE::tasks.pending.empty(),"unobserved late SexLab removal reproduces raised actor after successful ON sync");
  actor.sexLabPosition=-9.0F;skee::transform.UpdateNodeTransforms(&actor,false,true,"NPC");
  insideOriginal=[&]{Check(SKSE::tasks.pending.empty(),"notification never precedes original native write");};
  Check(call(remove)==NativeCallResult::kCompleted,"successful original result is preserved");DrainAll();
  Check(actor.displayedHeight==0 && !actor.sexLabPosition,"late removal rearms correction without restoring external key");
  actor.otherModHeight=3;call(add);DrainAll();
  Check(actor.displayedHeight==3 && actor.sexLabPosition==7 && !actor.temporary,"late replacement is reconciled; external key and unrelated height survive");
  call(remove);DrainAll();const auto before=actor.fullUpdates;call(remove);DrainAll();
  Check(actor.fullUpdates==before,"remove of missing key does not schedule a full scan");
  for(auto failure:{NativeCallResult::kFailedAbort,NativeCallResult::kFailedRetry}){nativeResult=failure;Check(call(add)==failure && SKSE::tasks.pending.empty(),"failed/retry native result and scheduling stay untouched");}
  nativeResult=NativeCallResult::kCompleted;
  for(auto key:{"OStim","internal","SFS_HH_SYNC","SomeOtherMod"}){arguments(&actor,false,"NPC",key);call(add);Check(SKSE::tasks.pending.empty(),"other owners and SFS writes cannot feed back into heel sync");}
  arguments(&actor,true);call(add);Check(SKSE::tasks.pending.empty(),"first-person writes stay outside third-person observer");
  arguments(&actor,false,"NPC Head [Head]");call(add);Check(SKSE::tasks.pending.empty(),"non-NPC writes remain unrelated");
  arguments(nullptr);call(add);Check(SKSE::tasks.pending.empty(),"null reference cannot enroll work");
  RE::Actor unmanaged{0x26};unmanaged.internalPosition=11;
  const std::array<RE::FormID,1> unmanagedIDs{unmanaged.id};g_highHeelScenes.Update(0x121,unmanagedIDs);
  arguments(&unmanaged);call(add);
  Check(SKSE::tasks.pending.empty() && unmanaged.internalPosition==11,"scene actor without an SFS heel keeps the original native and real heel state");
  g_highHeelScenes.Update(0x121,{});
  RE::TESObjectARMO actualBoots{0x912};actor.inventory.feet=&actualBoots;
  g_highHeelScenes.Update(0x120,actors,false);arguments(&actor);call(remove);DrainAll();
  Check(actor.displayedHeight==12,"OFF with retained footwear keeps selected registered height after late external cleanup");
  actor.inventory.feet=nullptr;call(add);DrainAll();
  Check(actor.displayedHeight==3 && actor.sexLabPosition==7,"OFF with stripped footwear reconciles late writes to barefoot alignment");
  g_highHeelScenes.Update(0x120,actors,true);
  arguments(&actor);actor.sexLabPosition.reset();sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&actor);SKSE::tasks.Drain();
  call(add);call(remove);DrainAll();
  Check(actor.displayedHeight==3 && g_queuedHighHeelSyncs.Size()==0 && g_pendingHighHeelResyncs.empty(),"writes during legacy callbacks finish at latest compensation and drain ownership");
  for(unsigned i=0;i<128;++i){call(add);call(remove);DrainAll();}
  Check(actor.displayedHeight==3 && !actor.temporary && g_queuedHighHeelSyncs.Size()==0,"128 native-write cycles leave no bootstrap or pending actor task");
  call(add);g_highHeelScenes.Update(0x120,{});sfs::native::racemenu::QueueRegisteredAppearanceHighHeelSync(&actor);DrainAll();Check(actor.internalPosition==9,"scene ending mid-write returns to current ordinary footwear");
  call(remove);Check(SKSE::tasks.pending.empty(),"ended scene cannot retain observer work");
  g_highHeelScenes.Update(0x120,actors);call(add);
  const auto beforeRevert=actor.fullUpdates;sfs::native::racemenu::ForgetAllRegisteredAppearanceNodes();DrainAll();
  Check(actor.fullUpdates==beforeRevert && g_queuedHighHeelSyncs.Size()==0 && g_pendingHighHeelResyncs.empty(),"revert cancels post-write notifications before they can alter the new scene");
  sfs::native::racemenu::ReleaseActorSceneResources(actor.id,true);actor.root.children.clear();heel.parent=nullptr;Check(heel.references==0,"native notifications retain no node references after release");
 }
 std::printf("%u production late-transform observer checks passed; not in-game proof.\n",checks);
}
