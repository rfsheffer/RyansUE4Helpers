// Copyright 2020-2021 Sheffer Online Services.
// MIT License. See LICENSE for details.

#include "RyEditorModule.h"

#define LOCTEXT_NAMESPACE "RyEditorModule"

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void FRyEditorModule::StartupModule()
{

}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void FRyEditorModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRyEditorModule, RyEditor)
DEFINE_LOG_CATEGORY(LogRyEditor);
