#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOMCX
#define NOHELP
#define NOCOMM
#include <windows.h>
#ifdef __INTELLISENSE__
#include <unknwn.h>
#endif
// Undefine GetCurrentTime macro to prevent
// conflict with Storyboard::GetCurrentTime
#undef GetCurrentTime

// STL headers must be included before import std; to avoid redefinition errors
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <ranges>
#include <regex>
#include <set>
#include <thread>
#include <variant>
#include <unordered_map>
#include <unordered_set>