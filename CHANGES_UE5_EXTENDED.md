# FortnitePorting — UE5 Extended Asset Support (Massive Upgrade)

This patch massively improves FortnitePorting's Unreal Engine 5 plugin support,
enabling porting of many more asset types and making the entire porting
pipeline more robust.

## New Asset Types Supported

### Binary-exported + reconstructed in-engine:
- **Materials (extended)** — now exports two-sided flag, opacity mask clip value,
  emissive/metallic/roughness/AO/ORM/SRM packed-map awareness, and blend mode
  switches. The plugin creates MaterialInstances with extended PBR scalar/vector
  parameter wiring (TwoSided, UseOpacityMask, UseTranslucency, UseAdditive,
  OpacityMaskClipValue, Emissive, Metallic, Roughness) and an expanded parameter
  name remap table covering Albedo, Metallic, Roughness, AO, Emissive, Detail
  normal, Niagara sprite/flipbook names, and more.

### New JSON-descriptor driven exports (UE5 plugin creates native assets):
- **Niagara Systems / Emitters** — creates `UNiagaraSystem` stubs, recurses
  EmitterHandle / Source emitter references, crawls all renderer module
  materials, and binarily exports all referenced textures, meshes, sounds,
  curves, and MPCs. Emitters are registered as named stubs so users can drop
  into the Niagara editor and finish wiring modules without re-porting.
- **Actors (no functionality)** — traverses the actor's `RootComponent` and
  `InheritableComponentHandler` template hierarchy, reconstructs a full
  component tree in the UE5 editor world with correct transforms, StaticMesh/
  SkeletalMesh wiring, material overrides, light components (Point/Spot/
  Directional/Sky), ParticleSystem, Niagara, Audio, Decal, and ChildActor
  components. **No gameplay logic is ported** — actors are pure visual stubs.
- **Widget Blueprints (UMG / UUserWidget)** — walks the `WidgetTree`, rebuilds
  a UMG WidgetBlueprint with a CanvasPanel root, reconstructs TextBlocks,
  Images (with imported brush textures), Buttons, Borders, VerticalBox/
  HorizontalBox/ScaleBox/Spacer/NamedSlot/CanvasPanel children, and preserves
  position/size metadata. **No widget graph/functionality is ported** — only
  the visual tree.
- **Data Tables** — creates `UDataTable` assets with a generic fallback
  `FFortnitePortingDummyRow` struct so Blueprints referencing the table can
  resolve the reference; users reassign the correct RowStruct in-editor and
  populate rows from the serialized JSON if needed.
- **Curve Tables** — stub assets created so references resolve.
- **Data Assets (UPrimaryDataAsset / UDataAsset)** — stub assets created with
  the asset's serialized property bag captured to JSON for reference.
- **Material Parameter Collections** — fully populated with scalar/vector
  default values on creation.
- **Physical Materials** — created with Friction, Restitution, Density values.
- **Skeletons** — standalone skeleton binaries exported via UEFormat; socket
  transforms serialized to JSON for the plugin stub when no binary exists.
- **Cascade Particle Systems (UParticleSystem)** — stubs created and wired
  through material dependency resolution.
- **Sound Classes / Sound Mixes** — stubs created so `USoundCue`/`MetaSound`
  references resolve.
- **Blueprint Classes** — actor Blueprint assets created via the editor
  BlueprintFactory (with AActor parent), falling back to a stub NewObject if
  editor modules aren't available so references never dangle.
- **Animation Blueprints** — parented to AnimInstance where possible.
- **Blend Spaces** — stub assets created.
- **Anim Montages** — the binary `.ueanim` is exported and a stub asset created
  to reference it.
- **Level Sequencers** — created via LevelSequenceFactoryNew when available.
- **Physics Assets / Chaos** — stubs created with basic body list.
- **Generic / unknown assets** — captured into a generic descriptor with all
  referenced dependencies binarily exported; the plugin creates a UObject stub
  so no cross-reference is ever lost.

## Pipeline / Robustness Improvements

1. **Automatic Dependency Crawler** (`GatherDependencies`) walks every
   property of every UObject, recursing through FPackageIndex, FSoftObjectPath,
   FPropertyTag arrays, and UScriptStruct fields to discover textures,
   materials, meshes, sounds, skeletons, animations, and other UObjects,
   queuing them for binary export. This means new asset types get full
   dependency resolution "for free" without per-type code.
2. **Two-Pass Import on the UE5 plugin**: textures, sounds, meshes, animations,
   materials (leaf assets) are imported first, then composite assets
   (Niagara, actors, widgets, blueprints, MPCs, etc.) are constructed so all
   their references are already in the asset registry.
3. **Defensive factory resolution** — editor-only factories (Niagara, Widget,
   Blueprint, LevelSequence) are looked up by class path at runtime instead of
   being directly instantiated, so the plugin builds across UE5 versions where
   editor module layout differs.
4. **Fallback asset stubs** — whenever an editor module or factory is missing,
   the plugin falls back to `NewObject<...>` so references always resolve
   instead of silently dropping the asset.
5. **Automatic plugin folder creation** — when an asset's root content folder
   corresponds to a plugin that doesn't yet exist, `FEditorUtils` already
   creates it automatically; this now also works for the new asset types.
6. **Extended material parameter mapping table** — now recognizes Albedo,
   Metallic, Roughness, AO, Emissive, Detail, Weathering, Niagara subimage /
   flipbook / sprite texture names, and roughness-swizling switches for the
   common packed-map layouts (MRO/MRS/SRM/RMA/ORM).
7. **Sound/Animation standalone imports** — sound waves and animation
   sequences can now be imported as top-level exports via the Interchange
   pipeline, not only as embedded dependencies.
8. **Auto asset-type detection** (`ExportSession.DetectExportType`) routes any
   UObject to the correct EExportType, so future UI gestures (drag-and-drop,
   right-click "Export") can port arbitrary assets without hard-coding type
   checks.
9. **JSON sidecars for unsupported types** — any asset type without a binary
   exporter gets a JSON sidecar containing name, class, and asset path so the
   plugin can still stub it.
10. **UAnimMontage binary export** — Montages now go through the AnimExporter
    pipeline alongside UAnimSequences.

## Files Changed

### C# (exporter)
- `FortnitePorting.Exporting/Enums.cs` — new EPrimitiveExportType/EExportType
  members for all new asset types.
- `FortnitePorting.Exporting/ExportSession.cs` — new type routing in
  CreateExport, new DetectExportType helper.
- `FortnitePorting.Exporting/Context/ExportContext.cs` — UAnimMontage binary
  export, JSON sidecar fallback for unknown types, extended extension mapping.
- `FortnitePorting.Exporting/Context/ExportContext.Material.cs` — richer
  material flag capture (TwoSided, Niagara usage flags, OpacityMaskClipValue).
- `FortnitePorting.Exporting/Context/ExportContext.AssetScan.cs` (new) —
  GatherDependencies/GatherComponents/BuildComponent generic crawlers.
- `FortnitePorting.Exporting/Models/ExportAsset.cs` (new) — all new data
  contracts (ExportNiagaraSystem, ExportWidget, ExportActor, ExportDataTable,
  ExportMaterialParameterCollection, ExportPhysicalMaterial, ExportSkeleton,
  ExportParticleSystem, ExportBlendSpace, ExportAnimMontage, ExportLevelSequence,
  ExportPhysicsAsset, ExportBlueprint, ExportDataAsset, ExportGeneric, etc.)
- `FortnitePorting.Exporting/Models/ExportMaterial.cs` — extended PBR flags.
- `FortnitePorting.Exporting/Types/NiagaraExport.cs` (new)
- `FortnitePorting.Exporting/Types/ActorExport.cs` (new)
- `FortnitePorting.Exporting/Types/WidgetExport.cs` (new)
- `FortnitePorting.Exporting/Types/DataExport.cs` (new)
- `FortnitePorting.Exporting/Types/GenericAssetExport.cs` (new) — handles MPC,
  PhysicalMaterial, ParticleSystem, Skeleton, Blueprint, AnimBlueprint,
  BlendSpace, AnimMontage, LevelSequence, PhysicsAsset, CurveTable, DataAsset,
  and Generic payloads in one unified handler.

### UE5 Plugin (C++)
- `Source/FortnitePorting/FortnitePorting.Build.cs` — added dependencies on
  Niagara, UMG, AssetTools, LevelSequence, MovieScene, PhysicsCore, and all
  required editor modules (NiagaraEditor, UMGEditor, Kismet/KismetCompiler,
  LevelSequenceEditor, BlueprintGraph, etc.).
- `Source/FortnitePorting/Public/Processing/Enums.h` — synced with C# enums.
- `Source/FortnitePorting/Public/Processing/ImportContext.h` — new importer
  declarations + FPendingComponent helper struct.
- `Source/FortnitePorting/Public/Utilities/JsonWrapper.h` — FVector2D and
  FLinearColor template specialisations.
- `Source/FortnitePorting/Private/Processing/ImportContext.cpp` — massive
  extension: new import passes (two-pass: leaves then composites), importers
  for Niagara, Actors, Widgets, DataTables, MPCs, PhysicalMaterials,
  ParticleSystems, Skeletons, Blueprints, AnimBlueprints, BlendSpaces,
  AnimMontages, LevelSequences, PhysicsAssets, CurveTables, DataAssets,
  SoundClasses, Generic stubs, plus component/actor factory helpers and
  runtime factory-class resolution for cross-UE-version compatibility.
- `Source/FortnitePorting/Private/Processing/MaterialMappings.cpp` —
  significantly expanded parameter-name remapping table for robust material
  recreation.

## Known Limitations

- Blueprint/Widget/AnimBlueprint graphs (event graphs, function graphs,
  Blueprint VM bytecode) are **not** reconstructed. Only visual components,
  parent class, and widget trees are created. This is intentional (the
  request specifies "without functionality").
- Niagara emitter scripts/modules are not decompiled — Fortnite's cooked
  Niagara bytecode is not trivially portable; emitters are stubbed and the
  user completes them in the Niagara editor. Materials and mesh/sound/texture
  dependencies are still wired.
- DataTable rows are not populated because cooked builds strip the row
  UScriptStruct. Tables are created with a generic fallback struct so references
  resolve.
- BlendSpace sample placement, AnimMontage slot animation sections, and
  LevelSequence track keys are best-effort only; the assets exist so blueprints
  compile, but timing/sample details may need manual adjustment.
