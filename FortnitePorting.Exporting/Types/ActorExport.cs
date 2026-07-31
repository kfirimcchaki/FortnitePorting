using System.Collections.Generic;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Assets.Exports.Actor;
using CUE4Parse.UE4.Assets.Objects;
using CUE4Parse.UE4.Objects.Core.Math;
using CUE4Parse.UE4.Objects.Engine;
using CUE4Parse.UE4.Objects.UObject;
using FortnitePorting.CUE4Parse.Extensions;
using FortnitePorting.Exporting.Models;
using FortnitePorting.Exporting.Models.Files.Meta;
using FortnitePorting.Shared.Extensions;

namespace FortnitePorting.Exporting.Types;

/// <summary>
/// Exports a placed AActor (either a Blueprint class or a raw actor). Spawns an
/// actor in the UE5 editor world with all discovered components recreated (meshes,
/// lights, particle systems etc.). NO gameplay logic is ported — the actor is
/// purely a visual/stub container with transforms and assets wired up.
/// </summary>
public class ActorExport : BaseExport
{
    public readonly List<ExportActor> Actors = [];

    public ActorExport(string name, UObject asset, EExportType exportType, ExportDataMeta metaData, IExportFileMeta? fileMeta)
        : base(name, exportType, metaData)
    {
        var actor = new ExportActor
        {
            Name = asset.Name,
            Class = asset.ExportType,
            Path = asset.GetPathName()
        };

        var refs = new List<ExportAssetRef>();
        actor.Components = Context.GatherComponents(asset, refs, actor.OverrideMaterials);

        // If asset is a BlueprintGeneratedClass, pick defaults from ClassDefaultObject.
        UObject? defaults = asset;
        try
        {
            if (asset is UBlueprintGeneratedClass bp && bp.ClassDefaultObject.TryLoad(out var cd) && cd is not null)
            {
                defaults = cd;
                actor.Class = bp.SuperStruct?.Name ?? asset.ExportType;
            }
        }
        catch { /* ignored */ }

        try
        {
            actor.Location = defaults.GetAbsoluteTransformFromRootComponent().Translation;
            actor.Rotation = defaults.GetAbsoluteTransformFromRootComponent().Rotator();
            actor.Scale = defaults.GetAbsoluteTransformFromRootComponent().Scale3D;
        }
        catch
        {
            actor.Location = defaults.GetOrDefault("RelativeLocation", FVector.ZeroVector);
            actor.Rotation = defaults.GetOrDefault("RelativeRotation", FRotator.ZeroRotator);
            actor.Scale = defaults.GetOrDefault("RelativeScale3D", FVector.OneVector);
        }

        // Final dependency sweep.
        Context.GatherDependencies(asset, refs, actor.OverrideMaterials);
        actor.ReferencedAssets = refs;
        Actors.Add(actor);
    }
}
