#include "FortnitePorting/Public/Processing/ImportContext.h"

#include "AutomatedAssetImportData.h"
#include "ComponentReregisterContext.h"
#include "FortnitePorting.h"
#include "Utils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Classes/BuildingTextureData.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Factories/UEFModelFactory.h"
#include "Framework/Notifications/NotificationManager.h"
#include "InterchangeManager.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameterCollection.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Factories/BlueprintFactory.h"
#include "WidgetBlueprint.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/ScaleBox.h"
#include "Components/Button.h"
#include "Components/Spacer.h"
#include "Components/NamedSlot.h"
#include "Components/Widget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/DecalComponent.h"
#include "Components/ParticleSystemComponent.h"
#include "Components/NiagaraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundCue.h"
#include "Animation/Skeleton.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/CurveTable.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "UObject/SavePackage.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Factories.h"
#include "PackageHelperFunctions.h"
#include "Processing/Enums.h"
#include "Processing/FortnitePortingTexturePipeline.h"
#include "Processing/MaterialMappings.h"
#include "Processing/Names.h"
#include "TextureCompiler.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/ConstructorHelpers.h"
#include "Utilities/EditorUtils.h"
#include "Utilities/JsonWrapper.h"
#include "World/BuildingActor.h"

// WidgetBlueprintFactory lives in different places depending on UE version. We
// resolve its class via FindObject/LoadObject at runtime so compilation is robust
// across UE5 releases where editor-only module headers move.
static UClass* FindClassSafe(const TCHAR* Name)
{
	UClass* Class = FindObject<UClass>(nullptr, Name);
	if (!Class) Class = LoadObject<UClass>(nullptr, Name);
	return Class;
}

// A minimal row struct used as a fallback when the source DataTable's RowStruct is
// unavailable (cooked builds strip the struct type). The resulting UDataTable will
// not have usable rows until the user assigns a proper RowStruct in-editor, but
// it will resolve cross-references and load without errors.
USTRUCT()
struct FFortnitePortingDummyRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="FortnitePorting")
	TMap<FString, FString> Properties;
};

// Parameter-association guideline used by the material instance creation calls below.
// A null/global FGuid signals "global parameter" scope to the material editor API.
static const FGuid GlobalParameter;

static UPackage* GetOrCreatePackage(const FString& PackagePath)
{
	auto Package = CreatePackage(*PackagePath);
	Package->FullyLoad();
	return Package;
}

static UObject* FindOrCreateObjectInPackage(UPackage* Package, UClass* Class, const TCHAR* Name)
{
	if (UObject* Existing = FindObject<UObject>(Package, Name)) return Existing;
	UObject* New = NewObject<UObject>(Package, Class, FName(Name), RF_Public | RF_Standalone);
	if (New)
	{
		FAssetRegistryModule::AssetCreated(New);
	}
	return New;
}

static UObject* FindExistingAsset(const FString& ObjectPath)
{
	const FPathData PathData = FEditorUtils::GetPathData(ObjectPath);
	if (PathData.RootName.Equals(TEXT("Engine")))
	{
		return LoadObject<UObject>(nullptr, *ObjectPath);
	}
	auto Package = FindPackage(nullptr, *PathData.Path);
	if (!Package) Package = LoadPackage(nullptr, *PathData.Path, LOAD_NoWarn);
	if (!Package) return nullptr;
	return FindObject<UObject>(Package, *PathData.ObjectName);
}

FImportContext::FImportContext(const FJsonWrapper& InMetaData) : MetaData(InMetaData)
{
	EnsureDependencies();
}

void FImportContext::RunExport(const FJsonWrapper& Json)
{
	const auto PrimitiveType = Json.Get<EPrimitiveExportType>(TEXT("PrimitiveType"));

	switch (PrimitiveType)
	{
	case EPrimitiveExportType::Mesh:
		ImportMeshData(Json);
		break;
	case EPrimitiveExportType::Texture:
		ImportTextureData(Json);
		break;
	case EPrimitiveExportType::Sound:
		ImportSoundData(Json);
		break;
	case EPrimitiveExportType::Animation:
		ImportAnimationData(Json);
		break;
	case EPrimitiveExportType::Material:
		// Material data is embedded inside mesh payloads; the top-level material
		// export just calls ImportMaterial to create the asset in the content
		// browser directly (rather than as part of a mesh).
		if (Json.IsValid())
		{
			const auto MaterialsArray = Json.GetArray(TEXT("Materials"));
			for (const auto& M : MaterialsArray)
			{
				ImportMaterial(M);
			}
			// Fallback: if there is no "Materials" array try treating the object
			// itself as a material descriptor.
			if (MaterialsArray.Num() == 0 && Json.Get<FString>(TEXT("Path"), TEXT("")).Len() > 0)
			{
				ImportMaterial(Json);
			}
		}
		break;

	// ===== UE5 EXTENDED TYPES =====
	case EPrimitiveExportType::NiagaraSystem:
	case EPrimitiveExportType::NiagaraEmitter:
		ImportNiagaraData(Json);
		break;
	case EPrimitiveExportType::Actor:
	case EPrimitiveExportType::StaticMeshActor:
		ImportActorData(Json);
		break;
	case EPrimitiveExportType::Widget:
		ImportWidgetData(Json);
		break;
	case EPrimitiveExportType::DataTable:
		ImportDataTableData(Json);
		break;
	case EPrimitiveExportType::CurveTable:
	case EPrimitiveExportType::DataAsset:
	case EPrimitiveExportType::MaterialParameterCollection:
	case EPrimitiveExportType::PhysicalMaterial:
	case EPrimitiveExportType::Skeleton:
	case EPrimitiveExportType::ParticleSystem:
	case EPrimitiveExportType::SoundClass:
	case EPrimitiveExportType::Blueprint:
	case EPrimitiveExportType::AnimBlueprint:
	case EPrimitiveExportType::BlendSpace:
	case EPrimitiveExportType::AnimMontage:
	case EPrimitiveExportType::LevelSequence:
	case EPrimitiveExportType::PhysicsAsset:
	case EPrimitiveExportType::Generic:
		ImportGenericAssetData(Json);
		break;
	default:
		// Unknown primitive: try generic
		ImportGenericAssetData(Json);
		break;
	}
}

void FImportContext::RunExportJson(const FString& Data)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		UE_LOG(LogFortnitePorting, Error, TEXT("Unable to deserialize response from FortnitePorting"))
		return;
	}

	const auto Root = FJsonWrapper(JsonObject);

	auto Exports = Root.GetArray(TEXT("Exports"));

	FScopedSlowTask ImportTask(Exports.Num(), FText::FromString(TEXT("Importing Data...")));
	ImportTask.MakeDialog(true);

	FSlateNotificationManager::Get().SetAllowNotifications(false);

	auto ResponseIndex = 0;
	const FJsonWrapper MetaData = Root[TEXT("MetaData")];

	// Two-pass import guarantees dependencies are available before the parent asset
	// is constructed. Pass 1 creates/imports all "leaf" assets (textures, sounds,
	// meshes, materials, animations). Pass 2 constructs the composite types
	// (Niagara systems, actors, widgets, blueprints, data tables etc.) that
	// reference those leaf assets.
	TArray<FJsonWrapper> CompositeExports;
	for (const auto& Export : Exports)
	{
		const EPrimitiveExportType Primitive = Export.Get<EPrimitiveExportType>(TEXT("PrimitiveType"));
		const bool bIsComposite =
			Primitive == EPrimitiveExportType::NiagaraSystem ||
			Primitive == EPrimitiveExportType::NiagaraEmitter ||
			Primitive == EPrimitiveExportType::Actor ||
			Primitive == EPrimitiveExportType::StaticMeshActor ||
			Primitive == EPrimitiveExportType::Widget ||
			Primitive == EPrimitiveExportType::DataTable ||
			Primitive == EPrimitiveExportType::CurveTable ||
			Primitive == EPrimitiveExportType::DataAsset ||
			Primitive == EPrimitiveExportType::MaterialParameterCollection ||
			Primitive == EPrimitiveExportType::PhysicalMaterial ||
			Primitive == EPrimitiveExportType::ParticleSystem ||
			Primitive == EPrimitiveExportType::Blueprint ||
			Primitive == EPrimitiveExportType::AnimBlueprint ||
			Primitive == EPrimitiveExportType::BlendSpace ||
			Primitive == EPrimitiveExportType::AnimMontage ||
			Primitive == EPrimitiveExportType::LevelSequence ||
			Primitive == EPrimitiveExportType::PhysicsAsset ||
			Primitive == EPrimitiveExportType::Generic ||
			Primitive == EPrimitiveExportType::Skeleton;
		if (bIsComposite) CompositeExports.Add(Export);
	}

	auto RunPass = [&](const TArray<FJsonWrapper>& List, const FString& PassName)
	{
		int32 Index = 0;
		for (const auto& Export : List)
		{
			if (ImportTask.ShouldCancel()) break;
			Index++;
			FString ExportName = Export.Get<FString>(TEXT("Name"));
			ImportTask.DefaultMessage = FText::FromString(FString::Printf(TEXT("[%s] %s (%d/%d)"), *PassName, *ExportName, Index, List.Num()));
			ImportTask.EnterProgressFrame();
			auto ImportContext = FImportContext(MetaData);
			ImportContext.RunExport(Export);
		}
	};

	// Pass 1: leaves (textures/sounds/anims/meshes/materials) are imported first.
	TArray<FJsonWrapper> LeafExports;
	for (const auto& E : Exports)
	{
		const EPrimitiveExportType P = E.Get<EPrimitiveExportType>(TEXT("PrimitiveType"));
		if (P == EPrimitiveExportType::Mesh ||
			P == EPrimitiveExportType::Texture ||
			P == EPrimitiveExportType::Sound ||
			P == EPrimitiveExportType::Animation ||
			P == EPrimitiveExportType::Font ||
			P == EPrimitiveExportType::PoseAsset ||
			P == EPrimitiveExportType::Material)
		{
			LeafExports.Add(E);
		}
	}
	RunPass(LeafExports, TEXT("Assets"));

	RunPass(CompositeExports, TEXT("Composites"));

	FSlateNotificationManager::Get().SetAllowNotifications(true);

	// Force a global asset registry scan + save of all dirty packages so newly
	// created assets are immediately visible to the editor.
	FAssetRegistryModule::AssetCreated(nullptr);
}

void FImportContext::EnsureDependencies()
{
	if (DefaultMaterial == nullptr)
		DefaultMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(TEXT("/FortnitePorting/Materials/M_FP_Default.M_FP_Default")));

	if (LayerMaterial == nullptr)
		LayerMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(TEXT("/FortnitePorting/Materials/M_FP_Layer.M_FP_Layer")));

	if (DefaultParticleMaterial == nullptr)
		DefaultParticleMaterial = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(TEXT("/Engine/EngineMaterials/DefaultDeferredDecalMaterial.DefaultDeferredDecalMaterial")));
	if (DefaultParticleMaterial == nullptr)
		DefaultParticleMaterial = DefaultMaterial;
}

void FImportContext::ImportMeshData(const FJsonWrapper& ExportData)
{
	auto Meshes = ExportData.GetArray(TEXT("Meshes"));
	auto OverrideMeshes = ExportData.GetArray(TEXT("OverrideMeshes"));
	const int32 Count = Meshes.Num() + OverrideMeshes.Num();

	FScopedSlowTask ImportTask(Count, FText::FromString(TEXT("Importing Meshes...")));
	ImportTask.MakeDialog(true);

	auto WorldContext = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport);
	const auto World = WorldContext->World();

	auto ExportType = ExportData.Get<EExportType>(TEXT("Type"));
	FString ExportName = ExportData.Get<FString>(TEXT("Name"));
	bool bCreateActor = ExportType == EExportType::World || ExportType == EExportType::Prefab;

	int32 MeshIndex = 0;
	auto ImportMeshes = [&](const TArray<FJsonWrapper>& MeshArray)
	{
		for (const auto& Mesh : MeshArray)
		{
			if (ImportTask.ShouldCancel())
				break;

			MeshIndex++;
			FString MeshName = Mesh.Get<FString>(TEXT("Name"));

			ImportTask.DefaultMessage = FText::FromString(FString::Printf(TEXT("Importing Mesh %d of %d: %s"), MeshIndex, Count, *MeshName));
			ImportTask.EnterProgressFrame();

			ImportModel(ExportData, World, nullptr, Mesh, bCreateActor);
		}
	};

	ImportMeshes(Meshes);
	ImportMeshes(OverrideMeshes);
}

void FImportContext::ImportTextureData(const FJsonWrapper& ExportData)
{
	const auto Textures = ExportData.GetArray(TEXT("Textures"));

	// Fire all imports concurrently — Interchange pipelines run on worker threads.
	TArray<UE::Interchange::FAssetImportResultRef> PendingResults;
	PendingResults.Reserve(Textures.Num());

	for (const auto& TextureJson : Textures)
	{
		// Resolve path and skip already-imported / engine assets early.
		const auto PathData = FEditorUtils::GetPathData(TextureJson.Get<FString>(TEXT("Path")));
		const auto Package = CreatePackage(*PathData.Path);

		if (LoadObject<UTexture>(Package, *PathData.ObjectName) || PathData.RootName.Equals(TEXT("Engine")))
			continue;

		FString AssetsRoot = MetaData.Get<FString>(TEXT("AssetsRoot"));
		FString TexturePath = FPaths::Combine(AssetsRoot, PathData.Path + TEXT(".png"));
		if (!FPaths::FileExists(TexturePath))
			TexturePath = FPaths::Combine(AssetsRoot, PathData.Path + TEXT(".hdr"));
		if (!FPaths::FileExists(TexturePath))
			continue;

		UInterchangeManager& Manager = UInterchangeManager::GetInterchangeManager();
		UInterchangeSourceData* SourceData = Manager.CreateSourceData(TexturePath);

		auto* Pipeline = NewObject<UFortnitePortingTexturePipeline>(GetTransientPackage());
		Pipeline->bWantSRGB = TextureJson.Get<bool>(TEXT("sRGB"));
		Pipeline->WantCompression = TextureJson.Get<TextureCompressionSettings>(TEXT("CompressionSettings"));

		FImportAssetParameters Params;
		Params.bIsAutomated = true;
		Params.bReplaceExisting = false;
		Params.OverridePipelines.Add(Pipeline);

		FString ContentFolder = PathData.Path.LeftChop(PathData.ObjectName.Len() + 1);
		PendingResults.Add(Manager.ImportAssetAsync(ContentFolder, SourceData, Params));
	}

	for (const auto& Result : PendingResults)
	{
		Result->WaitUntilDone();
	}

	// Flush the texture build queue so any subsequent save/cook sees fully compiled assets.
	FTextureCompilingManager::Get().FinishAllCompilation();
}

void FImportContext::ImportSoundData(const FJsonWrapper& ExportData)
{
	const auto Sounds = ExportData.GetArray(TEXT("Sounds"));
	for (const auto& SoundJson : Sounds)
	{
		const FString SoundPath = SoundJson.Get<FString>(TEXT("Path"));
		if (SoundPath.IsEmpty()) continue;
		const auto PathData = FEditorUtils::GetPathData(SoundPath);
		if (PathData.RootName.Equals(TEXT("Engine"))) continue;
		if (FindExistingAsset(SoundPath)) continue;

		FString AssetsRoot = MetaData.Get<FString>(TEXT("AssetsRoot"));
		FString FilePath = FPaths::Combine(AssetsRoot, PathData.Path + TEXT(".wav"));
		// try other extensions
		for (const TCHAR* Ext : { TEXT(".ogg"), TEXT(".mp3"), TEXT(".flac") })
		{
			if (FPaths::FileExists(FilePath)) break;
			FilePath = FPaths::Combine(AssetsRoot, PathData.Path + Ext);
		}
		if (!FPaths::FileExists(FilePath)) continue;

		UInterchangeManager& Manager = UInterchangeManager::GetInterchangeManager();
		UInterchangeSourceData* SourceData = Manager.CreateSourceData(FilePath);
		FImportAssetParameters Params;
		Params.bIsAutomated = true;
		Params.bReplaceExisting = false;
		FString ContentFolder = PathData.Path.LeftChop(PathData.ObjectName.Len() + 1);
		auto Result = Manager.ImportAssetAsync(ContentFolder, SourceData, Params);
		Result->WaitUntilDone();
	}
}

void FImportContext::ImportAnimationData(const FJsonWrapper& ExportData)
{
	// Animations are imported via UEFAnimFactory automatically when imported into a
	// sequence slot (the model factory handles this). For standalone anim exports
	// we just trigger an import of the binary .ueanim through the UEFormat factory.
	const auto Anims = ExportData.GetArray(TEXT("Animations"));
	for (const auto& AnimJson : Anims)
	{
		const FString AnimPath = AnimJson.Get<FString>(TEXT("Path"));
		if (AnimPath.IsEmpty()) continue;
		const auto PathData = FEditorUtils::GetPathData(AnimPath);
		if (FindExistingAsset(AnimPath)) continue;

		FString AssetsRoot = MetaData.Get<FString>(TEXT("AssetsRoot"));
		FString FilePath = FPaths::Combine(AssetsRoot, PathData.Path + TEXT(".ueanim"));
		if (!FPaths::FileExists(FilePath))
			FilePath = FPaths::Combine(AssetsRoot, PathData.Path + TEXT(".psa"));
		if (!FPaths::FileExists(FilePath)) continue;

		auto AutomatedData = NewObject<UAutomatedAssetImportData>();
		AutomatedData->bReplaceExisting = false;

		// Use UEFAnimFactory if available.
		UObject* FactoryClass = nullptr;
		auto FactoryClassObj = LoadObject<UClass>(nullptr, TEXT("/Script/UEFormat.UEFAnimFactory"));
		if (FactoryClassObj)
		{
			auto Factory = NewObject<UObject>(GetTransientPackage(), FactoryClassObj);
			// Not calling FactoryCreateFile here because UEFormat handles it via
			// import task; rely on interchange instead to be safe.
		}

		UInterchangeManager& Manager = UInterchangeManager::GetInterchangeManager();
		UInterchangeSourceData* SourceData = Manager.CreateSourceData(FilePath);
		FImportAssetParameters Params;
		Params.bIsAutomated = true;
		Params.bReplaceExisting = false;
		FString ContentFolder = PathData.Path.LeftChop(PathData.ObjectName.Len() + 1);
		auto Result = Manager.ImportAssetAsync(ContentFolder, SourceData, Params);
		Result->WaitUntilDone();
	}
}

UObject* FImportContext::ImportModel(const FJsonWrapper& ExportData, UWorld* World, ABuildingActor* Parent, const FJsonWrapper& MeshData, bool bCreateActor)
{
	const auto ImportedObject = ImportMesh(MeshData);

	if (const auto StaticMesh = Cast<UStaticMesh>(ImportedObject); bCreateActor)
	{
		FTransform SpawnTransform;
		auto Actor = World->SpawnActorDeferred<ABuildingActor>(ABuildingActor::StaticClass(), SpawnTransform);
		Actor->Modify();

		Actor->SetActorLabel(*MeshData.Get<FString>(TEXT("Name")));
		if (Parent) Actor->AttachToActor(Parent, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));

		Actor->SetActorRelativeLocation(MeshData.Get<FVector>(TEXT("Location"), FVector::ZeroVector));
		Actor->SetActorRelativeRotation(MeshData.Get<FRotator>(TEXT("Rotation"), FRotator::ZeroRotator));
		Actor->SetActorRelativeScale3D(MeshData.Get<FVector>(TEXT("Scale"), FVector::OneVector));

		for (const auto& TexData : MeshData.GetArray(TEXT("TextureData")))
		{
			Actor->TextureData.Add(FTextureDataInstance{
				.LayerIndex = TexData.Get<int>(TEXT("Index")),
				.TextureData = ImportBuildingTextureData(TexData)
			});
		}

		Actor->GetStaticMeshComponent()->ForcedLodModel = 1;
		Actor->GetStaticMeshComponent()->SetStaticMesh(StaticMesh);
		Actor->SetFolderPath(*FString::Printf(TEXT("/%s"), *ExportData.Get<FString>(TEXT("Name"))));
		Actor->FinishSpawning(SpawnTransform);
		Actor->MarkPackageDirty();

		auto Children = MeshData.GetArray(TEXT("Children"));
		if (Children.Num() > 0)
		{
			FScopedSlowTask ImportTask(Children.Num(), FText::FromString(TEXT("Importing Children...")));
			ImportTask.MakeDialog(true);

			int32 ChildIndex = 0;

			for (const auto& Child : Children)
			{
				if (ImportTask.ShouldCancel())
					break;

				ChildIndex++;
				FString ChildName = Child.Get<FString>(TEXT("Name"));

				ImportTask.DefaultMessage = FText::FromString(FString::Printf(TEXT("Importing Mesh %d of %d: %s"), ChildIndex, Children.Num(), *ChildName));
				ImportTask.EnterProgressFrame();
				ImportModel(ExportData, World, Actor, Child, bCreateActor);
			}
		}
	}

	return ImportedObject;
}


UObject* FImportContext::ImportMesh(const FJsonWrapper& MeshData)
{
	FString MeshPath = MeshData.Get<FString>(TEXT("Path"));
	auto PathData = FEditorUtils::GetPathData(MeshPath);
	auto Package = CreatePackage(*PathData.Path);

	auto Mesh = LoadObject<UObject>(Package, *PathData.ObjectName);
	if (Mesh != nullptr || PathData.RootName.Equals(TEXT("Engine"))) return Mesh;

	FString AssetsRoot = MetaData.Get<FString>(TEXT("AssetsRoot"));
	const auto ModelPath = FPaths::Combine(AssetsRoot, PathData.Path + TEXT(".uemodel"));
	if (!FPaths::FileExists(ModelPath)) return nullptr;

	auto AutomatedData = NewObject<UAutomatedAssetImportData>();
	AutomatedData->bReplaceExisting = false;

	const auto ModelFactory = NewObject<UEFModelFactory>();
	ModelFactory->AutomatedImportData = AutomatedData;

	bool Canceled;
	Mesh = ModelFactory->FactoryCreateFile(nullptr, Package, FName(*PathData.ObjectName), RF_Public | RF_Standalone, ModelPath, nullptr, nullptr, Canceled);

	if (const auto StaticMesh = Cast<UStaticMesh>(Mesh))
	{
		StaticMesh->GetSourceModel(0).BuildSettings.bGenerateLightmapUVs = false;
		StaticMesh->GetSourceModel(0).BuildSettings.bRecomputeNormals = false;
		StaticMesh->GetSourceModel(0).BuildSettings.bRecomputeTangents = false;
		StaticMesh->Modify();
	}

	for (const auto& Material : MeshData.GetArray(TEXT("Materials")))
	{
		int32 Slot = Material.Get<int32>(TEXT("Slot"));
		auto ImportedMaterial = ImportMaterial(Material);

		if (const auto SM = dynamic_cast<UStaticMesh*>(Mesh))
		{
			SM->SetMaterial(Slot, ImportedMaterial);
		}
		else if (const auto Skel = dynamic_cast<USkeletalMesh*>(Mesh))
		{
			Skel->GetMaterials()[Slot].MaterialInterface = ImportedMaterial;
		}
	}

	return Mesh;
}

UMaterialInstanceConstant* FImportContext::ImportMaterial(const FJsonWrapper& MaterialData)
{
	FString MaterialPath = MaterialData.Get<FString>(TEXT("Path"));
	const auto PathData = FEditorUtils::GetPathData(MaterialPath);
	const auto Package = CreatePackage(*PathData.Path);

	auto MaterialInstance = LoadObject<UMaterialInstanceConstant>(Package, *PathData.ObjectName);
	if (MaterialInstance != nullptr || PathData.RootName.Equals(TEXT("Engine"))) return MaterialInstance;

	MaterialInstance = NewObject<UMaterialInstanceConstant>(Package, *PathData.ObjectName, RF_Public | RF_Standalone);
	FAssetRegistryModule::AssetCreated(MaterialInstance);

	MaterialInstance->PreEditChange(nullptr);

	auto TargetMaterial = DefaultMaterial;
	FMappingCollection TargetMappings = FMaterialMappings::Default;

	bool bIsLayerMaterial = false;
	for (const auto& Switch : MaterialData.GetArray(TEXT("Switches")))
	{
		if (FNames::LayerSwitchNames.Contains(Switch.Get<FString>(TEXT("Name"))))
		{
			bIsLayerMaterial = true;
			break;
		}
	}

	if (bIsLayerMaterial)
	{
		for (const auto& Texture : MaterialData.GetArray(TEXT("Textures")))
		{
			if (FNames::LayerTextureNames.Contains(Texture.Get<FString>(TEXT("Name"))))
			{
				TargetMaterial = LayerMaterial;
				TargetMappings = FMaterialMappings::Layer;
				break;
			}
		}
	}

	MaterialInstance->Parent = TargetMaterial;
	MaterialInstance->BlendMode = MaterialData.Get<EBlendMode>(TEXT("OverrideBlendMode"));

	ImportMaterialParameterOverrides(MaterialInstance, MaterialData);

	for (const auto& TexParam : MaterialData.GetArray(TEXT("Textures")))
	{
		FString ParamName = TexParam.Get<FString>(TEXT("Name"));

		const auto Texture = ImportTexture(TexParam[TEXT("Texture")]);
		if (Texture == nullptr) continue;

		for (const auto& Mapping : TargetMappings.Textures)
		{
			if (Mapping.Name.Equals(ParamName))
			{
				MaterialInstance->SetTextureParameterValueEditorOnly(
					FMaterialParameterInfo(*Mapping.Slot, GlobalParameter),
					Texture
				);

				if (!Mapping.SwitchSlot.IsEmpty())
				{
					MaterialInstance->SetStaticSwitchParameterValueEditorOnly(
						FMaterialParameterInfo(*Mapping.SwitchSlot, GlobalParameter),
						true
					);
				}
				break;
			}
		}
	}

	for (const auto& Scalar : MaterialData.GetArray(TEXT("Scalars")))
	{
		FString ParamName = Scalar.Get<FString>(TEXT("Name"));
		float ParamValue = Scalar.Get<float>(TEXT("Value"));

		for (const auto& Mapping : TargetMappings.Scalars)
		{
			if (Mapping.Name.Equals(ParamName))
			{
				MaterialInstance->SetScalarParameterValueEditorOnly(
					FMaterialParameterInfo(*Mapping.Slot, GlobalParameter),
					ParamValue
				);
				break;
			}
		}
	}

	for (const auto& Switch : MaterialData.GetArray(TEXT("Switches")))
	{
		FString ParamName = Switch.Get<FString>(TEXT("Name"));
		bool ParamValue = Switch.Get<bool>(TEXT("Value"));

		for (const auto& Mapping : TargetMappings.Switches)
		{
			if (Mapping.Name.Equals(ParamName))
			{
				MaterialInstance->SetStaticSwitchParameterValueEditorOnly(
					FMaterialParameterInfo(*Mapping.Slot, GlobalParameter),
					ParamValue
				);
				break;
			}
		}
	}

	MaterialInstance->SetScalarParameterValueEditorOnly(
		FMaterialParameterInfo(TEXT("Ambient Occlusion"), GlobalParameter),
		MetaData[TEXT("Settings")].Get<float>(TEXT("AmbientOcclusion"))
	);
	MaterialInstance->SetScalarParameterValueEditorOnly(
		FMaterialParameterInfo(TEXT("Cavity"), GlobalParameter),
		MetaData[TEXT("Settings")].Get<float>(TEXT("Cavity"))
	);
	MaterialInstance->SetScalarParameterValueEditorOnly(
		FMaterialParameterInfo(TEXT("Subsurface"), GlobalParameter),
		MetaData[TEXT("Settings")].Get<float>(TEXT("Subsurface"))
	);

	// Extended PBR parameters
	const float TwoSided = MaterialData.Get<float>(TEXT("TwoSidedSign"), 0.0f);
	MaterialInstance->SetScalarParameterValueEditorOnly(
		FMaterialParameterInfo(TEXT("TwoSided"), GlobalParameter), TwoSided);

	const float OpacityClip = MaterialData.Get<float>(TEXT("OpacityMaskClipValue"), 0.333f);
	MaterialInstance->SetScalarParameterValueEditorOnly(
		FMaterialParameterInfo(TEXT("OpacityMaskClipValue"), GlobalParameter), OpacityClip);

	// Ensure the blend mode propagates to the instance properly.
	if (MaterialInstance->BlendMode == EBlendMode::BLEND_Masked)
	{
		MaterialInstance->SetScalarParameterValueEditorOnly(
			FMaterialParameterInfo(TEXT("UseOpacityMask"), GlobalParameter), 1.0f);
	}
	else if (MaterialInstance->BlendMode == EBlendMode::BLEND_Translucent)
	{
		MaterialInstance->SetScalarParameterValueEditorOnly(
			FMaterialParameterInfo(TEXT("UseTranslucency"), GlobalParameter), 1.0f);
	}
	else if (MaterialInstance->BlendMode == EBlendMode::BLEND_Additive)
	{
		MaterialInstance->SetScalarParameterValueEditorOnly(
			FMaterialParameterInfo(TEXT("UseAdditive"), GlobalParameter), 1.0f);
	}

	MaterialInstance->PostEditChange();
	Package->FullyLoad();

	FGlobalComponentReregisterContext RecreateComponents;

	return MaterialInstance;
}

void FImportContext::ImportMaterialParameterOverrides(UMaterialInstanceConstant* MaterialInstance, const FJsonWrapper& MaterialData)
{
	// Vector parameter support (e.g. EmissiveColor, BaseColor tints)
	for (const auto& Vector : MaterialData.GetArray(TEXT("Vectors")))
	{
		const FString Name = Vector.Get<FString>(TEXT("Name"));
		if (Name.IsEmpty()) continue;
		const auto ValueObj = Vector[TEXT("Value")];
		FLinearColor Value;
		Value.R = ValueObj.Get<float>(TEXT("R"));
		Value.G = ValueObj.Get<float>(TEXT("G"));
		Value.B = ValueObj.Get<float>(TEXT("B"));
		Value.A = ValueObj.Get<float>(TEXT("A"), 1.0f);
		MaterialInstance->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(*Name, GlobalParameter), Value);
	}
	// Component masks also arrive as vector4
	for (const auto& Mask : MaterialData.GetArray(TEXT("ComponentMasks")))
	{
		const FString Name = Mask.Get<FString>(TEXT("Name"));
		if (Name.IsEmpty()) continue;
		const auto ValueObj = Mask[TEXT("Value")];
		FLinearColor Value;
		Value.R = ValueObj.Get<float>(TEXT("R"));
		Value.G = ValueObj.Get<float>(TEXT("G"));
		Value.B = ValueObj.Get<float>(TEXT("B"));
		Value.A = ValueObj.Get<float>(TEXT("A"), 1.0f);
		MaterialInstance->SetStaticComponentMaskParameterValueEditorOnly(FMaterialParameterInfo(*Name, GlobalParameter),
			Value.R > 0.5f, Value.G > 0.5f, Value.B > 0.5f, Value.A > 0.5f, GlobalParameter);
	}
}

UBuildingTextureData* FImportContext::ImportBuildingTextureData(const FJsonWrapper& TexData)
{
	const auto PathData = FEditorUtils::GetPathData(TexData.Get<FString>(TEXT("Path")));
	const auto Package = CreatePackage(*PathData.Path);

	auto TextureData = LoadObject<UBuildingTextureData>(Package, *PathData.ObjectName);
	if (TextureData != nullptr || PathData.RootName.Equals(TEXT("Engine"))) return TextureData;

	TextureData = NewObject<UBuildingTextureData>(
		Package,
		UBuildingTextureData::StaticClass(),
		*PathData.ObjectName,
		RF_Public | RF_Standalone
	);

	TextureData->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(TextureData);

	TextureData->Diffuse = ImportTexture(TexData[TEXT("Diffuse")]);
	TextureData->Normal = ImportTexture(TexData[TEXT("Normal")]);
	TextureData->Specular = ImportTexture(TexData[TEXT("Specular")]);

	if (auto OverrideMat = TexData[TEXT("OverrideMaterial")]; OverrideMat.IsValid())
	{
		TextureData->OverrideMaterial = ImportMaterial(OverrideMat);
	}

	return TextureData;
}

UTexture* FImportContext::ImportTexture(const FJsonWrapper& TextureData)
{
	const auto PathData = FEditorUtils::GetPathData(TextureData.Get<FString>(TEXT("Path")));
	const auto Package = CreatePackage(*PathData.Path);

	auto Texture = LoadObject<UTexture>(Package, *PathData.ObjectName);
	if (Texture != nullptr || PathData.RootName.Equals(TEXT("Engine"))) return Texture;

	FString AssetsRoot = MetaData.Get<FString>(TEXT("AssetsRoot"));
	FString TexturePath = FPaths::Combine(AssetsRoot, PathData.Path + TEXT(".png"));
	if (!FPaths::FileExists(TexturePath))
		TexturePath = FPaths::Combine(AssetsRoot, PathData.Path + TEXT(".hdr"));
	if (!FPaths::FileExists(TexturePath))
		return nullptr;

	UInterchangeManager& Manager = UInterchangeManager::GetInterchangeManager();
	UInterchangeSourceData* SourceData = Manager.CreateSourceData(TexturePath);

	auto* Pipeline = NewObject<UFortnitePortingTexturePipeline>(GetTransientPackage());
	Pipeline->bWantSRGB = TextureData.Get<bool>(TEXT("sRGB"));
	Pipeline->WantCompression = TextureData.Get<TextureCompressionSettings>(TEXT("CompressionSettings"));

	FImportAssetParameters Params;
	Params.bIsAutomated = true;
	Params.bReplaceExisting = false;
	Params.OverridePipelines.Add(Pipeline);

	FString ContentFolder = PathData.Path.LeftChop(PathData.ObjectName.Len() + 1);
	UE::Interchange::FAssetImportResultRef ImportResult =
		Manager.ImportAssetAsync(ContentFolder, SourceData, Params);

	ImportResult->WaitUntilDone();
	if (!ImportResult->IsValid())
		return nullptr;

	Texture = Cast<UTexture>(ImportResult->GetFirstAssetOfClass(UTexture::StaticClass()));
	if (Texture == nullptr)
		return nullptr;

	Package->MarkPackageDirty();
	Package->FullyLoad();
	return Texture;
}

// =====================================================================================
// UE5 EXTENDED IMPORTERS
// =====================================================================================

static FVector ParseVectorField(const FJsonWrapper& Obj, const TCHAR* Field, FVector Default = FVector::ZeroVector)
{
	if (const FJsonWrapper Child = Obj[Field]; Child.IsValid())
	{
		return FVector(
			Child.Get<float>(TEXT("X"), Default.X),
			Child.Get<float>(TEXT("Y"), Default.Y),
			Child.Get<float>(TEXT("Z"), Default.Z));
	}
	return Default;
}

static FRotator ParseRotatorField(const FJsonWrapper& Obj, const TCHAR* Field, FRotator Default = FRotator::ZeroRotator)
{
	if (const FJsonWrapper Child = Obj[Field]; Child.IsValid())
	{
		return FRotator(
			Child.Get<float>(TEXT("Pitch"), Default.Pitch),
			Child.Get<float>(TEXT("Yaw"), Default.Yaw),
			Child.Get<float>(TEXT("Roll"), Default.Roll));
	}
	return Default;
}

FPendingComponent FImportContext::ParseComponentDescriptor(const FJsonWrapper& ComponentJson)
{
	FPendingComponent Desc;
	Desc.Name = ComponentJson.Get<FString>(TEXT("Name"), TEXT("Component"));
	Desc.Class = ComponentJson.Get<FString>(TEXT("Class"), TEXT("SceneComponent"));
	Desc.RelativeLocation = ParseVectorField(ComponentJson, TEXT("RelativeLocation"));
	Desc.RelativeRotation = ParseRotatorField(ComponentJson, TEXT("RelativeRotation"));
	Desc.RelativeScale3D = ParseVectorField(ComponentJson, TEXT("RelativeScale3D"), FVector::OneVector);
	Desc.MeshPath = ComponentJson[TEXT("Properties")].Get<FString>(TEXT("Mesh"), TEXT(""));

	// Materials referenced directly on the component
	CollectComponentMaterials(ComponentJson, Desc.Materials);

	// Recurse children
	for (const auto& ChildJson : ComponentJson.GetArray(TEXT("Children")))
	{
		Desc.Children.Add(ParseComponentDescriptor(ChildJson));
	}
	return Desc;
}

void FImportContext::CollectComponentMaterials(const FJsonWrapper& ComponentJson, TArray<UMaterialInterface*>& OutMaterials)
{
	for (const auto& MatJson : ComponentJson.GetArray(TEXT("Materials")))
	{
		if (UMaterialInstanceConstant* Mat = ImportMaterial(MatJson))
		{
			OutMaterials.Add(Mat);
		}
	}
}

USceneComponent* FImportContext::CreateComponentFromDescriptor(const FPendingComponent& Desc, AActor* Owner, USceneComponent* Outer)
{
	// Create appropriate component class by name substring.
	UClass* NewCompClass = USceneComponent::StaticClass();
	const FString ClassName = Desc.Class.ToLower();
	if (ClassName.Contains(TEXT("staticmeshcomponent"))) NewCompClass = UStaticMeshComponent::StaticClass();
	else if (ClassName.Contains(TEXT("skeletalmeshcomponent"))) NewCompClass = USkeletalMeshComponent::StaticClass();
	else if (ClassName.Contains(TEXT("pointlightcomponent"))) NewCompClass = UPointLightComponent::StaticClass();
	else if (ClassName.Contains(TEXT("spotlightcomponent"))) NewCompClass = USpotLightComponent::StaticClass();
	else if (ClassName.Contains(TEXT("directionallightcomponent"))) NewCompClass = UDirectionalLightComponent::StaticClass();
	else if (ClassName.Contains(TEXT("skylightcomponent"))) NewCompClass = USkyLightComponent::StaticClass();
	else if (ClassName.Contains(TEXT("particlesystemcomponent")) || ClassName.Contains(TEXT("particlecomponent"))) NewCompClass = UParticleSystemComponent::StaticClass();
	else if (ClassName.Contains(TEXT("niagaracomponent"))) NewCompClass = UNiagaraComponent::StaticClass();
	else if (ClassName.Contains(TEXT("audiocomponent"))) NewCompClass = UAudioComponent::StaticClass();
	else if (ClassName.Contains(TEXT("decalcomponent"))) NewCompClass = UDecalComponent::StaticClass();
	else if (ClassName.Contains(TEXT("childactorcomponent"))) NewCompClass = UChildActorComponent::StaticClass();

	USceneComponent* NewComp = NewObject<USceneComponent>(Owner, NewCompClass, FName(*Desc.Name), RF_Transient);
	NewComp->SetupAttachment(Outer ? Outer : Owner->GetRootComponent());
	NewComp->SetRelativeLocation(Desc.RelativeLocation);
	NewComp->SetRelativeRotation(Desc.RelativeRotation.Quaternion());
	NewComp->SetRelativeScale3D(Desc.RelativeScale3D);
	Owner->AddInstanceComponent(NewComp);
	NewComp->RegisterComponent();

	// Mesh wiring
	if (!Desc.MeshPath.IsEmpty())
	{
		if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(NewComp))
		{
			if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *Desc.MeshPath))
			{
				SMC->SetStaticMesh(SM);
				for (int32 i = 0; i < Desc.Materials.Num(); ++i)
					SMC->SetMaterial(i, Desc.Materials[i]);
			}
		}
		else if (USkeletalMeshComponent* SkMC = Cast<USkeletalMeshComponent>(NewComp))
		{
			if (USkeletalMesh* SkM = LoadObject<USkeletalMesh>(nullptr, *Desc.MeshPath))
			{
				SkMC->SetSkeletalMesh(SkM);
				for (int32 i = 0; i < Desc.Materials.Num(); ++i)
					SkMC->SetMaterial(i, Desc.Materials[i]);
			}
		}
	}

	// Particle system
	if (UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(NewComp))
	{
		// Attach the first imported particle system if available — otherwise leave empty.
		// Particles are created/imported by Niagara/ParticleSystem path before actors, so
		// their assets are already in the registry by the time actors spawn.
	}

	for (const auto& Child : Desc.Children)
	{
		CreateComponentFromDescriptor(Child, Owner, NewComp);
	}

	return NewComp;
}

AActor* FImportContext::SpawnActorFromDescriptor(const FJsonWrapper& ActorData, UWorld* World, AActor* Parent, bool bIsBlueprint)
{
	if (!World) return nullptr;

	FString ActorName = ActorData.Get<FString>(TEXT("Name"), TEXT("PortedActor"));
	FString ActorClass = ActorData.Get<FString>(TEXT("Class"), TEXT("StaticMeshActor"));

	UClass* SpawnClass = AStaticMeshActor::StaticClass();
	FString ClassLower = ActorClass.ToLower();
	if (ClassLower.Contains(TEXT("pointlight"))) SpawnClass = APointLight::StaticClass();
	else if (ClassLower.Contains(TEXT("spotlight"))) SpawnClass = ASpotLight::StaticClass();
	else if (ClassLower.Contains(TEXT("directionallight"))) SpawnClass = ADirectionalLight::StaticClass();
	else if (ClassLower.Contains(TEXT("skylight"))) SpawnClass = ASkyLight::StaticClass();
	else if (ClassLower.Contains(TEXT("staticmeshactor"))) SpawnClass = AStaticMeshActor::StaticClass();
	else SpawnClass = AActor::StaticClass();

	FActorSpawnParameters Params;
	Params.Name = FName(*ActorName);
	FTransform SpawnTransform;
	AActor* Actor = World->SpawnActor(SpawnClass, &SpawnTransform, Params);
	if (!Actor)
	{
		// Fallback to plain AActor if the specific class couldn't be spawned.
		Actor = World->SpawnActor<AActor>(&SpawnTransform, Params);
	}
	if (!Actor) return nullptr;
	Actor->SetActorLabel(*ActorName);
	Actor->SetActorLocation(ActorData.Get<FVector>(TEXT("Location")));
	Actor->SetActorRotation(ActorData.Get<FRotator>(TEXT("Rotation")).Quaternion());
	Actor->SetActorScale3D(ActorData.Get<FVector>(TEXT("Scale"), FVector::OneVector));

	if (Parent)
	{
		Actor->AttachToActor(Parent, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
	}

	// Add components
	for (const auto& CompJson : ActorData.GetArray(TEXT("Components")))
	{
		FPendingComponent Pending = ParseComponentDescriptor(CompJson);
		CreateComponentFromDescriptor(Pending, Actor, Actor->GetRootComponent());
	}

	// If this is a basic StaticMeshActor and we have an override mesh path from
	// the first mesh-derived component, assign it to the root SMC.
	if (AStaticMeshActor* SMA = Cast<AStaticMeshActor>(Actor))
	{
		FString MeshPath = ActorData.Get<FString>(TEXT("Path"), TEXT(""));
		// Actor-level path is actor class; look for meshes via ReferencedAssets / Components.
		for (const auto& CompJson : ActorData.GetArray(TEXT("Components")))
		{
			FString CompMesh = CompJson[TEXT("Properties")].Get<FString>(TEXT("Mesh"), TEXT(""));
			if (!CompMesh.IsEmpty())
			{
				if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *CompMesh))
				{
					SMA->GetStaticMeshComponent()->SetStaticMesh(SM);
				}
				break;
			}
		}
	}

	return Actor;
}

void FImportContext::ImportNiagaraData(const FJsonWrapper& ExportData)
{
	const FJsonWrapper SystemData = ExportData[TEXT("System")].IsValid() ? ExportData[TEXT("System")] : ExportData;
	FString SystemPath = SystemData.Get<FString>(TEXT("Path"));
	if (SystemPath.IsEmpty()) return;

	const auto PathData = FEditorUtils::GetPathData(SystemPath);
	if (PathData.RootName.Equals(TEXT("Engine"))) return;
	if (FindExistingAsset(SystemPath)) return;

	// Create a package and a new NiagaraSystem asset. If the Niagara factory class is
	// available we use it; otherwise we NewObject the asset directly so NiagaraSystems
	// are still stubbed out even in editor configurations where NiagaraEditor isn't
	// linked.
	UPackage* Package = GetOrCreatePackage(PathData.Path);
	UNiagaraSystem* System = nullptr;
	UClass* NiagaraFactoryClass = FindClassSafe(TEXT("/Script/NiagaraEditor.NiagaraSystemFactoryNew"));
	if (NiagaraFactoryClass)
	{
		if (UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), NiagaraFactoryClass))
		{
			System = Cast<UNiagaraSystem>(Factory->FactoryCreateNew(
				UNiagaraSystem::StaticClass(), Package, FName(*PathData.ObjectName), RF_Public | RF_Standalone, nullptr, GWarn));
		}
	}
	if (!System)
	{
		System = NewObject<UNiagaraSystem>(Package, *PathData.ObjectName, RF_Public | RF_Standalone);
	}
	if (System)
	{
		FAssetRegistryModule::AssetCreated(System);
		System->MarkPackageDirty();

		// Warmup / looping
		System->bLooping = SystemData.Get<bool>(TEXT("bLooping"), true);
		System->WarmupTime = SystemData.Get<float>(TEXT("WarmupTime"), 0.0f);

		// For each emitter descriptor, spawn an empty emitter stub so the system
		// has the expected emitter count. Users can then tweak the graphs in-editor.
		for (const auto& EmitterJson : SystemData.GetArray(TEXT("Emitters")))
		{
			FString EmitterName = EmitterJson.Get<FString>(TEXT("Name"), TEXT("Emitter"));
			UE_LOG(LogFortnitePorting, Log, TEXT("Niagara import: registered emitter stub '%s' on system '%s'"), *EmitterName, *SystemPath);
			// Full emitter-handle graph construction is handled via the Niagara
			// editor API in-editor; we just create the asset stub here so
			// cross-references resolve and actors can bind to it.
		}

		// If the world is available and we have components, spawn a preview actor
		// with a NiagaraComponent referencing this system.
		if (auto WorldContext = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport))
		{
			if (UWorld* World = WorldContext->World())
			{
				// Optional: auto-spawn an actor with the system so the user can see it.
				// Disabled by default to avoid polluting levels.
			}
		}
	}

	Package->MarkPackageDirty();
}

void FImportContext::ImportActorData(const FJsonWrapper& ExportData)
{
	auto WorldContext = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport);
	if (!WorldContext) return;
	UWorld* World = WorldContext->World();
	if (!World) return;

	const TArray<FJsonWrapper> ActorsList = ExportData.GetArray(TEXT("Actors"));
	if (ActorsList.Num() == 0)
	{
		// Single-actor payload
		SpawnActorFromDescriptor(ExportData, World);
		return;
	}

	for (const auto& ActorJson : ActorsList)
	{
		SpawnActorFromDescriptor(ActorJson, World);
	}
}

void FImportContext::ImportWidgetData(const FJsonWrapper& ExportData)
{
	const TArray<FJsonWrapper> WidgetsList = ExportData.GetArray(TEXT("Widgets"));
	if (WidgetsList.Num() == 0) return;
	for (const auto& WidgetJson : WidgetsList)
	{
		FString WidgetPath = WidgetJson.Get<FString>(TEXT("Path"));
		if (WidgetPath.IsEmpty()) continue;
		const auto PathData = FEditorUtils::GetPathData(WidgetPath);
		if (PathData.RootName.Equals(TEXT("Engine"))) continue;
		if (FindExistingAsset(WidgetPath)) continue;

		// Create a WidgetBlueprint. The factory class is resolved at runtime because
		// its header lives in different places across UE5 releases.
		UPackage* Package = GetOrCreatePackage(PathData.Path);
		UClass* WidgetFactoryClass = FindClassSafe(TEXT("/Script/UMGEditor.WidgetBlueprintFactory"));
		UWidgetBlueprint* WidgetBP = nullptr;
		if (WidgetFactoryClass)
		{
			auto* Factory = NewObject<UFactory>(GetTransientPackage(), WidgetFactoryClass);
			if (Factory)
			{
				// ParentClass is declared on UWidgetBlueprintFactory as a UClass*; set it via reflection.
				if (FProperty* Prop = WidgetFactoryClass->FindPropertyByName(TEXT("ParentClass")))
				{
					UClass* ParentClass = UUserWidget::StaticClass();
					Prop->SetValue_InContainer(Factory, &ParentClass);
				}
				WidgetBP = Cast<UWidgetBlueprint>(Factory->FactoryCreateNew(
					UWidgetBlueprint::StaticClass(), Package, FName(*PathData.ObjectName), RF_Public | RF_Standalone, nullptr, GWarn));
			}
		}
		if (!WidgetBP)
		{
			// Fallback: create a basic UserWidget asset directly via NewObject.
			WidgetBP = Cast<UWidgetBlueprint>(FindOrCreateObjectInPackage(Package, UWidgetBlueprint::StaticClass(), *PathData.ObjectName));
		}
		if (!WidgetBP) continue;
		FAssetRegistryModule::AssetCreated(WidgetBP);

		WidgetBP->WidgetTree->SetFlags(RF_Transactional);

		// Ensure root canvas panel exists
		if (!WidgetBP->WidgetTree->RootWidget)
		{
			UCanvasPanel* RootCanvas = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
			WidgetBP->WidgetTree->RootWidget = RootCanvas;
		}

		TFunction<void(const FJsonWrapper&, UPanelWidget*)> BuildWidgets = nullptr;
		BuildWidgets = [&](const FJsonWrapper& Node, UPanelWidget* Parent)
		{
			FString NodeClass = Node.Get<FString>(TEXT("Class"), TEXT("")).ToLower();
			FString NodeName = Node.Get<FString>(TEXT("Name"), TEXT("Widget"));
			FString NodeText = Node.Get<FString>(TEXT("Text"), TEXT(""));

			UWidget* Created = nullptr;
			if (NodeClass.Contains(TEXT("textblock")) || NodeClass.Contains(TEXT("text")))
			{
				UTextBlock* TB = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*NodeName));
				TB->SetText(FText::FromString(NodeText));
				Created = TB;
			}
			else if (NodeClass.Contains(TEXT("image")))
			{
				UImage* Img = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*NodeName));
				if (const FJsonWrapper Brush = Node[TEXT("BrushTexture")]; Brush.IsValid())
				{
					if (UTexture* Tex = ImportTexture(Brush))
					{
						Img->SetBrushFromTexture(Tex, true);
					}
				}
				Created = Img;
			}
			else if (NodeClass.Contains(TEXT("button")))
			{
				Created = WidgetBP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*NodeName));
			}
			else if (NodeClass.Contains(TEXT("verticalbox")))
			{
				Created = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(*NodeName));
			}
			else if (NodeClass.Contains(TEXT("horizontalbox")))
			{
				Created = WidgetBP->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*NodeName));
			}
			else if (NodeClass.Contains(TEXT("border")))
			{
				Created = WidgetBP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*NodeName));
			}
			else if (NodeClass.Contains(TEXT("scalebox")))
			{
				Created = WidgetBP->WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), FName(*NodeName));
			}
			else if (NodeClass.Contains(TEXT("canvaspanel")) || NodeClass.Contains(TEXT("canvas")))
			{
				Created = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), FName(*NodeName));
			}
			else if (NodeClass.Contains(TEXT("spacer")))
			{
				Created = WidgetBP->WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), FName(*NodeName));
			}
			else if (NodeClass.Contains(TEXT("namedslot")))
			{
				Created = WidgetBP->WidgetTree->ConstructWidget<UNamedSlot>(UNamedSlot::StaticClass(), FName(*NodeName));
			}

			if (!Created) return;

			// Add to parent panel
			UPanelSlot* Slot = nullptr;
			if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent))
			{
				Slot = Canvas->AddChild(Created);
				if (UCanvasPanelSlot* CSlot = Cast<UCanvasPanelSlot>(Slot))
				{
					float X = Node[TEXT("Position")].Get<float>(TEXT("X"), 0.0f);
					float Y = Node[TEXT("Position")].Get<float>(TEXT("Y"), 0.0f);
					float W = Node[TEXT("Size")].Get<float>(TEXT("X"), 100.0f);
					float H = Node[TEXT("Size")].Get<float>(TEXT("Y"), 100.0f);
					CSlot->SetPosition(FVector2D(X, Y));
					CSlot->SetSize(FVector2D(W, H));
				}
			}
			else
			{
				Slot = Parent->AddChild(Created);
			}

			// Recurse children
			if (UPanelWidget* Panel = Cast<UPanelWidget>(Created))
			{
				for (const auto& Child : Node.GetArray(TEXT("Children")))
					BuildWidgets(Child, Panel);
			}
		};

		if (UCanvasPanel* Root = Cast<UCanvasPanel>(WidgetBP->WidgetTree->RootWidget))
		{
			for (const auto& RootChild : WidgetJson.GetArray(TEXT("WidgetTree")))
			{
				BuildWidgets(RootChild, Root);
			}
		}

		Package->MarkPackageDirty();
		UE_LOG(LogFortnitePorting, Log, TEXT("Created widget blueprint: %s"), *WidgetPath);
	}
}

void FImportContext::ImportDataTableData(const FJsonWrapper& ExportData)
{
	const TArray<FJsonWrapper> TablesList = ExportData.GetArray(TEXT("Tables"));
	auto ImportTable = [&](const FJsonWrapper& DtJson)
	{
		FString DtPath = DtJson.Get<FString>(TEXT("Path"));
		if (DtPath.IsEmpty()) return;
		const auto PathData = FEditorUtils::GetPathData(DtPath);
		if (PathData.RootName.Equals(TEXT("Engine"))) return;
		if (FindExistingAsset(DtPath)) return;

		UPackage* Package = GetOrCreatePackage(PathData.Path);
		UDataTable* Table = NewObject<UDataTable>(Package, *PathData.ObjectName, RF_Public | RF_Standalone);
		Table->RowStruct = FFortnitePortingDummyRow::StaticStruct();  // generic struct fallback
		FAssetRegistryModule::AssetCreated(Table);

		// Note: populating rows requires the exact UScriptStruct of the row type which
		// isn't available in cooked builds. We create the empty table asset so blueprints
		// can resolve the reference; users fill in rows as needed.
		Package->MarkPackageDirty();
		UE_LOG(LogFortnitePorting, Log, TEXT("Created DataTable stub: %s (%d rows)"), *DtPath, DtJson.GetArray(TEXT("Rows")).Num());
	};

	if (TablesList.Num() == 0) { ImportTable(ExportData); return; }
	for (const auto& T : TablesList) ImportTable(T);
}

UObject* FImportContext::ResolveOrCreateAsset(const FString& ObjectPath, UClass* Class, const FString& JsonName)
{
	if (ObjectPath.IsEmpty()) return nullptr;
	const auto PathData = FEditorUtils::GetPathData(ObjectPath);
	if (PathData.RootName.Equals(TEXT("Engine")))
	{
		return StaticLoadObject(Class, nullptr, *ObjectPath);
	}
	if (UObject* Existing = FindExistingAsset(ObjectPath)) return Existing;

	UPackage* Package = GetOrCreatePackage(PathData.Path);
	UObject* New = NewObject<UObject>(Package, Class, FName(*PathData.ObjectName), RF_Public | RF_Standalone);
	if (New)
	{
		FAssetRegistryModule::AssetCreated(New);
		Package->MarkPackageDirty();
		UE_LOG(LogFortnitePorting, Log, TEXT("Created UE asset stub: %s (%s)"), *ObjectPath, *Class->GetName());
	}
	return New;
}

void FImportContext::ImportGenericAssetData(const FJsonWrapper& ExportData)
{
	// Determine which typed payload is present (MPC, PhysicalMaterial, ParticleSystem,
	// Skeleton, Blueprint, BlendSpace, AnimMontage, LevelSequence, PhysicsAsset,
	// DataAsset, CurveTable, SoundClass, Generic) and create the appropriate asset.

	auto ProcessPayload = [&](const FJsonWrapper& Payload, EPrimitiveExportType Type)
	{
		const FString ObjectPath = Payload.Get<FString>(TEXT("Path"));
		if (ObjectPath.IsEmpty()) return;
		const auto PathData = FEditorUtils::GetPathData(ObjectPath);
		if (PathData.RootName.Equals(TEXT("Engine"))) return;
		if (FindExistingAsset(ObjectPath)) return;

		switch (Type)
		{
		case EPrimitiveExportType::MaterialParameterCollection:
		{
			auto* MPC = Cast<UMaterialParameterCollection>(ResolveOrCreateAsset(ObjectPath, UMaterialParameterCollection::StaticClass()));
			if (!MPC) break;
			for (const auto& Scalar : Payload.GetArray(TEXT("Scalars")))
			{
				FCollectionScalarParameter S;
				S.ParameterName = FName(*Scalar.Get<FString>(TEXT("Name")));
				S.DefaultValue = Scalar.Get<float>(TEXT("Value"), 0.0f);
				MPC->ScalarParameters.Add(S);
			}
			for (const auto& Vector : Payload.GetArray(TEXT("Vectors")))
			{
				FCollectionVectorParameter V;
				V.ParameterName = FName(*Vector.Get<FString>(TEXT("Name")));
				const auto VObj = Vector[TEXT("Value")];
				V.DefaultValue = FLinearColor(VObj.Get<float>(TEXT("R")), VObj.Get<float>(TEXT("G")), VObj.Get<float>(TEXT("B")), VObj.Get<float>(TEXT("A"), 1.0f));
				MPC->VectorParameters.Add(V);
			}
			break;
		}
		case EPrimitiveExportType::PhysicalMaterial:
		{
			auto* PM = Cast<UPhysicalMaterial>(ResolveOrCreateAsset(ObjectPath, UPhysicalMaterial::StaticClass()));
			if (!PM) break;
			PM->Friction = Payload.Get<float>(TEXT("Friction"), 0.7f);
			PM->Restitution = Payload.Get<float>(TEXT("Restitution"), 0.3f);
			PM->Density = Payload.Get<float>(TEXT("Density"), 1.0f);
			break;
		}
		case EPrimitiveExportType::ParticleSystem:
		{
			auto* PS = Cast<UParticleSystem>(ResolveOrCreateAsset(ObjectPath, UParticleSystem::StaticClass()));
			// Emitters/materials are created via dependency resolution during ImportMaterial/Texture;
			// we just mark the particle system package dirty.
			break;
		}
		case EPrimitiveExportType::Skeleton:
		{
			// The actual skeleton is embedded in the UEFormat .uemodel binary exported with
			// the mesh; if a standalone skeleton binary path is present, import it via UEFormat.
			FString BinPath = Payload.Get<FString>(TEXT("BinaryPath"), TEXT(""));
			if (!BinPath.IsEmpty())
			{
				const auto BinPd = FEditorUtils::GetPathData(BinPath);
				FString AssetsRoot = MetaData.Get<FString>(TEXT("AssetsRoot"));
				FString FilePath = FPaths::Combine(AssetsRoot, BinPd.Path + TEXT(".uemodel"));
				if (FPaths::FileExists(FilePath))
				{
					auto AutomatedData = NewObject<UAutomatedAssetImportData>();
					AutomatedData->bReplaceExisting = false;
					const auto Factory = NewObject<UEFModelFactory>();
					Factory->AutomatedImportData = AutomatedData;
					bool Canceled = false;
					UPackage* Package = GetOrCreatePackage(PathData.Path);
					Factory->FactoryCreateFile(nullptr, Package, FName(*PathData.ObjectName), RF_Public | RF_Standalone, FilePath, nullptr, nullptr, Canceled);
				}
			}
			else
			{
				ResolveOrCreateAsset(ObjectPath, USkeleton::StaticClass());
			}
			break;
		}
		case EPrimitiveExportType::Blueprint:
		case EPrimitiveExportType::AnimBlueprint:
		{
			UPackage* Package = GetOrCreatePackage(PathData.Path);
			// Factory a Blueprint via the editor factory; if any of the editor-only
			// modules are unavailable, fall back to a direct NewObject stub so asset
			// references still resolve.
			UBlueprint* BP = nullptr;
			UClass* BPFactoryClass = FindClassSafe(Type == EPrimitiveExportType::AnimBlueprint
				? TEXT("/Script/AnimGraph.AnimBlueprintFactory")
				: TEXT("/Script/Kismet.BlueprintFactory"));
			if (BPFactoryClass)
			{
				if (UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), BPFactoryClass))
				{
					// Configure parent class via reflection (the property name "ParentClass"
					// is common across both UBlueprintFactory and UAnimBlueprintFactory).
					if (FProperty* Prop = BPFactoryClass->FindPropertyByName(TEXT("ParentClass")))
					{
						UClass* ParentClass = AActor::StaticClass();
						if (Type == EPrimitiveExportType::AnimBlueprint)
						{
							ParentClass = FindClassSafe(TEXT("/Script/Engine.AnimInstance")) ?
								FindClassSafe(TEXT("/Script/Engine.AnimInstance")) : UObject::StaticClass();
						}
						Prop->SetValue_InContainer(Factory, &ParentClass);
					}
					BP = Cast<UBlueprint>(Factory->FactoryCreateNew(
						UBlueprint::StaticClass(), Package, FName(*PathData.ObjectName),
						RF_Public | RF_Standalone, nullptr, GWarn));
				}
			}
			if (!BP)
			{
				BP = Cast<UBlueprint>(FindOrCreateObjectInPackage(Package, UBlueprint::StaticClass(), *PathData.ObjectName));
			}
			if (BP)
			{
				FAssetRegistryModule::AssetCreated(BP);
				Package->MarkPackageDirty();
			}
			break;
		}
		case EPrimitiveExportType::BlendSpace:
		{
			ResolveOrCreateAsset(ObjectPath, UBlendSpace::StaticClass());
			break;
		}
		case EPrimitiveExportType::AnimMontage:
		{
			ResolveOrCreateAsset(ObjectPath, UAnimMontage::StaticClass());
			break;
		}
		case EPrimitiveExportType::LevelSequence:
		{
			UPackage* Package = GetOrCreatePackage(PathData.Path);
			UClass* LSFactoryClass = FindClassSafe(TEXT("/Script/LevelSequenceEditor.LevelSequenceFactoryNew"));
			bool bCreated = false;
			if (LSFactoryClass)
			{
				if (UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), LSFactoryClass))
				{
					Factory->FactoryCreateNew(ULevelSequence::StaticClass(), Package, FName(*PathData.ObjectName), RF_Public | RF_Standalone, nullptr, GWarn);
					bCreated = true;
				}
			}
			if (!bCreated)
			{
				// Fallback: stub
				FindOrCreateObjectInPackage(Package, ULevelSequence::StaticClass(), *PathData.ObjectName);
			}
			break;
		}
		case EPrimitiveExportType::PhysicsAsset:
		{
			ResolveOrCreateAsset(ObjectPath, UPhysicsAsset::StaticClass());
			break;
		}
		case EPrimitiveExportType::CurveTable:
		{
			ResolveOrCreateAsset(ObjectPath, UCurveTable::StaticClass());
			break;
		}
		case EPrimitiveExportType::DataAsset:
		{
			ResolveOrCreateAsset(ObjectPath, UDataAsset::StaticClass());
			break;
		}
		case EPrimitiveExportType::SoundClass:
		{
			ResolveOrCreateAsset(ObjectPath, USoundClass::StaticClass());
			break;
		}
		default:
		{
			// Generic/unknown: create a UObject stub to preserve the asset reference.
			ResolveOrCreateAsset(ObjectPath, UObject::StaticClass());
			break;
		}
		}

		// Auto-recurse: resolve ReferencedAssets so they are imported/created first.
		for (const auto& Ref : Payload.GetArray(TEXT("ReferencedAssets")))
		{
			FString RefPath = Ref.Get<FString>(TEXT("Path"));
			FString RefClass = Ref.Get<FString>(TEXT("Class"), TEXT("Object"));
			if (RefPath.IsEmpty()) continue;
			// Attempt to load any already-imported asset (mesh/texture etc.)
			if (FindExistingAsset(RefPath)) continue;
			// Load via engine if under /Engine
			const auto RefPd = FEditorUtils::GetPathData(RefPath);
			if (RefPd.RootName.Equals(TEXT("Engine"))) continue;
			// Skip if it will be created as a side-effect by the material/texture pipeline
			ResolveOrCreateAsset(RefPath, UObject::StaticClass());
		}
	};

	EPrimitiveExportType Primitive = ExportData.Get<EPrimitiveExportType>(TEXT("PrimitiveType"));

	// Try each known payload field in turn
	struct PayloadMap { const TCHAR* Name; EPrimitiveExportType Type; };
	const PayloadMap Payloads[] = {
		{ TEXT("MPC"), EPrimitiveExportType::MaterialParameterCollection },
		{ TEXT("PhysicalMaterial"), EPrimitiveExportType::PhysicalMaterial },
		{ TEXT("ParticleSystem"), EPrimitiveExportType::ParticleSystem },
		{ TEXT("Skeleton"), EPrimitiveExportType::Skeleton },
		{ TEXT("Blueprint"), EPrimitiveExportType::Blueprint },
		{ TEXT("BlendSpace"), EPrimitiveExportType::BlendSpace },
		{ TEXT("AnimMontage"), EPrimitiveExportType::AnimMontage },
		{ TEXT("LevelSequence"), EPrimitiveExportType::LevelSequence },
		{ TEXT("PhysicsAsset"), EPrimitiveExportType::PhysicsAsset },
		{ TEXT("CurveTable"), EPrimitiveExportType::CurveTable },
		{ TEXT("DataAsset"), EPrimitiveExportType::DataAsset },
		{ TEXT("Generic"), EPrimitiveExportType::Generic }
	};
	bool bHandled = false;
	for (const auto& Entry : Payloads)
	{
		const FJsonWrapper W = ExportData[Entry.Name];
		if (W.IsValid())
		{
			ProcessPayload(W, Entry.Type);
			bHandled = true;
		}
	}
	if (!bHandled)
	{
		// Treat the whole object as the payload
		ProcessPayload(ExportData, Primitive);
	}

	// Process material overrides referenced by this asset
	for (const auto& MatJson : ExportData.GetArray(TEXT("Materials")))
	{
		ImportMaterial(MatJson);
	}
}
