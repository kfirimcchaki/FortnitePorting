using System;
using System.Collections;
using System.Collections.Generic;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Assets.Exports.Component;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse.UE4.Assets.Exports.Animation;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.Assets.Exports.Sound;
using CUE4Parse.UE4.Assets.Exports.StaticMesh;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Assets.Objects;
using CUE4Parse.UE4.Assets.Objects.Properties;
using CUE4Parse.UE4.Objects.Core.Math;
using CUE4Parse.UE4.Objects.UObject;
using CUE4Parse.Utils;
using FortnitePorting.CUE4Parse.Extensions;
using FortnitePorting.Exporting.Models;
using FortnitePorting.Shared.Extensions;
using Serilog;

namespace FortnitePorting.Exporting.Context;

public partial class ExportContext
{
    /// <summary>
    /// Walks an arbitrary UObject's properties and extracts any asset references it
    /// can resolve. Textures, materials, meshes, sounds, and other UObjects are
    /// discovered recursively up to <paramref name="maxDepth"/> and their side-effects
    /// (exported binaries, ExportMaterial, ExportTexture) are collected so callers
    /// can populate ReferencedAssets/Materials/Textures lists on any export type.
    /// </summary>
    public void GatherDependencies(
        UObject? obj,
        List<ExportAssetRef> assetRefs,
        List<ExportMaterial>? materials = null,
        HashSet<string>? visited = null,
        int depth = 0,
        int maxDepth = 4)
    {
        if (obj is null || depth > maxDepth) return;
        visited ??= new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        var path = obj.GetPathName();
        if (!visited.Add(path)) return;

        assetRefs.Add(new ExportAssetRef
        {
            Path = path,
            Name = obj.Name,
            Class = obj.ExportType
        });

        try
        {
            switch (obj)
            {
                case UTexture texture:
                    Export(texture);
                    break;
                case UMaterialInterface mat:
                    materials?.AddIfNotNull(Material(mat, materials.Count));
                    break;
                case UStaticMesh sm:
                    Export(sm);
                    break;
                case USkeletalMesh sk:
                    Export(sk);
                    if (sk.Skeleton.TryLoad(out USkeleton skel) && skel is not null)
                        GatherDependencies(skel, assetRefs, materials, visited, depth + 1, maxDepth);
                    break;
                case USkeleton skeleton:
                    Export(skeleton);
                    break;
                case UAnimSequenceBase anim:
                    Export(anim);
                    break;
                case USoundWave snd:
                    Export(snd);
                    break;
            }
        }
        catch (Exception e)
        {
            Log.Warning(e, "GatherDependencies: failed to binarily export sub-asset {0}", path);
        }

        if (obj.Properties is not null)
        {
            foreach (var prop in obj.Properties)
            {
                try { RecurseTag(prop.Tag, assetRefs, materials, visited, depth, maxDepth); }
                catch (Exception e)
                {
                    Log.Warning(e, "GatherDependencies: failed to crawl property {0} on {1}", prop.Name.Text, path);
                }
            }
        }

        try
        {
            if (obj.Template?.Load() is { } tplObj)
                GatherDependencies(tplObj, assetRefs, materials, visited, depth + 1, maxDepth);
        }
        catch { /* ignored */ }
    }

    private void RecurseTag(
        FPropertyTagType? tag,
        List<ExportAssetRef> assetRefs,
        List<ExportMaterial>? materials,
        HashSet<string> visited,
        int depth,
        int maxDepth)
    {
        if (tag is null) return;

        // Use FPropertyTagType.GetValue<T> for strongly-typed extraction. This handles
        // all the concrete FPropertyTagType<T> subclasses (ObjectProperty,
        // SoftObjectProperty, StructProperty, ArrayProperty, MapProperty, SetProperty).
        //
        // Object/soft-object references -> recurse into the loaded UObject.
        if (tag.GetValue<FPackageIndex>() is { IsNull: false } pkg)
        {
            try
            {
                var loaded = pkg.Load();
                if (loaded is not null) GatherDependencies(loaded, assetRefs, materials, visited, depth + 1, maxDepth);
            }
            catch { /* ignored */ }
            return;
        }

        if (tag.GetValue<FSoftObjectPath?>() is { AssetPathName.IsNone: false } soft)
        {
            try
            {
                if (soft.TryLoad(out var loaded) && loaded is not null)
                    GatherDependencies(loaded, assetRefs, materials, visited, depth + 1, maxDepth);
            }
            catch { /* ignored */ }
            return;
        }

        // Arrays (UScriptArray wraps a List<FPropertyTagType>).
        if (tag.GetValue<FPropertyTagType[]>() is { } arr)
        {
            foreach (var t in arr) RecurseTag(t, assetRefs, materials, visited, depth, maxDepth);
            return;
        }

        // UScriptArray.Properties list
        if (tag.GetValue<UScriptArray>() is { } scriptArr)
        {
            foreach (var t in scriptArr.Properties) RecurseTag(t, assetRefs, materials, visited, depth, maxDepth);
            return;
        }

        // Structs (FScriptStruct -> IUStruct). We unwrap to FStructFallback when possible
        // so we can recurse into its FPropertyTag children.
        if (tag.GetValue<FScriptStruct>() is { } ss)
        {
            WalkStructValue(ss.StructType, assetRefs, materials, visited, depth, maxDepth);
            return;
        }

        // Maps (Dictionary<FPropertyTagType, FPropertyTagType?>)
        if (tag.GetValue<UScriptMap>() is { } map)
        {
            foreach (var (k, v) in map.Properties)
            {
                RecurseTag(k, assetRefs, materials, visited, depth, maxDepth);
                if (v is not null) RecurseTag(v, assetRefs, materials, visited, depth, maxDepth);
            }
            return;
        }

        // Sets (List<FPropertyTagType>)
        if (tag.GetValue<UScriptSet>() is { } set)
        {
            foreach (var e in set.Properties) RecurseTag(e, assetRefs, materials, visited, depth, maxDepth);
            return;
        }
    }

    private void WalkStructValue(
        object? value,
        List<ExportAssetRef> assetRefs,
        List<ExportMaterial>? materials,
        HashSet<string> visited,
        int depth,
        int maxDepth)
    {
        if (value is null) return;

        if (value is FStructFallback fallback)
        {
            if (fallback.Properties is null) return;
            foreach (var prop in fallback.Properties)
            {
                try { RecurseTag(prop.Tag, assetRefs, materials, visited, depth, maxDepth); }
                catch { /* ignored */ }
            }
            return;
        }

        // Reflection fallback for strongly-typed structs (FVector, FLinearColor etc.)
        try
        {
            foreach (var field in value.GetType().GetFields(System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance))
            {
                try
                {
                    var fv = field.GetValue(value);
                    WalkClrValue(fv, assetRefs, materials, visited, depth, maxDepth);
                }
                catch { /* ignored */ }
            }
        }
        catch { /* ignored */ }
    }

    private void WalkClrValue(
        object? fv,
        List<ExportAssetRef> assetRefs,
        List<ExportMaterial>? materials,
        HashSet<string> visited,
        int depth,
        int maxDepth)
    {
        if (fv is null) return;
        switch (fv)
        {
            case FPackageIndex pkg when !pkg.IsNull:
                try
                {
                    var loaded = pkg.Load();
                    if (loaded is not null) GatherDependencies(loaded, assetRefs, materials, visited, depth + 1, maxDepth);
                }
                catch { /* ignored */ }
                break;
            case FSoftObjectPath soft when !soft.AssetPathName.IsNone:
                try
                {
                    if (soft.TryLoad(out var loaded) && loaded is not null)
                        GatherDependencies(loaded, assetRefs, materials, visited, depth + 1, maxDepth);
                }
                catch { /* ignored */ }
                break;
            case FPropertyTagType tag:
                RecurseTag(tag, assetRefs, materials, visited, depth, maxDepth);
                break;
            case IEnumerable enumerable when fv is not string:
                foreach (var item in enumerable)
                    WalkClrValue(item, assetRefs, materials, visited, depth, maxDepth);
                break;
        }
    }

    public List<ExportComponent> GatherComponents(UObject? actorOrBlueprint, List<ExportAssetRef> refs, List<ExportMaterial> materials)
    {
        var result = new List<ExportComponent>();
        if (actorOrBlueprint is null) return result;

        UObject? rootComponent = null;
        try { rootComponent = actorOrBlueprint.GetOrDefault<FPackageIndex?>("RootComponent")?.Load(); }
        catch { /* ignored */ }

        try
        {
            if (actorOrBlueprint.TryGetValue(out UObject ich, "InheritableComponentHandler"))
            {
                var records = ich.GetOrDefault("Records", Array.Empty<FStructFallback>());
                foreach (var record in records)
                {
                    var tmpl = record.GetOrDefault<FPackageIndex?>("ComponentTemplate")?.Load();
                    if (tmpl is not null)
                    {
                        var comp = BuildComponent(tmpl, refs, materials);
                        if (comp is not null) result.Add(comp);
                    }
                }
            }
        }
        catch { /* ignored */ }

        if (rootComponent is not null)
        {
            var built = BuildComponent(rootComponent, refs, materials);
            if (built is not null) result.Add(built);
        }

        try
        {
            foreach (var propName in new[] { "BlueprintCreatedComponents", "Components", "NativeComponents", "InstanceComponents" })
            {
                var arr = actorOrBlueprint.GetOrDefault<FPackageIndex[]>(propName, []);
                foreach (var pkg in arr)
                {
                    if (pkg is null || pkg.IsNull) continue;
                    var comp = pkg.Load();
                    if (comp is null) continue;
                    var built = BuildComponent(comp, refs, materials);
                    if (built is not null) result.Add(built);
                }
            }
        }
        catch { /* ignored */ }

        try
        {
            if (actorOrBlueprint.TryGetValue(out UObject scs, "SimpleConstructionScript"))
            {
                var nodes = scs.GetOrDefault("AllNodes", Array.Empty<UObject>());
                foreach (var node in nodes)
                {
                    if (node is null) continue;
                    var tmpl = node.GetOrDefault<UObject?>("ComponentTemplate");
                    if (tmpl is null) continue;
                    var built = BuildComponent(tmpl, refs, materials);
                    if (built is not null) result.Add(built);
                }
            }
        }
        catch { /* ignored */ }

        return result;
    }

    public ExportComponent? BuildComponent(UObject component, List<ExportAssetRef> refs, List<ExportMaterial> materials)
    {
        if (component is null) return null;
        try { GatherDependencies(component, refs, materials, null, 0, 2); }
        catch { /* ignored */ }

        var ec = new ExportComponent
        {
            Name = component.Name,
            Class = component.ExportType,
            SourceObject = component
        };

        try
        {
            ec.RelativeLocation = component.GetOrDefault("RelativeLocation", FVector.ZeroVector);
            ec.RelativeRotation = component.GetOrDefault("RelativeRotation", FRotator.ZeroRotator);
            ec.RelativeScale3D = component.GetOrDefault("RelativeScale3D", FVector.OneVector);
        }
        catch { /* ignored */ }

        try
        {
            FPackageIndex? meshRef = null;
            if (component.TryGetValue(out FPackageIndex smi, "StaticMesh")) meshRef = smi;
            else if (component.TryGetValue(out FPackageIndex skmi, "SkeletalMesh")) meshRef = skmi;
            else if (component.TryGetValue(out FPackageIndex mi, "Mesh")) meshRef = mi;

            if (meshRef is { IsNull: false } && meshRef.TryLoad(out var meshObj) && meshObj is not null)
            {
                ec.Properties["Mesh"] = meshObj.GetPathName();
                GatherDependencies(meshObj, refs, materials);
            }
        }
        catch { /* ignored */ }

        try
        {
            var overrideMats = component.GetOrDefault<FPackageIndex[]>("OverrideMaterials", []);
            for (var i = 0; i < overrideMats.Length; i++)
            {
                var pkg = overrideMats[i];
                if (pkg is null || pkg.IsNull) continue;
                var mat = pkg.Load<UMaterialInterface>();
                if (mat is null) continue;
                var expMat = Material(mat, i);
                if (expMat is not null) ec.Materials.Add(expMat);
            }
        }
        catch { /* ignored */ }

        void AddChildrenFrom(string propName)
        {
            try
            {
                var kids = component.GetOrDefault<FPackageIndex[]>(propName, []);
                foreach (var k in kids)
                {
                    if (k is null || k.IsNull) continue;
                    var childComp = k.Load();
                    if (childComp is null) continue;
                    var builtChild = BuildComponent(childComp, refs, materials);
                    if (builtChild is not null) ec.Children.Add(builtChild);
                }
            }
            catch { /* ignored */ }
        }

        AddChildrenFrom("AttachChildren");
        AddChildrenFrom("Children");

        return ec;
    }
}
