using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Assets.Objects;
using CUE4Parse.UE4.Assets.Objects.Properties;
using CUE4Parse.UE4.Objects.UObject;
using FortnitePorting.CUE4Parse.Extensions;
using FortnitePorting.Exporting.Models;
using FortnitePorting.Exporting.Models.Files.Meta;
using FortnitePorting.Shared.Extensions;

namespace FortnitePorting.Exporting.Types;

public class NiagaraExport : BaseExport
{
    public readonly ExportNiagaraSystem System = new();
    public readonly List<ExportMaterial> Materials = [];
    public readonly List<ExportTexture> Textures = [];

    public NiagaraExport(string name, UObject asset, EExportType exportType, ExportDataMeta metaData, IExportFileMeta? fileMeta)
        : base(name, exportType, metaData)
    {
        var refs = new List<ExportAssetRef>();
        System.Name = asset.Name;
        System.Path = asset.GetPathName();
        System.bLooping = asset.GetOrDefault("bLooping", true);
        System.WarmupTime = asset.GetOrDefault("WarmupTime", 0f);

        var emitters = new List<UObject>();

        void AddEmitterFromProperty(string propName)
        {
            try
            {
                if (asset.TryGetValue(out FPackageIndex[] arr, propName))
                {
                    foreach (var pkg in arr)
                    {
                        if (pkg is null || pkg.IsNull) continue;
                        var loaded = pkg.Load();
                        if (loaded is not null) emitters.Add(loaded);
                    }
                }
                else if (asset.TryGetValue(out FPropertyTagType[] tags, propName))
                {
                    foreach (var tag in tags)
                    {
                        var pkg = tag?.GetValue<FPackageIndex>();
                        if (pkg is null || pkg.IsNull) continue;
                        var loaded = pkg.Load();
                        if (loaded is not null) emitters.Add(loaded);
                    }
                }
            }
            catch { /* ignored */ }
        }

        AddEmitterFromProperty("EmitterHandles");
        AddEmitterFromProperty("Emitters");

        if (asset.ExportType.Contains("NiagaraEmitter", global::System.StringComparison.OrdinalIgnoreCase))
        {
            emitters.Clear();
            emitters.Add(asset);
        }

        foreach (var em in emitters)
        {
            var ne = BuildEmitter(em, refs);
            if (ne is not null) System.Emitters.Add(ne);
        }

        System.Components = Context.GatherComponents(asset, refs, Materials);
        Context.GatherDependencies(asset, refs, Materials);
        System.ReferencedAssets = refs;

        foreach (var texParam in Materials.SelectMany(m => m.Textures))
            Textures.AddIfNotNull(texParam.Texture);
    }

    private ExportNiagaraEmitter? BuildEmitter(UObject emitter, List<ExportAssetRef> refs)
    {
        if (emitter is null) return null;
        var ne = new ExportNiagaraEmitter { Name = emitter.Name, Path = emitter.GetPathName() };

        UObject? actual = emitter;
        try
        {
            if (emitter.TryGetValue(out FPackageIndex sourceIdx, "Instance") && !sourceIdx.IsNull)
            {
                var loaded = sourceIdx.Load();
                if (loaded is not null) actual = loaded;
            }
            else if (emitter.TryGetValue(out FSoftObjectPath sourceSoft, "Source") && !sourceSoft.AssetPathName.IsNone)
            {
                var loaded = sourceSoft.LoadOrDefault<UObject>();
                if (loaded is not null) actual = loaded;
            }
        }
        catch { /* ignored */ }

        Context.GatherDependencies(actual ?? emitter, refs, Materials);

        try
        {
            if ((actual ?? emitter).TryGetValue(out FPropertyTagType[] rendererProps, "Renderers"))
            {
                foreach (var r in rendererProps)
                    ScanTagForMaterials(r, ne);
            }
        }
        catch { /* ignored */ }

        ne.ReferencedAssets = refs;
        return ne;
    }

    private void ScanTagForMaterials(FPropertyTagType? tag, ExportNiagaraEmitter ne)
    {
        if (tag is null) return;

        if (tag.GetValue<FPackageIndex>() is { IsNull: false } pkg)
        {
            var mat = pkg.Load<UMaterialInterface>();
            if (mat is not null) AddMaterialFrom(mat, ne);
            return;
        }

        if (tag.GetValue<FSoftObjectPath?>() is { AssetPathName.IsNone: false } soft)
        {
            var mat = soft.LoadOrDefault<UMaterialInterface>();
            if (mat is not null) AddMaterialFrom(mat, ne);
            return;
        }

        if (tag.GetValue<FScriptStruct>() is { StructType: FStructFallback fb })
        {
            foreach (var p in fb.Properties)
                ScanTagForMaterials(p.Tag, ne);
            return;
        }

        if (tag.GetValue<FPropertyTagType[]>() is { } arr)
        {
            foreach (var t in arr) ScanTagForMaterials(t, ne);
            return;
        }

        if (tag.GetValue<UScriptArray>() is { } sa)
        {
            foreach (var t in sa.Properties) ScanTagForMaterials(t, ne);
        }
    }

    private void AddMaterialFrom(UMaterialInterface mat, ExportNiagaraEmitter ne)
    {
        var expMat = Context.Material(mat, Materials.Count);
        if (expMat is null) return;
        if (Materials.Any(m => m.Hash == expMat.Hash)) return;
        Materials.Add(expMat);
        ne.Materials.Add(expMat);
    }
}
