using System;
using System.Collections.Generic;
using System.Reflection;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Assets.Exports.Animation;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse.UE4.Assets.Exports.Material.Parameters;
using CUE4Parse.UE4.Objects.Engine.Animation;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.Assets.Exports.Sound;
using CUE4Parse.UE4.Assets.Exports.StaticMesh;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Assets.Objects;
using CUE4Parse.UE4.Assets.Objects.Properties;
using CUE4Parse.UE4.Objects.Core.Math;
using CUE4Parse.UE4.Objects.Engine;
using CUE4Parse.UE4.Objects.UObject;
using FortnitePorting.Exporting.Models;
using FortnitePorting.Exporting.Models.Files.Meta;
using FortnitePorting.Shared.Extensions;

namespace FortnitePorting.Exporting.Types;

public class GenericAssetExport : BaseExport
{
    public List<ExportAssetRef> ReferencedAssets = [];
    public List<ExportMaterial> Materials = [];

    public ExportMaterialParameterCollection? MPC;
    public ExportPhysicalMaterial? PhysicalMaterial;
    public ExportParticleSystem? ParticleSystem;
    public ExportSkeleton? Skeleton;
    public ExportBlueprint? Blueprint;
    public ExportBlendSpace? BlendSpace;
    public ExportAnimMontage? AnimMontage;
    public ExportLevelSequence? LevelSequence;
    public ExportPhysicsAsset? PhysicsAsset;
    public ExportCurveTable? CurveTable;
    public ExportDataAsset? DataAsset;
    public ExportGeneric? Generic;

    public GenericAssetExport(string name, UObject asset, EExportType exportType, ExportDataMeta metaData, IExportFileMeta? fileMeta)
        : base(name, exportType, metaData)
    {
        var refs = new List<ExportAssetRef>();

        Context.GatherDependencies(asset, refs, Materials);
        ReferencedAssets = refs;

        switch (asset.ExportType.ToLowerInvariant())
        {
            case "materialparametercollection":
                MPC = BuildMPC(asset);
                break;
            case "physicalmaterial" or "physicalmaterialmask":
                PhysicalMaterial = BuildPhysicalMaterial(asset);
                break;
            case "particlesystem":
                ParticleSystem = BuildParticleSystem(asset, refs);
                break;
            case "skeleton":
                Skeleton = BuildSkeleton(asset, refs);
                break;
            case "animblueprintgeneratedclass" or "animblueprint":
                Blueprint = BuildBlueprint(asset, "AnimBlueprint", refs);
                break;
            case "blueprintgeneratedclass" or "blueprint":
                Blueprint = BuildBlueprint(asset, "Blueprint", refs);
                break;
            case "blendspace" or "blendspace1d" or "aimoffset" or "aimoffsetblendspace":
                BlendSpace = BuildBlendSpace(asset, refs);
                break;
            case "animmontage":
                AnimMontage = BuildAnimMontage(asset, refs);
                break;
            case "levelsequence":
                LevelSequence = BuildLevelSequence(asset, refs);
                break;
            case "physicsasset" or "rbphysicsconstraintsetup" or "bodysetup":
                PhysicsAsset = BuildPhysicsAsset(asset, refs);
                break;
            case "curvetable":
                CurveTable = BuildCurveTable(asset, refs);
                break;
            default:
                if (exportType == EPrimitiveExportType.DataAsset.AsExportType() ||
                    asset.ExportType.EndsWith("DataAsset", StringComparison.OrdinalIgnoreCase) ||
                    (asset.ExportType.EndsWith("Data", StringComparison.OrdinalIgnoreCase) && asset.ExportType.Contains("Data", StringComparison.Ordinal)))
                {
                    DataAsset = BuildDataAsset(asset, refs);
                }
                else
                {
                    Generic = BuildGeneric(asset, refs);
                }
                break;
        }
    }

    private ExportMaterialParameterCollection BuildMPC(UObject asset)
    {
        var mpc = new ExportMaterialParameterCollection { Name = asset.Name, Path = asset.GetPathName() };
        CollectNamedFloatArray(asset, "ScalarParameters",
            (name, v) => mpc.Scalars.Add(new ExportScalarMPC { Name = name, Value = v }));
        CollectNamedLinearColorArray(asset, "VectorParameters",
            (name, c) => mpc.Vectors.Add(new ExportVectorMPC { Name = name, Value = c }));
        return mpc;
    }

    private ExportPhysicalMaterial BuildPhysicalMaterial(UObject asset)
    {
        var pm = new ExportPhysicalMaterial { Name = asset.Name, Path = asset.GetPathName() };
        pm.Friction = asset.GetOrDefault("Friction", 0.7f);
        pm.Restitution = asset.GetOrDefault("Restitution", 0.3f);
        pm.Density = asset.GetOrDefault("Density", 1.0f);
        try { if (asset.TryGetValue(out FName st, "SurfaceType")) pm.SurfaceType = st.Text; } catch { /* ignored */ }
        return pm;
    }

    private ExportParticleSystem BuildParticleSystem(UObject asset, List<ExportAssetRef> refs)
    {
        var ps = new ExportParticleSystem { Name = asset.Name, Path = asset.GetPathName() };
        ps.Components = Context.GatherComponents(asset, refs, Materials);
        try
        {
            var emitters = asset.GetOrDefault<FPackageIndex[]>("Emitters", []);
            foreach (var ePkg in emitters)
            {
                if (ePkg is null || ePkg.IsNull) continue;
                var emitter = ePkg.Load();
                if (emitter is null) continue;
                var pe = new ExportParticleEmitter { Name = emitter.Name };
                Context.GatherDependencies(emitter, refs, Materials);
                try
                {
                    var modules = emitter.GetOrDefault<FPackageIndex[]>("LODModules", []) ?? emitter.GetOrDefault<FPackageIndex[]>("Modules", []);
                    foreach (var mPkg in modules)
                    {
                        if (mPkg is null || mPkg.IsNull) continue;
                        var u = mPkg.Load();
                        if (u is null) continue;
                        if (u.TryGetValue(out FPackageIndex matIdx, "Material") && !matIdx.IsNull)
                        {
                            var mat = matIdx.Load<UMaterialInterface>();
                            var expMat = Context.Material(mat, Materials.Count);
                            if (expMat is not null) { Materials.Add(expMat); pe.Materials.Add(expMat); }
                        }
                    }
                }
                catch { /* ignored */ }
                ps.Emitters.Add(pe);
            }
        }
        catch { /* ignored */ }
        ps.ReferencedAssets = refs;
        return ps;
    }

    private ExportSkeleton BuildSkeleton(UObject asset, List<ExportAssetRef> refs)
    {
        var sk = new ExportSkeleton { Name = asset.Name, Path = asset.GetPathName() };
        try
        {
            if (asset is USkeleton skeleton) sk.BinaryPath = Context.Export(skeleton);
        }
        catch { /* ignored */ }
        try
        {
            var sockets = asset.GetOrDefault<FPackageIndex[]>("Sockets", []);
            foreach (var s in sockets)
            {
                if (s is null || s.IsNull) continue;
                var sock = s.Load();
                if (sock is null) continue;
                sk.Sockets.Add(new ExportSkeletonSocket
                {
                    Name = sock.Name,
                    ParentBone = sock.GetOrDefault<FName?>("SocketBoneName")?.Text ?? string.Empty,
                    RelativeLocation = sock.GetOrDefault("RelativeLocation", FVector.ZeroVector),
                    RelativeRotation = sock.GetOrDefault("RelativeRotation", FRotator.ZeroRotator),
                    RelativeScale = sock.GetOrDefault("RelativeScale", FVector.OneVector)
                });
            }
        }
        catch { /* ignored */ }
        return sk;
    }

    private ExportBlueprint BuildBlueprint(UObject asset, string classType, List<ExportAssetRef> refs)
    {
        var bp = new ExportBlueprint { Name = asset.Name, Path = asset.GetPathName(), ClassType = classType };
        try
        {
            if (asset is UBlueprintGeneratedClass bpgc && bpgc.SuperStruct is { IsNull: false } ss && !ss.IsNull)
                bp.ParentClass = ss.Name;
        }
        catch { /* ignored */ }
        bp.Components = Context.GatherComponents(asset, refs, Materials);
        bp.ReferencedAssets = refs;
        return bp;
    }

    private ExportBlendSpace BuildBlendSpace(UObject asset, List<ExportAssetRef> refs)
    {
        var bs = new ExportBlendSpace { Name = asset.Name, Path = asset.GetPathName() };
        try
        {
            if (asset.TryGetValue(out FPackageIndex skelIdx, "Skeleton") && !skelIdx.IsNull)
            {
                var skel = skelIdx.Load<USkeleton>();
                bs.SkeletonPath = Context.Export(skel);
            }
        }
        catch { /* ignored */ }
        try
        {
            // Prefer the strongly-typed FBlendSample[] when available (UBlendSpaceBase.SampleData).
            if (asset is UBlendSpaceBase bsTyped)
            {
                foreach (var s in bsTyped.SampleData)
                {
                    var sample = new ExportBlendSpaceSample
                    {
                        SampleValue = s.SampleValue
                    };
                    try
                    {
                        if (s.Animation is { IsNull: false } animPkg)
                        {
                            var anim = animPkg.Load<UAnimSequence>();
                            sample.AnimationPath = Context.Export(anim);
                        }
                    }
                    catch { /* ignored */ }
                    bs.Samples.Add(sample);
                }
            }
            else if (asset.TryGetValue(out FPropertyTagType[] samples, "SampleData"))
            {
                foreach (var s in samples)
                {
                    var sample = new ExportBlendSpaceSample();
                    if (s?.GetValue<FScriptStruct>() is { StructType: FStructFallback sfb })
                    {
                        foreach (var p in sfb.Properties)
                        {
                            if (p.Name.Text.StartsWith("SampleValue", StringComparison.OrdinalIgnoreCase))
                            {
                                var vecTag = p.Tag?.GetValue<FScriptStruct>();
                                if (vecTag?.StructType is FStructFallback vecFb)
                                {
                                    sample.SampleValue = new FVector(
                                        vecFb.GetOrDefault<float>("X", 0),
                                        vecFb.GetOrDefault<float>("Y", 0),
                                        vecFb.GetOrDefault<float>("Z", 0));
                                }
                                else if (p.Tag?.GetValue<FVector>() is { } fv)
                                {
                                    sample.SampleValue = fv;
                                }
                            }
                            else if (p.Name.Text.Equals("Animation", StringComparison.OrdinalIgnoreCase))
                            {
                                var animPkg = p.Tag?.GetValue<FPackageIndex>();
                                if (animPkg is { IsNull: false })
                                {
                                    var anim = animPkg.Load<UAnimSequence>();
                                    sample.AnimationPath = Context.Export(anim);
                                }
                            }
                        }
                    }
                    bs.Samples.Add(sample);
                }
            }
        }
        catch { /* ignored */ }
        bs.ReferencedAssets = refs;
        return bs;
    }

    private ExportAnimMontage BuildAnimMontage(UObject asset, List<ExportAssetRef> refs)
    {
        var am = new ExportAnimMontage { Name = asset.Name, Path = asset.GetPathName() };
        try
        {
            if (asset.TryGetValue(out FPackageIndex skelIdx, "Skeleton") && !skelIdx.IsNull)
            {
                var skel = skelIdx.Load<USkeleton>();
                am.SkeletonPath = Context.Export(skel);
            }
            if (asset is UAnimSequenceBase seq) am.AnimationPath = Context.Export(seq);
            if (asset is UAnimMontage m)
            {
                am.Length = m.CalculateSequenceLength();
                foreach (var composite in m.SlotAnimTracks)
                {
                    foreach (var segment in composite.AnimTrack.AnimSegments)
                    {
                        var segAnimName = "Unknown";
                        try
                        {
                            if (segment.AnimReference is { IsNull: false } ar && !ar.IsNull)
                            {
                                var segAnim = ar.Load<UAnimSequenceBase>();
                                if (segAnim is not null) segAnimName = segAnim.Name;
                            }
                        }
                        catch { /* ignored */ }
                        am.Sections.Add(new ExportAnimMontageSection
                        {
                            Name = segAnimName,
                            StartTime = segment.StartPos,
                            EndTime = segment.StartPos + segment.AnimEndTime - segment.AnimStartTime
                        });
                    }
                }
            }
        }
        catch { /* ignored */ }
        am.ReferencedAssets = refs;
        return am;
    }

    private ExportLevelSequence BuildLevelSequence(UObject asset, List<ExportAssetRef> refs)
    {
        var ls = new ExportLevelSequence { Name = asset.Name, Path = asset.GetPathName() };
        try
        {
            ls.SequenceStart = 0f;
            ls.SequenceEnd = 5f;
            if (asset.TryGetValue(out FPackageIndex msIdx, "MovieScene") && !msIdx.IsNull)
            {
                var ms = msIdx.Load();
                if (ms is not null) Context.GatherDependencies(ms, refs, Materials);
            }
        }
        catch { /* ignored */ }
        ls.ReferencedAssets = refs;
        return ls;
    }

    private ExportPhysicsAsset BuildPhysicsAsset(UObject asset, List<ExportAssetRef> refs)
    {
        var pa = new ExportPhysicsAsset { Name = asset.Name, Path = asset.GetPathName() };
        try
        {
            if (asset.TryGetValue(out FPackageIndex skelIdx, "PreviewSkeletalMesh") && !skelIdx.IsNull)
            {
                var sm = skelIdx.Load<USkeletalMesh>();
                if (sm is not null && sm.Skeleton.TryLoad(out USkeleton skel))
                    pa.SkeletonPath = Context.Export(skel);
            }
            var bodies = asset.GetOrDefault<FPackageIndex[]>("Bodies", []);
            if (bodies.Length == 0) bodies = asset.GetOrDefault<FPackageIndex[]>("SkeletalBodySetups", []);
            foreach (var b in bodies)
            {
                if (b is null || b.IsNull) continue;
                var body = b.Load();
                if (body is null) continue;
                pa.Bodies.Add(new ExportPhysicsBody
                {
                    BoneName = body.GetOrDefault<FName?>("BoneName")?.Text ?? string.Empty,
                    Mass = body.GetOrDefault("Mass", 1.0f)
                });
            }
        }
        catch { /* ignored */ }
        pa.ReferencedAssets = refs;
        return pa;
    }

    private ExportCurveTable BuildCurveTable(UObject asset, List<ExportAssetRef> refs)
    {
        var ct = new ExportCurveTable { Name = asset.Name, Path = asset.GetPathName() };
        Context.GatherDependencies(asset, refs);
        return ct;
    }

    private ExportDataAsset BuildDataAsset(UObject asset, List<ExportAssetRef> refs)
    {
        var da = new ExportDataAsset
        {
            Name = asset.Name,
            Path = asset.GetPathName(),
            Class = asset.ExportType
        };
        try
        {
            foreach (var p in asset.Properties)
            {
                try { da.Properties[p.Name.Text] = SerializeValue(p.Tag); } catch { /* ignored */ }
            }
        }
        catch { /* ignored */ }
        da.ReferencedAssets = refs;
        return da;
    }

    private ExportGeneric BuildGeneric(UObject asset, List<ExportAssetRef> refs)
    {
        var g = new ExportGeneric { Name = asset.Name, Path = asset.GetPathName(), Class = asset.ExportType };
        try
        {
            foreach (var p in asset.Properties)
            {
                try { g.Properties[p.Name.Text] = SerializeValue(p.Tag); } catch { /* ignored */ }
            }
        }
        catch { /* ignored */ }
        g.ReferencedAssets = refs;
        return g;
    }

    // -------------------------------------------------------------------------
    // Serialization helpers — operate on FPropertyTagType using GetValue<T>
    // instead of direct pattern matching against the abstract base class.
    // -------------------------------------------------------------------------

    private static object SerializeValue(FPropertyTagType? v)
    {
        if (v is null) return string.Empty;
        if (v.GetValue<FName>() is { } n) return n.Text;
        if (v.GetValue<FLinearColor>() is { } c) return new { R = c.R, G = c.G, B = c.B, A = c.A };
        if (v.GetValue<FVector>() is { } vec) return new { X = vec.X, Y = vec.Y, Z = vec.Z };
        if (v.GetValue<FVector2D>() is { } v2) return new { X = v2.X, Y = v2.Y };
        if (v.GetValue<FRotator>() is { } r) return new { Pitch = r.Pitch, Yaw = r.Yaw, Roll = r.Roll };
        var pkg = v.GetValue<FPackageIndex>();
        if (pkg is not null && !pkg.IsNull) return pkg.Name;
        var soft = v.GetValue<FSoftObjectPath>();
        if (soft is not null && !soft.AssetPathName.IsNone) return soft.AssetPathName.Text;
        var gen = v.GenericValue;
        return gen switch
        {
            int or float or double or bool or string => gen,
            null => string.Empty,
            FScriptStruct ss => SerializeStruct(ss),
            System.Collections.IEnumerable arr => SerializeEnumerable(arr),
            _ => gen.ToString() ?? string.Empty
        };
    }

    private static Dictionary<string, object> SerializeStruct(FScriptStruct ss)
    {
        var result = new Dictionary<string, object>();
        if (ss.StructType is FStructFallback fb)
        {
            foreach (var p in fb.Properties) result[p.Name.Text] = SerializeValue(p.Tag);
            return result;
        }
        if (ss.StructType is null) return result;
        foreach (var f in ss.StructType.GetType().GetFields(BindingFlags.Public | BindingFlags.Instance))
        {
            try { result[f.Name] = SerializeClrValue(f.GetValue(ss.StructType)); } catch { /* ignored */ }
        }
        return result;
    }

    private static List<object> SerializeEnumerable(System.Collections.IEnumerable arr)
    {
        var list = new List<object>();
        foreach (var item in arr)
        {
            if (item is FPropertyTagType tag) list.Add(SerializeValue(tag));
            else list.Add(SerializeClrValue(item));
        }
        return list;
    }

    private static object SerializeClrValue(object? v)
    {
        return v switch
        {
            null => string.Empty,
            FName n => n.Text,
            FLinearColor c => new { R = c.R, G = c.G, B = c.B, A = c.A },
            FVector vec => new { X = vec.X, Y = vec.Y, Z = vec.Z },
            FRotator r => new { Pitch = r.Pitch, Yaw = r.Yaw, Roll = r.Roll },
            FPackageIndex pkg when !pkg.IsNull => pkg.Name,
            FSoftObjectPath soft => soft.AssetPathName.Text,
            int or float or double or bool or string => v,
            FScriptStruct ss => SerializeStruct(ss),
            System.Collections.IEnumerable arr and not string => SerializeEnumerableClr(arr),
            _ => v.ToString() ?? string.Empty
        };
    }

    private static List<object> SerializeEnumerableClr(System.Collections.IEnumerable arr)
    {
        var list = new List<object>();
        foreach (var item in arr) list.Add(SerializeClrValue(item));
        return list;
    }

    // Walks an FPropertyTagType array-of-structs property and fires the callback
    // for entries whose ParameterName/DefaultValue fields are both present.
    private static void CollectNamedFloatArray(UObject asset, string propName, Action<string, float> onFloat)
    {
        try
        {
            if (!asset.TryGetValue(out FPropertyTagType[] arr, propName)) return;
            foreach (var tag in arr) CollectNamedFloatFromTag(tag, onFloat);
        }
        catch { /* ignored */ }
    }

    private static void CollectNamedFloatFromTag(FPropertyTagType? tag, Action<string, float> onFloat)
    {
        if (tag is null) return;
        if (tag.GetValue<FScriptStruct>() is { StructType: FStructFallback fb })
        {
            string? name = null;
            float value = 0;
            bool hasValue = false;
            foreach (var p in fb.Properties)
            {
                if (p.Name.Text.Equals("ParameterName", StringComparison.OrdinalIgnoreCase))
                {
                    var n = p.Tag?.GetValue<FName>();
                    if (n.HasValue) name = n.Value.Text;
                }
                else if (p.Name.Text.Equals("DefaultValue", StringComparison.OrdinalIgnoreCase))
                {
                    if (p.Tag?.GetValue<float>() is float fv) { value = fv; hasValue = true; }
                    else if (p.Tag?.GetValue<int>() is int iv) { value = iv; hasValue = true; }
                    else if (p.Tag?.GetValue<double>() is double dv) { value = (float)dv; hasValue = true; }
                }
            }
            if (name is not null && hasValue) onFloat(name, value);
        }
    }

    private static void CollectNamedLinearColorArray(UObject asset, string propName, Action<string, FLinearColor> onVec)
    {
        try
        {
            if (!asset.TryGetValue(out FPropertyTagType[] arr, propName)) return;
            foreach (var tag in arr) CollectNamedLinearColorFromTag(tag, onVec);
        }
        catch { /* ignored */ }
    }

    private static void CollectNamedLinearColorFromTag(FPropertyTagType? tag, Action<string, FLinearColor> onVec)
    {
        if (tag is null) return;
        if (tag.GetValue<FScriptStruct>() is { StructType: FStructFallback fb })
        {
            string? name = null;
            FLinearColor value = default;
            bool hasValue = false;
            foreach (var p in fb.Properties)
            {
                if (p.Name.Text.Equals("ParameterName", StringComparison.OrdinalIgnoreCase))
                {
                    var n = p.Tag?.GetValue<FName>();
                    if (n.HasValue) name = n.Value.Text;
                }
                else if (p.Name.Text.Equals("DefaultValue", StringComparison.OrdinalIgnoreCase))
                {
                    var ss = p.Tag?.GetValue<FScriptStruct>();
                    if (ss?.StructType is FStructFallback vecFb)
                    {
                        value = new FLinearColor(
                            vecFb.GetOrDefault<float>("R", 0),
                            vecFb.GetOrDefault<float>("G", 0),
                            vecFb.GetOrDefault<float>("B", 0),
                            vecFb.GetOrDefault<float>("A", 1));
                        hasValue = true;
                    }
                    else if (p.Tag?.GetValue<FLinearColor>() is { } lc) { value = lc; hasValue = true; }
                }
            }
            if (name is not null && hasValue) onVec(name, value);
        }
    }
}

file static class PrimitiveExportTypeExtensions
{
    public static EExportType AsExportType(this EPrimitiveExportType t) =>
        t switch
        {
            EPrimitiveExportType.NiagaraSystem => EExportType.NiagaraSystem,
            EPrimitiveExportType.NiagaraEmitter => EExportType.NiagaraEmitter,
            EPrimitiveExportType.Actor => EExportType.Actor,
            EPrimitiveExportType.Widget => EExportType.WidgetBlueprint,
            EPrimitiveExportType.DataTable => EExportType.DataTable,
            EPrimitiveExportType.CurveTable => EExportType.CurveTable,
            EPrimitiveExportType.DataAsset => EExportType.DataAsset,
            EPrimitiveExportType.MaterialParameterCollection => EExportType.MaterialParameterCollection,
            EPrimitiveExportType.PhysicalMaterial => EExportType.PhysicalMaterial,
            EPrimitiveExportType.Skeleton => EExportType.Skeleton,
            EPrimitiveExportType.ParticleSystem => EExportType.ParticleSystem,
            EPrimitiveExportType.SoundClass => EExportType.SoundClass,
            EPrimitiveExportType.Blueprint => EExportType.Blueprint,
            EPrimitiveExportType.AnimBlueprint => EExportType.AnimBlueprint,
            EPrimitiveExportType.BlendSpace => EExportType.BlendSpace,
            EPrimitiveExportType.AnimMontage => EExportType.AnimMontage,
            EPrimitiveExportType.LevelSequence => EExportType.LevelSequence,
            EPrimitiveExportType.PhysicsAsset => EExportType.PhysicsAsset,
            _ => EExportType.Generic
        };
}
