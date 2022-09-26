#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include <imgui.h>

bool BeginGroupPanel(const char* name, const ImVec2& size, float maxWidth, const char* droppableID, void* payload, size_t payloadSize);
void EndGroupPanel();

