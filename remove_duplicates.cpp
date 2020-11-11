#include "remove_duplicates.h"

#include <algorithm>
#include <map>
#include <string>

using namespace std;

bool IsDuplicate(const map<string, double>& lhs, const map<string, double>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    return equal(lhs.begin(), lhs.end(), rhs.begin(),
        [](const auto& old_word_freq, const auto& new_word_freq) {
            return old_word_freq.first == new_word_freq.first;
        });
}

vector<int> FindDuplicateIds(const SearchServer& search_server) {
    vector<int> ids_to_delete;

    for (auto it_i = search_server.begin(); it_i != search_server.end(); ++it_i) {
        const int lhs_id = *it_i;
        if (count(ids_to_delete.begin(), ids_to_delete.end(), lhs_id) == 0) {
            const map<string, double>& lhs = search_server.GetWordFrequencies(lhs_id);

            for (auto it_j = it_i + 1; it_j != search_server.end(); ++it_j) {
                const int rhs_id = *it_j;
                const map<string, double>& rhs = search_server.GetWordFrequencies(rhs_id);

                if (IsDuplicate(lhs, rhs)) {
                    ids_to_delete.push_back(rhs_id);
                }
            }
        }
    }

    sort(ids_to_delete.begin(), ids_to_delete.end());

    return ids_to_delete;
}

void RemoveDuplicates(SearchServer& search_server) {
    for (int id : FindDuplicateIds(search_server)) {
        search_server.RemoveDocument(id);
        std::cout << "Found duplicate document id "s << id << std::endl;
    }
}