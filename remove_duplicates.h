#pragma once

#include "search_server.h"

#include <vector>

std::vector<int> FindDuplicateIds(const SearchServer& search_server);
void RemoveDuplicates(SearchServer& search_server);
