using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.Utils;
using FortnitePorting.Exporting.Custom;
using FortnitePorting.Exporting.Models;
using FortnitePorting.Exporting.Models.Files;
using FortnitePorting.Exporting.Models.Files.Meta;
using FortnitePorting.Exporting.Styles;
using FortnitePorting.Exporting.Types;
using FortnitePorting.Models;

namespace FortnitePorting.Exporting;

public class ExportSession
{
    public ExportDataMeta Meta { get; }

    public Func<string, Stream>? OpenCustomAssetResource { get; }

    public ExportSession(ExportDataMeta meta, Func<string, Stream>? openCustomAssetResource = null)
    {
        Meta = meta;
        OpenCustomAssetResource = openCustomAssetResource;
    }

    public async Task<ExportData> RunAsync(IEnumerable<BaseExport> exports)
    {
        var list = new List<BaseExport>();
        foreach (var export in exports)
        {
            list.Add(export);
            if (Meta.CancellationToken.IsCancellationRequested) break;
        }

        foreach (var export in list)
            await export.WaitForExports();

        return new ExportData
        {
            MetaData = Meta,
            Exports = list.ToArray()
        };
    }

    public async Task<ExportData> RunAsync(Func<IEnumerable<BaseExport>> exportFunction)
        => await RunAsync(exportFunction());

    public BaseExport CreateExport(
        string displayName,
        UObject asset,
        EExportType exportType,
        ExportStyleBase[] styles,
        IExportFileMeta? fileMeta = null)
    {
        var primitiveType = exportType.PrimitiveType;
        return primitiveType switch
        {
            EPrimitiveExportType.Mesh => new MeshExport(displayName, asset, styles, exportType, Meta, fileMeta),
            EPrimitiveExportType.Texture => new TextureExport(displayName, asset, exportType, Meta, fileMeta),
            EPrimitiveExportType.Sound => new SoundExport(displayName, asset, exportType, Meta, fileMeta),
            EPrimitiveExportType.Animation => new AnimExport(displayName, asset, styles, exportType, Meta, fileMeta),
            EPrimitiveExportType.Font => new FontExport(displayName, asset, exportType, Meta, fileMeta),
            EPrimitiveExportType.PoseAsset => new PoseAssetExport(displayName, asset, exportType, Meta, fileMeta),
            EPrimitiveExportType.Material => new MaterialExport(displayName, asset, exportType, Meta, fileMeta),

            // ===== UE5 EXTENDED EXPORTS =====
            EPrimitiveExportType.NiagaraSystem or EPrimitiveExportType.NiagaraEmitter =>
                new NiagaraExport(displayName, asset, exportType, Meta, fileMeta),
            EPrimitiveExportType.Actor =>
                new ActorExport(displayName, asset, exportType, Meta, fileMeta),
            EPrimitiveExportType.Widget =>
                new WidgetExport(displayName, asset, exportType, Meta, fileMeta),
            EPrimitiveExportType.DataTable =>
                new DataTableExport(displayName, asset, exportType, Meta, fileMeta),

            // Catch-all generic handler covers: CurveTable, DataAsset, MPC,
            // PhysicalMaterial, Skeleton, ParticleSystem, SoundClass, Blueprint,
            // AnimBlueprint, BlendSpace, AnimMontage, LevelSequence, PhysicsAsset,
            // StaticMeshActor and unknown/Generic types.
            EPrimitiveExportType.CurveTable or
            EPrimitiveExportType.DataAsset or
            EPrimitiveExportType.MaterialParameterCollection or
            EPrimitiveExportType.PhysicalMaterial or
            EPrimitiveExportType.Skeleton or
            EPrimitiveExportType.ParticleSystem or
            EPrimitiveExportType.SoundClass or
            EPrimitiveExportType.Blueprint or
            EPrimitiveExportType.AnimBlueprint or
            EPrimitiveExportType.BlendSpace or
            EPrimitiveExportType.AnimMontage or
            EPrimitiveExportType.StaticMeshActor or
            EPrimitiveExportType.LevelSequence or
            EPrimitiveExportType.PhysicsAsset or
            EPrimitiveExportType.Generic =>
                new GenericAssetExport(displayName, asset, exportType, Meta, fileMeta),

            _ => throw new NotImplementedException($"Exporting {primitiveType} assets is not supported yet.")
        };
    }

    public MeshExport CreateCustomMeshExport(string name, MeshDefinition mesh, EExportType exportType)
    {
        if (OpenCustomAssetResource is null)
            throw new InvalidOperationException("CreateCustomMeshExport requires ExportSession.OpenCustomAssetResource.");

        return new MeshExport(name, mesh, OpenCustomAssetResource, exportType, Meta);
    }

    public BaseExport CreateTastyExport() => new TastyExport(Meta);

    public static string FixPath(string path)
    {
        var outPath = path.SubstringBeforeLast(".");
        var extension = path.SubstringAfterLast(".");
        if (extension.Equals("umap") && outPath.Contains("_Generated_"))
        {
            outPath += "." + path.SubstringBeforeLast("/_Generated").SubstringAfterLast("/");
        }

        return outPath;
    }

    /// <summary>
    /// Best-effort auto-detection of the EExportType to use for an arbitrary UObject,
    /// covering all new UE5 extended types. This makes it possible to right-click or
    /// drag-and-drop any asset and get sensible porting without manual type selection.
    /// </summary>
    public static EExportType DetectExportType(UObject asset)
    {
        if (asset is null) return EExportType.None;
        var t = asset.ExportType.ToLowerInvariant();

        // Well-known UE5 types mapped to our new extended exports:
        if (t.Contains("niagarasystem")) return EExportType.NiagaraSystem;
        if (t.Contains("niagaraemitter")) return EExportType.NiagaraEmitter;
        if (t == "staticmeshactor" || t == "pointlight" || t == "spotlight" || t == "directionallight" || t.Contains("skyLight"))
            return EExportType.Actor;
        if (t.Contains("widgetblueprint") || t.Contains("userwidget")) return EExportType.WidgetBlueprint;
        if (t == "datatable") return EExportType.DataTable;
        if (t == "curvetable" || t.Contains("curve")) return EExportType.CurveTable;
        if (t.EndsWith("dataasset") || t == "primarydataasset") return EExportType.DataAsset;
        if (t == "materialparametercollection") return EExportType.MaterialParameterCollection;
        if (t.StartsWith("physicalmaterial")) return EExportType.PhysicalMaterial;
        if (t == "skeleton") return EExportType.Skeleton;
        if (t == "particlesystem") return EExportType.ParticleSystem;
        if (t == "soundclass" || t == "soundmix" || t == "soundcue") return EExportType.SoundClass;
        if (t == "animblueprintgeneratedclass" || t.Contains("animblueprint")) return EExportType.AnimBlueprint;
        if (t == "blueprintgeneratedclass" || t == "blueprint") return EExportType.Blueprint;
        if (t.Contains("blendspace") || t.Contains("aimoffset")) return EExportType.BlendSpace;
        if (t == "animmontage") return EExportType.AnimMontage;
        if (t == "levelsequence") return EExportType.LevelSequence;
        if (t == "physicsasset" || t.Contains("body setup") || t.Contains("physics")) return EExportType.PhysicsAsset;

        // First-class asset types already supported natively:
        if (t.Contains("staticmesh") || t.Contains("skeletalmesh")) return EExportType.Mesh;
        if (t.Contains("texture")) return EExportType.Texture;
        if (t.Contains("soundwave") || t.Contains("soundcue")) return EExportType.Sound;
        if (t.Contains("animsequence") || t.Contains("animstreamable")) return EExportType.Animation;
        if (t.Contains("materialinstance")) return EExportType.MaterialInstance;
        if (t.Contains("material")) return EExportType.Material;
        if (t.Contains("font")) return EExportType.Font;
        if (t.Contains("poseasset")) return EExportType.PoseAsset;
        if (t == "world") return EExportType.World;

        // Fallback: wrap as generic — all dependencies will still be exported and
        // the UE5 plugin will stub the asset so cross-references resolve.
        return EExportType.Generic;
    }
}
