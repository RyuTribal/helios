#pragma once


#ifdef HVE_PLATFORM_WINDOWS
	#ifndef NOMINMAX
		// See github.com/skypjack/entt/wiki/Frequently-Asked-Questions#warning-c4003-the-min-the-max-and-the-macro
		#define NOMINMAX
	#endif
#endif

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <cassert>
#include <stdexcept>
#include <limits>
#include <fstream>


#include <string>
#include <sstream>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <array>

#include <chrono>
#include <future>

#ifdef HVE_PLATFORM_WINDOWS
	#include <Windows.h>
#endif