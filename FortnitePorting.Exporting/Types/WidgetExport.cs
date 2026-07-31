using System.Collections.Generic;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Assets.Objects;
using CUE4Parse.UE4.Assets.Objects.Properties;
using CUE4Parse.UE4.Objects.Core.Math;
using CUE4Parse.UE4.Objects.UObject;
using FortnitePorting.Exporting.Models;
using FortnitePorting.Exporting.Models.Files.Meta;
using FortnitePorting.Shared.Extensions;

namespace FortnitePorting.Exporting.Types;

/// <summary>
/// Exports a UWidgetBlueprint / UUserWidget into a tree of widget descriptors plus
/// all referenced brush textures.
/// </summary>
public class WidgetExport : BaseExport
{
    public readonly List<ExportWidget> Widgets = [];

    public WidgetExport(string name, UObject asset, EExportType exportType, ExportDataMeta metaData, IExportFileMeta? fileMeta)
        : base(name, exportType, metaData)
    {
        var widget = new ExportWidget
        {
            Name = asset.Name,
            Path = asset.GetPathName()
        };

        var refs = new List<ExportAssetRef>();

        UObject? widgetTree = null;
        try
        {
            if (asset.TryGetValue(out FPackageIndex wtIdx, "WidgetTree") && !wtIdx.IsNull)
                widgetTree = wtIdx.Load();
        }
        catch { /* ignored */ }

        if (widgetTree is not null) WalkWidgetTree(widgetTree, widget.WidgetTree, refs);
        else WalkWidgetTree(asset, widget.WidgetTree, refs);

        Context.GatherDependencies(asset, refs);
        widget.ReferencedAssets = refs;

        Widgets.Add(widget);
    }

    private void WalkWidgetTree(UObject tree, List<ExportWidgetChild> outList, List<ExportAssetRef> refs)
    {
        UObject? root = null;
        try
        {
            if (tree.TryGetValue(out FPackageIndex rootIdx, "RootWidget") && !rootIdx.IsNull)
                root = rootIdx.Load();
        }
        catch { /* ignored */ }

        if (root is not null)
        {
            var built = BuildWidget(root, refs);
            if (built is not null) outList.Add(built);
            return;
        }

        try
        {
            if (tree.TryGetValue(out FPackageIndex[] arr, "AllWidgets"))
            {
                foreach (var pkg in arr)
                {
                    if (pkg is null || pkg.IsNull) continue;
                    var w = pkg.Load();
                    if (w is null) continue;
                    var built = BuildWidget(w, refs);
                    if (built is not null) outList.Add(built);
                }
            }
        }
        catch { /* ignored */ }
    }

    private ExportWidgetChild? BuildWidget(UObject widget, List<ExportAssetRef> refs)
    {
        if (widget is null) return null;
        var child = new ExportWidgetChild
        {
            Name = widget.Name,
            Class = widget.ExportType
        };

        try { child.Position = widget.GetOrDefault("Position", new FVector2D(0, 0)); } catch { /* ignored */ }
        try { child.Size = widget.GetOrDefault("Size", new FVector2D(100, 100)); } catch { /* ignored */ }
        try
        {
            if (widget.TryGetValue(out FLinearColor col, "ColorAndOpacity")) child.Color = col;
        }
        catch { /* ignored */ }

        try
        {
            if (widget.ExportType.Contains("Text", System.StringComparison.OrdinalIgnoreCase))
                child.Text = widget.GetOrDefault("Text", string.Empty) ?? string.Empty;
        }
        catch { /* ignored */ }

        // Image widget brush (FSlateBrush struct -> ResourceObject is a UTexture2D/UObject).
        try
        {
            if (widget.Properties != null)
            {
                foreach (var prop in widget.Properties)
                {
                    if (!prop.Name.Text.Equals("Brush")) continue;
                    TryExtractBrushResource(prop.Tag, child, refs);
                }
            }
        }
        catch { /* ignored */ }

        // Slots array on panel widgets (FSlot structs with "Content" -> UWidget).
        try
        {
            if (widget.TryGetValue(out FPropertyTagType[] slots, "Slots"))
            {
                foreach (var slotTag in slots) WalkSlotForContent(slotTag, child, refs);
            }
        }
        catch { /* ignored */ }

        // "Children" array (UVerticalBox / UHorizontalBox etc.).
        try
        {
            if (widget.TryGetValue(out FPackageIndex[] kids, "Children"))
            {
                foreach (var k in kids)
                {
                    if (k is null || k.IsNull) continue;
                    var w = k.Load();
                    var built = BuildWidget(w, refs);
                    if (built is not null) child.Children.Add(built);
                }
            }
        }
        catch { /* ignored */ }

        return child;
    }

    private void WalkSlotForContent(FPropertyTagType? slotTag, ExportWidgetChild parent, List<ExportAssetRef> refs)
    {
        if (slotTag is null) return;

        // Case 1: slot is itself an ObjectProperty pointing to a UObject slot.
        if (slotTag.GetValue<FPackageIndex>() is { IsNull: false } pkg)
        {
            var slotObj = pkg.Load();
            if (slotObj is not null && slotObj.TryGetValue(out FPackageIndex contentIdx, "Content") && !contentIdx.IsNull)
            {
                var childWidget = contentIdx.Load();
                var built = BuildWidget(childWidget, refs);
                if (built is not null) parent.Children.Add(built);
            }
            return;
        }

        // Case 2: slot is a StructProperty (FStructFallback) whose Properties include
        // a field "Content" referencing the child widget.
        if (slotTag.GetValue<FScriptStruct>() is { StructType: FStructFallback fb })
        {
            foreach (var p in fb.Properties)
            {
                bool isContentField =
                    p.Name.Text.Equals("Content", System.StringComparison.OrdinalIgnoreCase);
                if (!isContentField) continue;
                var childPkg = p.Tag?.GetValue<FPackageIndex>();
                if (childPkg is null || childPkg.IsNull) continue;
                var childWidget = childPkg.Load();
                if (childWidget is null) continue;
                var built = BuildWidget(childWidget, refs);
                if (built is not null) parent.Children.Add(built);
            }
        }
    }

    private void TryExtractBrushResource(FPropertyTagType? tag, ExportWidgetChild child, List<ExportAssetRef> refs)
    {
        if (tag is null) return;

        // Direct object reference
        if (tag.GetValue<FPackageIndex>() is { IsNull: false } pkg)
        {
            RecordBrushResource(pkg.Load(), child, refs);
            return;
        }

        // Struct FSlateBrush -> look at its properties for a ResourceObject / ResourceObject_Path.
        if (tag.GetValue<FScriptStruct>() is { StructType: FStructFallback fb })
        {
            foreach (var p in fb.Properties)
            {
                var innerPkg = p.Tag?.GetValue<FPackageIndex>();
                if (innerPkg is null || innerPkg.IsNull) continue;
                var res = innerPkg.Load();
                RecordBrushResource(res, child, refs);
            }
        }
    }

    private void RecordBrushResource(UObject? res, ExportWidgetChild child, List<ExportAssetRef> refs)
    {
        if (res is null) return;
        if (res is UTexture tex)
        {
            var texPath = Context.Export(tex);
            child.BrushTexture = new ExportTexture(texPath, tex.SRGB, tex.CompressionSettings);
        }
        else
        {
            refs.Add(new ExportAssetRef { Path = res.GetPathName(), Name = res.Name, Class = res.ExportType });
        }
    }
}
