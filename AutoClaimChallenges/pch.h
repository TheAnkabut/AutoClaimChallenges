#pragma once

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <span>
#include <cstdint>

#include "IMGUI/imgui.h"
#include "IMGUI/imgui_stdlib.h"
#include "IMGUI/imgui_searchablecombo.h"
#include "IMGUI/imgui_rangeslider.h"

#include "logging.h"