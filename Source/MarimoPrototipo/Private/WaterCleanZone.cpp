#include "WaterCleanZone.h"
#include "WaterCleanerComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "WaterCleanProgressSubsystem.h"

AWaterCleanZone::AWaterCleanZone()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;   // only ticks while stamps are pending

    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
    SetRootComponent(Bounds);
    Bounds->SetBoxExtent(FVector(500.f, 500.f, 200.f));
    Bounds->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
    Bounds->SetGenerateOverlapEvents(true);

    WaterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaterMesh"));
    WaterMesh->SetupAttachment(Bounds);
    WaterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWaterCleanZone::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // Auto-fit the water mesh to the box (XY), whatever size the mesh asset has
    if (WaterMesh && WaterMesh->GetStaticMesh())
    {
        const FVector MeshSize = WaterMesh->GetStaticMesh()->GetBounds().BoxExtent * 2.0;
        const FVector E = Bounds->GetUnscaledBoxExtent();
        if (MeshSize.X > KINDA_SMALL_NUMBER && MeshSize.Y > KINDA_SMALL_NUMBER)
        {
            const FVector Cur = WaterMesh->GetRelativeScale3D();
            WaterMesh->SetRelativeScale3D(FVector(E.X * 2.0 / MeshSize.X, E.Y * 2.0 / MeshSize.Y, Cur.Z));
        }
    }
}

void AWaterCleanZone::BeginPlay()
{
    Super::BeginPlay();

    // Visual mask: white = dirty, black = clean
    MaskRT = UKismetRenderingLibrary::CreateRenderTarget2D(this, MaskRes, MaskRes, RTF_R8);
    UKismetRenderingLibrary::ClearRenderTarget2D(this, MaskRT, FLinearColor::White);

    MID = WaterMesh->CreateDynamicMaterialInstance(0);
    if (MID)
    {
        const FVector C = Bounds->GetComponentLocation();
        const FVector E = Bounds->GetScaledBoxExtent();
        MID->SetTextureParameterValue(TEXT("DirtMask"), MaskRT);
        MID->SetVectorParameterValue(TEXT("ZoneMin"), FLinearColor(C.X - E.X, C.Y - E.Y, 0.f));
        MID->SetVectorParameterValue(TEXT("ZoneSize"), FLinearColor(E.X * 2.f, E.Y * 2.f, 0.f));
    }

    // Coarse progress grid
    const int32 NumCells = ProgressRes * ProgressRes;
    CellDirty.Init(true, NumCells);
    CellCleaned.Init(false, NumCells);
    Total = NumCells;

    // Overlap events drive the cleaner component
    Bounds->OnComponentBeginOverlap.AddDynamic(this, &AWaterCleanZone::OnBoundsBeginOverlap);
    Bounds->OnComponentEndOverlap.AddDynamic(this, &AWaterCleanZone::OnBoundsEndOverlap);

    // Robots that are already inside at game start
    TArray<AActor*> Overlapping;
    Bounds->GetOverlappingActors(Overlapping);
    for (AActor* Other : Overlapping)
    {
        if (UWaterCleanerComponent* Cleaner = Other->FindComponentByClass<UWaterCleanerComponent>())
        {
            Cleaner->AddZone(this);
        }
    }

    if (UWaterCleanProgressSubsystem* Sub = GetWorld()->GetSubsystem<UWaterCleanProgressSubsystem>())
    {
        Sub->RegisterZone(this);
    }
}

void AWaterCleanZone::OnBoundsBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!OtherActor) return;
    if (UWaterCleanerComponent* Cleaner = OtherActor->FindComponentByClass<UWaterCleanerComponent>())
    {
        Cleaner->AddZone(this);
    }
}

void AWaterCleanZone::OnBoundsEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32)
{
    if (!OtherActor) return;
    if (UWaterCleanerComponent* Cleaner = OtherActor->FindComponentByClass<UWaterCleanerComponent>())
    {
        Cleaner->RemoveZone(this);
    }
}

void AWaterCleanZone::CleanSegment(const FVector& From, const FVector& To, float WorldRadius)
{
    if (bDone) return;

    const FVector E = Bounds->GetScaledBoxExtent();
    PendingRadiusUV = FVector2D(WorldRadius / (E.X * 2.f), WorldRadius / (E.Y * 2.f));

    if (!bHasLastStamp)
    {
        LastStampXY = FVector2D(From.X, From.Y);
        bHasLastStamp = true;
        QueuePoint(LastStampXY);
    }

    // Fixed spacing with carry-over: a standing or slow robot costs nothing extra
    const FVector2D ToXY(To.X, To.Y);
    const float Spacing = FMath::Max(1.f, WorldRadius * SpacingFactor);
    const FVector2D Delta = ToXY - LastStampXY;
    float Remaining = Delta.Size();
    if (Remaining < Spacing) return;

    const FVector2D Dir = Delta / Remaining;
    while (Remaining >= Spacing)
    {
        LastStampXY += Dir * Spacing;
        Remaining -= Spacing;
        QueuePoint(LastStampXY);
    }
}

void AWaterCleanZone::QueuePoint(const FVector2D& WorldXY)
{
    const FVector C = Bounds->GetComponentLocation();
    const FVector E = Bounds->GetScaledBoxExtent();
    const FVector2D UV((WorldXY.X - (C.X - E.X)) / (E.X * 2.f),
                       (WorldXY.Y - (C.Y - E.Y)) / (E.Y * 2.f));
    if (UV.X < 0.f || UV.Y < 0.f || UV.X > 1.f || UV.Y > 1.f) return;

    PendingUV.Add(UV);
    SetActorTickEnabled(true);

    // Progress: touch only a few cells of the coarse grid
    const float RX = FMath::Max(0.01f, PendingRadiusUV.X * ProgressRes);
    const float RY = FMath::Max(0.01f, PendingRadiusUV.Y * ProgressRes);
    const float PX = UV.X * ProgressRes;
    const float PY = UV.Y * ProgressRes;

    const int32 X0 = FMath::Max(0, FMath::FloorToInt(PX - RX));
    const int32 X1 = FMath::Min(ProgressRes - 1, FMath::CeilToInt(PX + RX));
    const int32 Y0 = FMath::Max(0, FMath::FloorToInt(PY - RY));
    const int32 Y1 = FMath::Min(ProgressRes - 1, FMath::CeilToInt(PY + RY));

    for (int32 y = Y0; y <= Y1; ++y)
    {
        for (int32 x = X0; x <= X1; ++x)
        {
            const float nx = (x + 0.5f - PX) / RX;
            const float ny = (y + 0.5f - PY) / RY;
            if (nx * nx + ny * ny > 1.f) continue;

            const int32 Idx = y * ProgressRes + x;
            if (CellDirty[Idx] && !CellCleaned[Idx])
            {
                CellCleaned[Idx] = true;
                ++CleanedCount;
            }
        }
    }
}

void AWaterCleanZone::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    TimeSinceFlush += DeltaSeconds;
    if (TimeSinceFlush >= FlushInterval)
    {
        TimeSinceFlush = 0.f;
        FlushStamps();
    }
}

void AWaterCleanZone::FlushStamps()
{
    if (PendingUV.IsEmpty())
    {
        SetActorTickEnabled(false);
        return;
    }

    
    if (IsValid(MaskRT) && IsValid(BrushTexture))
    {
        UCanvas* Canvas = nullptr;
        FVector2D Size;
        FDrawToRenderTargetContext Ctx;
        UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, MaskRT, Canvas, Size, Ctx);
        if (Canvas)
        {
            const FVector2D RPx = PendingRadiusUV * Size;
            for (const FVector2D& UV : PendingUV)
            {
                const FVector2D Px = UV * Size;
                Canvas->K2_DrawTexture(BrushTexture, Px - RPx, RPx * 2.f,
                                       FVector2D(0.f, 0.f), FVector2D(1.f, 1.f),
                                       FLinearColor::Black, BLEND_Translucent);
            }
            UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Ctx);
        }
    }

    PendingUV.Reset();
    
    bool bJustStarted = false;
    if (!bStarted && CleanedCount > 0)
    {
        bStarted = true;
        bJustStarted = true;
    }

    bool bJustCompleted = false;
    if (!bDone && GetCleanPercent() >= CleanThreshold)
    {
        bDone = true;
        bJustCompleted = true;
        UKismetRenderingLibrary::ClearRenderTarget2D(this, MaskRT, FLinearColor::Black);  // snap to fully clean
        SetActorTickEnabled(false);
    }

    // Report only when something really changed
    const float P = GetProgress();
    if (bJustStarted || bJustCompleted || !FMath::IsNearlyEqual(P, LastBroadcastProgress, 0.002f))
    {
        LastBroadcastProgress = P;
        OnZoneProgressChanged.Broadcast(this, P);
    }

    if (bJustCompleted)
    {
        OnZoneCleaned.Broadcast(this);
    }
}

float AWaterCleanZone::GetAreaWeight() const
{
    const FVector E = Bounds->GetScaledBoxExtent();
    return FMath::Max(1.f, E.X * E.Y * 4.f);
}

void AWaterCleanZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (UWaterCleanProgressSubsystem* Sub = World->GetSubsystem<UWaterCleanProgressSubsystem>())
        {
            Sub->UnregisterZone(this);
        }
    }
    Super::EndPlay(EndPlayReason);
}

