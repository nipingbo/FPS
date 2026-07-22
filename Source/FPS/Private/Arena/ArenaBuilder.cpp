#include "Arena/ArenaBuilder.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"

AArenaBuilder::AArenaBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AArenaBuilder::BuildArena()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ClearArena();

	// 引擎内置立方体（100cm 边长，缩放即尺寸）
	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("ArenaBuilder: 找不到 /Engine/BasicShapes/Cube.Cube"));
		return;
	}

	// 单位：厘米。场地 6000 x 4000（60m x 40m），墙高 400。
	// 地板 6000 x 4000 x 100，顶面在 z=0
	SpawnedActors.Add(SpawnBox(FVector(0, 0, -50), FVector(60, 40, 1), FRotator::ZeroRotator, TEXT("Arena_Floor")));

	// 四面外墙（厚 100，高 400，中心高 200）
	SpawnedActors.Add(SpawnBox(FVector(0, 2000, 200), FVector(60, 1, 4), FRotator::ZeroRotator, TEXT("Wall_North")));
	SpawnedActors.Add(SpawnBox(FVector(0, -2000, 200), FVector(60, 1, 4), FRotator::ZeroRotator, TEXT("Wall_South")));
	SpawnedActors.Add(SpawnBox(FVector(3000, 0, 200), FVector(1, 40, 4), FRotator::ZeroRotator, TEXT("Wall_East")));
	SpawnedActors.Add(SpawnBox(FVector(-3000, 0, 200), FVector(1, 40, 4), FRotator::ZeroRotator, TEXT("Wall_West")));

	// 中央高台（控制点）800 x 800 x 200，顶面 z=200
	SpawnedActors.Add(SpawnBox(FVector(0, 0, 100), FVector(8, 8, 2), FRotator::ZeroRotator, TEXT("Platform_Center")));

	// 南北两侧坡道，连接地面到高台（约 26.5°）
	SpawnedActors.Add(SpawnBox(FVector(0, 600, 100), FVector(4.5f, 4, 0.5f), FRotator(0, 0, -26.5f), TEXT("Ramp_North")));
	SpawnedActors.Add(SpawnBox(FVector(0, -600, 100), FVector(4.5f, 4, 0.5f), FRotator(0, 0, 26.5f), TEXT("Ramp_South")));

	// 内侧隔墙，制造侧翼通道与视线遮挡
	SpawnedActors.Add(SpawnBox(FVector(1200, 0, 200), FVector(1, 30, 4), FRotator::ZeroRotator, TEXT("Wall_Inner_E")));
	SpawnedActors.Add(SpawnBox(FVector(-1200, 0, 200), FVector(1, 30, 4), FRotator::ZeroRotator, TEXT("Wall_Inner_W")));

	// 集装箱掩体（散布，打断视线、制造交火节奏）
	SpawnedActors.Add(SpawnBox(FVector(1500, 1000, 100), FVector(4, 2, 2), FRotator::ZeroRotator, TEXT("Cover_Crate_1")));
	SpawnedActors.Add(SpawnBox(FVector(-1500, -1000, 100), FVector(4, 2, 2), FRotator::ZeroRotator, TEXT("Cover_Crate_2")));
	SpawnedActors.Add(SpawnBox(FVector(1500, -1000, 100), FVector(2, 4, 2), FRotator::ZeroRotator, TEXT("Cover_Crate_3")));
	SpawnedActors.Add(SpawnBox(FVector(-1500, 1000, 100), FVector(2, 4, 2), FRotator::ZeroRotator, TEXT("Cover_Crate_4")));

	// 出生区标记（贴地薄板，仅用于布局参考）
	SpawnedActors.Add(SpawnBox(FVector(0, 1800, 5), FVector(10, 10, 0.1f), FRotator::ZeroRotator, TEXT("Spawn_Blue")));
	SpawnedActors.Add(SpawnBox(FVector(0, -1800, 5), FVector(10, 10, 0.1f), FRotator::ZeroRotator, TEXT("Spawn_Red")));

	UE_LOG(LogTemp, Warning, TEXT("ArenaBuilder: 已生成 %d 个灰盒 Actor。"), SpawnedActors.Num());
#endif
}

void AArenaBuilder::ClearArena()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const TWeakObjectPtr<AActor>& Weak : SpawnedActors)
	{
		if (AActor* Actor = Weak.Get())
		{
			World->DestroyActor(Actor);
		}
	}
	SpawnedActors.Empty();
#endif
}

#if WITH_EDITOR
TWeakObjectPtr<AActor> AArenaBuilder::SpawnBox(const FVector& Location, const FVector& Scale, const FRotator& Rotation, const FString& Label)
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.bDeferConstruction = false;

	AStaticMeshActor* Box = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, Params);
	if (Box)
	{
		Box->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
		Box->SetActorScale3D(Scale);
		Box->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
		Box->SetActorLabel(Label);
		Box->SetFolderPath(FName(TEXT("/Arena/GrayBox")));
		Box->bIsEditorOnlyActor = true; // 仅编辑器用，打包时排除
		return TWeakObjectPtr<AActor>(Box);
	}
	return nullptr;
}
#endif
