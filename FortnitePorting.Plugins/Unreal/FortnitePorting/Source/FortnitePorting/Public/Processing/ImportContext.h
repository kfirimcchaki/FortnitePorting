#pragma once
#include "CoreMinimal.h"
#include "EditorAssetLibrary.h"
#include "Factories/TextureFactory.h"
#include "Factories/UEFModelFactory.h"
#include "Utilities/JsonWrapper.h"
#include "World/BuildingActor.h"

class UMaterialInterface;
class UMaterialInstanceConstant;
class UTexture;
class UDataTable;
class UCurveTable;
class UMaterialParameterCollection;
class UPhysicalMaterial;
class UParticleSystem;
class UNiagaraSystem;
class UBlueprint;
class UWidgetBlueprint;
class USkeleton;
class UBlendSpaceBase;
class UAnimMontage;
class ULevelSequence;
class UPhysicsAsset;
class AActor;
class USceneComponent;

/**
 * Describes an actor or blueprint component pending creation (used by the Actor /
 * Blueprint / Niagara / ParticleSystem importers).
 */
struct FPendingComponent
{
	FString Name;
	FString Class;
	FVector RelativeLocation = FVector::ZeroVector;
	FRotator RelativeRotation = FRotator::ZeroRotator;
	FVector RelativeScale3D = FVector::OneVector;
	FString MeshPath;
	TArray<UMaterialInterface*> Materials;
	TArray<FPendingComponent> Children;
};

class FImportContext
{
public:
	FImportContext(const FJsonWrapper& MetaData);
	void RunExport(const FJsonWrapper& Json);

	static void RunExportJson(const FString& Data);
	static void EnsureDependencies();

	inline static UMaterial* DefaultMaterial;
	inline static UMaterial* LayerMaterial;
	inline static UMaterial* DefaultParticleMaterial;

private:
	FJsonWrapper MetaData;

	void ImportMeshData(const FJsonWrapper& ExportData);
	void ImportTextureData(const FJsonWrapper& ExportData);
	void ImportSoundData(const FJsonWrapper& ExportData);
	void ImportAnimationData(const FJsonWrapper& ExportData);

	// ===== UE5 EXTENDED IMPORTERS =====
	void ImportNiagaraData(const FJsonWrapper& ExportData);
	void ImportActorData(const FJsonWrapper& ExportData);
	void ImportWidgetData(const FJsonWrapper& ExportData);
	void ImportDataTableData(const FJsonWrapper& ExportData);
	void ImportGenericAssetData(const FJsonWrapper& ExportData);

	// Actor/component creation helpers (shared between Actor/Blueprint/Niagara paths).
	AActor* SpawnActorFromDescriptor(const FJsonWrapper& ActorData, UWorld* World, AActor* Parent = nullptr, bool bIsBlueprint = false);
	USceneComponent* CreateComponentFromDescriptor(const FPendingComponent& Desc, AActor* Owner, USceneComponent* Outer);
	FPendingComponent ParseComponentDescriptor(const FJsonWrapper& ComponentJson);
	void CollectComponentMaterials(const FJsonWrapper& ComponentJson, TArray<UMaterialInterface*>& OutMaterials);

	UObject* ImportModel(const FJsonWrapper& ExportData, UWorld* World, ABuildingActor* Parent, const FJsonWrapper& MeshData, bool bCreateActor);
	UObject* ImportMesh(const FJsonWrapper& MeshData);

	UMaterialInstanceConstant* ImportMaterial(const FJsonWrapper& MaterialData);
	void ImportMaterialParameterOverrides(UMaterialInstanceConstant* MaterialInstance, const FJsonWrapper& MaterialData);

	UBuildingTextureData* ImportBuildingTextureData(const FJsonWrapper& TexData);

	UTexture* ImportTexture(const FJsonWrapper& TextureData);

	// Extended asset creation helpers
	UObject* ResolveOrCreateAsset(const FString& ObjectPath, UClass* Class, const FString& AssetJsonName = TEXT(""));
};
