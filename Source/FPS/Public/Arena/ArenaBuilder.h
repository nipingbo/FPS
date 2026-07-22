#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "ArenaBuilder.generated.h"

/**
 * 在编辑器里一键生成「废弃工业仓库」竞技场的灰盒布局。
 * 拖一个 AArenaBuilder 到关卡，在 Details 面板点 "Build Arena" 即可。
 * 生成的灰盒是持久化在关卡中的真实 Actor（仅编辑器，打包时排除），可后续换皮。
 */
UCLASS(Blueprintable)
class AArenaBuilder : public AActor
{
	GENERATED_BODY()

public:
	AArenaBuilder();

	// 在编辑器 Details 面板点此按钮，生成灰盒竞技场
	UFUNCTION(CallInEditor, Category = "Arena")
	void BuildArena();

	// 清除已生成的灰盒
	UFUNCTION(CallInEditor, Category = "Arena")
	void ClearArena();

protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh = nullptr;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> SpawnedActors;

	// 生成一个立方体灰盒并记录下来
	TWeakObjectPtr<AActor> SpawnBox(const FVector& Location, const FVector& Scale, const FRotator& Rotation, const FString& Label);
#endif
};
