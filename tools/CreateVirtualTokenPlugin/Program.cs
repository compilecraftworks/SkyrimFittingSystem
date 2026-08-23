using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;
using Noggog;

const string PluginName = "SkyrimFittingSystem-VirtualTokens.esl";
var outputDirectory = args.Length > 0 ? Path.GetFullPath(args[0]) : Path.GetFullPath("data");
Directory.CreateDirectory(outputDirectory);
var outputPath = Path.Combine(outputDirectory, PluginName);
var modKey = ModKey.FromFileName(PluginName);
var mod = new SkyrimMod(modKey, SkyrimRelease.SkyrimSE, forceUseLowerFormIDRanges: true)
{
    IsSmallMaster = true,
    UsingLocalization = false,
};
mod.ModHeader.Author = "Skyrim Fitting System";
mod.ModHeader.Description = "Fixed non-playable, model-less ARMO forms used only as virtual worn-slot tokens by the SFS experimental DLL.";
mod.ModHeader.FormVersion = 44;
mod.ModHeader.Flags |= SkyrimModHeader.HeaderFlag.Small;

static uint LocalFormIDForSlot(int slot) =>
    slot >= 43 ? (uint)(0x800 + slot - 43) : (uint)(0x813 + slot - 30);

for (var slot = 30; slot <= 61; ++slot)
{
    var armor = new Armor(new FormKey(modKey, LocalFormIDForSlot(slot)), SkyrimRelease.SkyrimSE)
    {
        EditorID = $"SFS_VirtualWornToken_Slot{slot}",
        Name = $"SFS Virtual Slot {slot}",
        BodyTemplate = new BodyTemplate
        {
            FirstPersonFlags = (BipedObjectFlag)(1u << (slot - 30)),
            ArmorType = ArmorType.Clothing,
        },
        Value = 0,
        Weight = 0,
    };
    armor.MajorRecordFlagsRaw |= 0x00000004; // Non-playable
    mod.Armors.Add(armor);
}

mod.ModHeader.Stats.NumRecords = 33;
mod.ModHeader.Stats.NextFormID = 0x820;
mod.BeginWrite
    .ToPath(new FilePath(outputPath))
    .WithNoLoadOrder()
    .WithExplicitOverridingMasterList(ModKey.FromFileName("Skyrim.esm"))
    .WithMastersListContent(Mutagen.Bethesda.Plugins.Binary.Parameters.MastersListContentOption.Iterate)
    .Write();

Console.WriteLine(outputPath);
