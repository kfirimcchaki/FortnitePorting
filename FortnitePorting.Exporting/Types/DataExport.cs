using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Assets.Objects;
using CUE4Parse.UE4.Assets.Objects.Properties;
using CUE4Parse.UE4.Objects.Core.Math;
using CUE4Parse.UE4.Objects.UObject;
using FortnitePorting.Exporting.Models;
using FortnitePorting.Exporting.Models.Files.Meta;
using FortnitePorting.Shared.Extensions;

namespace FortnitePorting.Exporting.Types;

public class DataTableExport : BaseExport
{
    public readonly List<ExportDataTable> Tables = [];

    public DataTableExport(string name, UObject asset, EExportType exportType, ExportDataMeta metaData, IExportFileMeta? fileMeta)
        : base(name, exportType, metaData)
    {
        var dt = new ExportDataTable { Name = asset.Name, Path = asset.GetPathName() };

        try
        {
            if (asset.TryGetValue(out FPackageIndex rowStructIdx, "RowStruct") && !rowStructIdx.IsNull)
            {
                var rs = rowStructIdx.Load();
                dt.RowStruct = rs?.Name ?? string.Empty;
            }
        }
        catch { /* ignored */ }

        try
        {
            if (asset.TryGetValue(out FPropertyTagType[] rowMap, "RowMap"))
            {
                foreach (var entry in rowMap)
                {
                    var row = new ExportDataRow();
                    FlattenTagToRow(entry, row);
                    if (!string.IsNullOrWhiteSpace(row.Name)) dt.Rows.Add(row);
                }
            }
        }
        catch { /* ignored */ }

        var refs = new List<ExportAssetRef>();
        Context.GatherDependencies(asset, refs);

        Tables.Add(dt);
    }

    private static void FlattenTagToRow(FPropertyTagType? tag, ExportDataRow row)
    {
        if (tag is null) return;

        // Key/Value pair struct (Key=FName, Value=struct)
        if (tag.GetValue<FScriptStruct>() is { StructType: FStructFallback fb })
        {
            foreach (var p in fb.Properties)
            {
                if (p.Name.Text.Equals("Key", System.StringComparison.OrdinalIgnoreCase))
                {
                    var keyName = p.Tag?.GetValue<FName>();
                    if (keyName is not null) row.Name = keyName.Value.Text;
                }
                else if (p.Name.Text.Equals("Value", System.StringComparison.OrdinalIgnoreCase))
                {
                    FlattenTagToDict(p.Tag, row.Properties);
                }
                else
                {
                    row.Properties[p.Name.Text] = SerializeValue(p.Tag);
                }
            }
        }
    }

    internal static void FlattenTagToDict(FPropertyTagType? tag, Dictionary<string, object> dict)
    {
        if (tag is null) return;
        if (tag.GetValue<FScriptStruct>() is { StructType: FStructFallback fb })
        {
            foreach (var p in fb.Properties) dict[p.Name.Text] = SerializeValue(p.Tag);
            return;
        }
        if (tag.GetValue<FPropertyTagType[]>() is { } arr)
        {
            var list = new List<object>();
            foreach (var item in arr) list.Add(SerializeValue(item));
            dict["_array"] = list;
        }
    }

    internal static object SerializeValue(FPropertyTagType? v)
    {
        if (v is null) return string.Empty;

        if (v.GetValue<FName>() is { } n) return n.Text;
        if (v.GetValue<FLinearColor>() is { } c) return new { R = c.R, G = c.G, B = c.B, A = c.A };
        if (v.GetValue<FVector>() is { } vec) return new { X = vec.X, Y = vec.Y, Z = vec.Z };
        if (v.GetValue<FRotator>() is { } r) return new { Pitch = r.Pitch, Yaw = r.Yaw, Roll = r.Roll };
        var pkg = v.GetValue<FPackageIndex>();
        if (pkg is not null && !pkg.IsNull) return pkg.Name;
        var soft = v.GetValue<FSoftObjectPath>();
        if (soft is not null && !soft.AssetPathName.IsNone) return soft.AssetPathName.Text;

        var s = v.GenericValue;
        return s switch
        {
            int or float or double or bool or string => s,
            null => string.Empty,
            FScriptStruct ss => SerializeStructTag(ss),
            IEnumerable enumerable => SerializeEnumerable(enumerable),
            _ => s.ToString() ?? string.Empty
        };
    }

    private static Dictionary<string, object> SerializeStructTag(FScriptStruct ss)
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

    private static List<object> SerializeEnumerable(IEnumerable arr)
    {
        var list = new List<object>();
        foreach (var item in arr)
        {
            if (item is FPropertyTagType tag) list.Add(SerializeValue(tag));
            else list.Add(SerializeClrValue(item));
        }
        return list;
    }

    internal static object SerializeClrValue(object? v)
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
            FScriptStruct ss => SerializeStructTag(ss),
            IEnumerable arr and not string => SerializeEnumerableClr(arr),
            _ => v.ToString() ?? string.Empty
        };
    }

    private static List<object> SerializeEnumerableClr(IEnumerable arr)
    {
        var list = new List<object>();
        foreach (var item in arr) list.Add(SerializeClrValue(item));
        return list;
    }
}
