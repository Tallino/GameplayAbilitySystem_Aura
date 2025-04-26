// Copyright Tritaghiaccio Games S.r.l


#include "UI/HUD/AuraHUD.h"

#include "UI/Widget/AuraUserWidget.h"

void AAuraHUD::BeginPlay()
{
	Super::BeginPlay();

	UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), OverlayWidgetCLass);
	Widget->AddToViewport();
	
}
