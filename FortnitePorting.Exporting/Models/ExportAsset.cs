using System.Collections.Generic;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.Assets.Exports.Sound;
using CUE4Parse.UE4.Assets.Exports.StaticMesh;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Objects.Core.Math;
using CUE4Parse.UE4.Objects.UObject;
using Newtonsoft.Json;
// FVector2D is provided by CUE4Parse's CUE4Parse.UE4.Objects.Core.Math namespace.

namespace FortnitePorting.Exporting.Models;

/// <summary>
/// Generic asset reference used by extended (UE5) export types to reference dependencies.
/// The UE5 plugin will attempt to create placeholder / stub assets and wire them together
/// so the user gets a fully assembled project even for assets CUE4Parse can't fully decode.
/// </summary>
public class ExportAssetRef
{
    public string Path = string.Empty;
    public string Class = string.Empty;
    public string Name = string.Empty;
}

/// <summary>
/// Exported component description (used by Actor / Widget / Niagara exports).
/// Holds transform data and class information so the UE5 plugin can recreate the
/// component hierarchy as closely as possible without gameplay functionality.
/// </summary>
public class ExportComponent
{
    public string Name = string.Empty;
    public string Class = string.Empty;
    public FVector RelativeLocation = FVector.ZeroVector;
    public FRotator RelativeRotation = FRotator.ZeroRotator;
    public FVector RelativeScale3D = FVector.OneVector;

    // Component-specific properties (mesh reference, material overrides, etc)
    public Dictionary<string, object> Properties = new();

    // For mesh-type components: material slot overrides
    public List<ExportMaterial> Materials = [];

    // Child components
    public List<ExportComponent> Children = [];

    [JsonIgnore] public UObject? SourceObject;
}

/// <summary>
/// Extended actor export (no gameplay logic) — just components, transforms, meshes.
/// </summary>
public class ExportActor
{
    public string Name = string.Empty;
    public string Class = string.Empty;
    public string Path = string.Empty;
    public FVector Location = FVector.ZeroVector;
    public FRotator Rotation = FRotator.ZeroRotator;
    public FVector Scale = FVector.OneVector;
    public List<ExportComponent> Components = [];
    public List<ExportAssetRef> ReferencedAssets = [];
    public List<ExportMaterial> OverrideMaterials = [];
}

/// <summary>
/// Exported Niagara module / renderer / emitter attribute summary (best-effort).
/// Since CUE4Parse cannot fully decode Niagara bytecode, we collect the serialized
/// properties the plugin can reflect into a dummy UNiagaraSystem asset.
/// </summary>
public class ExportNiagaraModule
{
    public string Name = string.Empty;
    public string Class = string.Empty;
    public Dictionary<string, object> Attributes = new();
}

public class ExportNiagaraEmitter
{
    public string Name = string.Empty;
    public string Path = string.Empty;
    public List<ExportNiagaraModule> Modules = [];
    public List<ExportAssetRef> ReferencedAssets = [];
    public List<ExportMaterial> Materials = [];
}

public class ExportNiagaraSystem
{
    public string Name = string.Empty;
    public string Path = string.Empty;
    public List<ExportNiagaraEmitter> Emitters = [];
    public List<ExportComponent> Components = [];
    public List<ExportAssetRef> ReferencedAssets = [];
    public FVector LoopBounds = FVector.ZeroVector;
    public bool bLooping = true;
    public float WarmupTime;
}

/// <summary>
/// Widget export (UMG / UUserWidget): a tree of widget elements with common layout
/// properties plus a list of referenced textures/materials so the plugin can build a
/// rudimentary UWidgetBlueprint.
/// </summary>
public class ExportWidgetChild
{
    public string Name = string.Empty;
    public string Class = string.Empty;
    public FVector2D Position = new(0, 0);
    public FVector2D Size = new(100, 100);
    public FLinearColor Color = new(1, 1, 1, 1);
    public string Text = string.Empty;
    public ExportTexture? BrushTexture;
    public Dictionary<string, object> Meta = new();
    public List<ExportWidgetChild> Children = [];
}

public class ExportWidget
{
    public string Name = string.Empty;
    public string Path = string.Empty;
    public string ParentClass = string.Empty;
    public FVector2D DesignerSize = new(1920, 1080);
    public List<ExportWidgetChild> WidgetTree = [];
    public List<ExportAssetRef> ReferencedAssets = [];
}

/// <summary>
/// Generic data row for DataTable / CurveTable exports. Serialized as a flat
/// property dictionary so the plugin can reflect it into a UDataTable with
/// best-effort column support.
/// </summary>
public class ExportDataRow
{
    public string Name = string.Empty;
    public Dictionary<string, object> Properties = new();
}

public class ExportDataTable
{
    public string Name = string.Empty;
    public string Path = string.Empty;
    public string RowStruct = string.Empty;
    public List<ExportDataRow> Rows = [];
}

public class ExportCurve
{
    public string Name = string.Empty;
    public string CurveType = "Real"; // Real, Vector, LinearColor, Object
    public List<KeyValuePair<float, float>> Keys = [];
}

public class ExportCurveTable
{
    public string Name = string.Empty;
    public string Path = string.Empty;
    public List<ExportCurve> Curves = [];
}

/// <summary>
/// Generic data asset (UPrimaryDataAsset / UDataAsset) export — just properties
/// as name/value pairs and referenced assets.
/// </summary>
public class ExportDataAsset
{
    public string Name = string.Empty;
    public string Path = string.Empty;
    public string Class = string.Empty;
    public Dictionary<string, object> Properties = new();
    public List<ExportAssetRef> ReferencedAssets = [];
}

/// <summary>
/// Material Parameter Collection export.
/// </summary>
public class ExportScalarMPC { public string Name = string.Empty; public float Value; }
public class ExportVectorMPC { public string Name = string.Empty; public FLinearColor Value = new(); }

public class ExportMaterialParameterCollection
{
    public string Name = string.Empty;
    public string Path = string.Empty;
    public List<ExportScalarMPC> Scalars = [];
    public List<ExportVectorMPC> Vectors = [];
    public List<ExportAssetRef> ReferencedAssets = [];
}

/// <summary>
/// Physical material export (surface type + physical properties).
/// </summary>
public class ExportPhysicalMaterial
{
    public string Name = string.Empty;
    public string Path = string.Empty;
    public float Friction = 0.7f;
    public float Restitution = 0.3f;
    public float Density = 1.0f;
    public string SurfaceType = string.Empty;
}

/// <summary>
/// Cascade (legacy) ParticleSystem export (best-effort: emitters + materials +
/// referenced textures/curves).
/// </summary>
public class ExportParticleEmitter
{
    public string Name = string.Empty;
    public List<ExportMaterial> Materials = [];
    public List<ExportAssetRef> ReferencedAssets = [];
}

public class ExportParticleSystem
{
    public string Name = string.Empty;
    public string Path = string.Empty;
    public List<ExportParticleEmitter> Emitters = [];
    public List<ExportComponent> Components = [];
    public List<ExportAssetRef> ReferencedAssets = [];
}

/// <summary>
/// Skeleton export reference (skeleton itself is exported as UEMODEL binary,
/// this metadata provides socket/transform info for the plugin).
/// </summary>
public class ExportSkeletonSocket
{
    public string Name = string.Empty;
    public string ParentBone = string.Empty;
    public FVector RelativeLocation = FVector.ZeroVector;
    public FRotator RelativeRotation = FRotator.ZeroRotator;
    public FVector RelativeScale = FVector.OneVector;
}

public class ExportSkeleton
{
    public string Name = string.Empty;
    public string Path = string.Empty;
    public string BinaryPath = string.Empty;
    public List<string> BoneNames = [];
    public List<ExportSkeletonSocket> Sockets = [];
}

/// <summary>
/// Blend space export (samples + referenced animations).
/// </summary>
public class ExportBlendSpaceSample
{
    public FVector SampleValue = FVector.ZeroVector;
    public string AnimationPath = string.Empty;
}

public class ExportBlendSpace
{
    public string Name = string.Empty;
    public string Path = string.Empty;
    public string SkeletonPath = string.Empty;
    public List<ExportBlendSpaceSample> Samples = [];
    public List<ExportAssetRef> ReferencedAssets = [];
}

/// <summary>
/// Anim montage export (slots, sections, referenced animations).
/// </summary>
public class ExportAnimMontageSection
{
    public string Name = string.Empty;
    public float StartTime;
    public float EndTime;
}

public class ExportAnimMontage
{
    public string Name = string.Empty;
    public string Path = string.Empty;
    public string SkeletonPath = string.Empty;
    public string AnimationPath = string.Empty;
    public float Length;
    public List<ExportAnimMontageSection> Sections = [];
    public List<ExportAssetRef> ReferencedAssets = [];
}

/// <summary>
/// Level sequence export (binding summary, tracks as best-effort).
/// </summary>
public class ExportLevelSequenceBinding
{
    public string Name = string.Empty;
    public string DisplayName = string.Empty;
    public List<Dictionary<string, object>> Tracks = [];
}

public class ExportLevelSequence
{
    public string Name = string.Empty;
    public string Path = string.Empty;
    public float SequenceStart;
    public float SequenceEnd;
    public List<ExportLevelSequenceBinding> Bindings = [];
    public List<ExportAssetRef> ReferencedAssets = [];
}

/// <summary>
/// Physics / Chaos asset export (body setup best-effort).
/// </summary>
public class ExportPhysicsBody
{
    public string BoneName = string.Empty;
    public FVector Location = FVector.ZeroVector;
    public FRotator Rotation = FRotator.ZeroRotator;
    public float Mass = 1.0f;
}

public class ExportPhysicsAsset
{
    public string Name = string.Empty;
    public string Path = string.Empty;
    public string SkeletonPath = string.Empty;
    public List<ExportPhysicsBody> Bodies = [];
    public List<ExportAssetRef> ReferencedAssets = [];
}

/// <summary>
/// Blueprint / AnimBlueprint summary for best-effort stub generation.
/// </summary>
public class ExportBlueprint
{
    public string Name = string.Empty;
    public string Path = string.Empty;
    public string ParentClass = string.Empty;
    public string ClassType = "Blueprint"; // Blueprint or AnimBlueprint
    public List<ExportComponent> Components = [];
    public List<ExportAssetRef> ReferencedAssets = [];
    public Dictionary<string, object> Variables = new();
}

/// <summary>
/// Generic catch-all for unknown / unsupported export types: captures a list of
/// referenced assets + raw key properties so UE5 plugin can at least stub them.
/// </summary>
public class ExportGeneric
{
    public string Name = string.Empty;
    public string Path = string.Empty;
    public string Class = string.Empty;
    public Dictionary<string, object> Properties = new();
    public List<ExportAssetRef> ReferencedAssets = [];
}

/// <summary>
/// Extends ExportMaterial with extra UE5 fields (emissive, two-sided, etc.) so
/// plugin-side material recreation is more robust.
/// </summary>
public static class ExportMaterialExtensions
{
    // Extension dictionary is stored via ConditionalWeakTable in helper — but for
    // Newtonsoft.Json we just add extra fields via a separate companion record.
}

public record ExportMaterialExtras
{
    public bool TwoSided;
    public bool Masked;
    public float OpacityMaskClipValue = 0.333f;
    public string EmissiveTexture = string.Empty;
    public string MetallicTexture = string.Empty;
    public string RoughnessTexture = string.Empty;
    public string AOTexture = string.Empty;
    public string ORMTexture = string.Empty;
    public float MetallicScalar = -1f;
    public float RoughnessScalar = -1f;
    public FLinearColor EmissiveColor = new(0, 0, 0, 0);
}
