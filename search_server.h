#pragma once

#include "concurrent_map.h"
#include "document.h"
#include "log_duration.h"

#include <algorithm>
#include <execution>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_set>
#include <utility>

class SearchServer {
public:
    inline static constexpr size_t MAX_RESULT_DOCUMENT_COUNT = 5;

    SearchServer() = default;

    template<typename StopWordsCollection>
    explicit SearchServer(const StopWordsCollection& stop_words);
    explicit SearchServer(const std::string& stop_words);
    explicit SearchServer(const std::string_view& stop_words);

    void AddDocument(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings);

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view& raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy&, const std::string_view& raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy&, const std::string_view& raw_query, int document_id) const;

    template<typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, const DocumentPredicate& predicate) const;
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, const DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const;

    template<typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view& raw_query, const DocumentPredicate& predicate) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view& raw_query, const DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view& raw_query) const;

    template<typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, const std::string_view& raw_query, const DocumentPredicate& predicate) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, const std::string_view& raw_query, const DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, const std::string_view& raw_query) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);
    void RemoveDocument(const std::execution::parallel_policy&, int document_id);

    int GetDocumentCount() const;
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;
    std::string GetStopWords() const;

    const std::set<int>::const_iterator begin() const {
        return document_ids_.begin();
    }

    const std::set<int>::const_iterator end() const {
        return document_ids_.end();
    }

private:
    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    struct Query {
        std::unordered_set<std::string_view> plus_words;
        std::unordered_set<std::string_view> minus_words;
    };

private:
    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, const DocumentPredicate& predicate) const;
    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, const DocumentPredicate& predicate) const;
    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query, const DocumentPredicate& predicate) const;

    QueryWord ParseQueryWord(std::string_view text) const;
    Query ParseQuery(const std::string_view& text, const bool all_words = false) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view& word) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);

    std::vector<std::string_view> SplitIntoWords(std::string_view text) const;
    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view& text) const;

    template<typename StringCollection>
    std::set<std::string, std::less<>> MakeUniqueNonEmptyStringCollection(const StringCollection& collection) const;

    bool HasMinusWord(const std::unordered_set<std::string_view>& minus_words, const int document_id) const;
    bool IsStopWord(const std::string_view& word) const;
    static bool IsValidWord(const std::string_view& word);
    static bool IsValidMinusWord(const std::string_view& word);

private:
    std::unordered_set<std::string> words_to_documents_;
    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> document_data_;
    std::set<int> document_ids_;
};

template<typename StopWordsCollection>
SearchServer::SearchServer(const StopWordsCollection& stop_words) :
    stop_words_(MakeUniqueNonEmptyStringCollection(stop_words)) {
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, const DocumentPredicate& predicate) const {
    Query query = ParseQuery(raw_query);

    std::vector<Document> result = FindAllDocuments(query, predicate);

    std::sort(result.begin(), result.end(),
        [](const Document& lhs, const Document& rhs) {
            return Document::CompareRelevance(lhs, rhs);
        });

    if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
        result.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return result;
}

template<typename DocumentPredicate>
inline std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, const std::string_view& raw_query, const DocumentPredicate& predicate) const {
    return FindTopDocuments(raw_query, predicate);
}

template<typename DocumentPredicate>
inline std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, const std::string_view& raw_query, const DocumentPredicate& predicate) const {
    Query query = ParseQuery(raw_query);

    std::vector<Document> result = FindAllDocuments(std::execution::par, query, predicate);

    std::sort(std::execution::par, result.begin(), result.end(),
        [](const Document& lhs, const Document& rhs) {
            return Document::CompareRelevance(lhs, rhs);
        });

    if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
        result.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return result;
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, const DocumentPredicate& predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string_view& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0 ||
            word_to_document_freqs_.at(word).size() == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = document_data_.at(document_id);
            if (predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string_view& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto& [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({
                                        document_id,
                                        relevance,
                                        document_data_.at(document_id).rating
            });
    }
    return matched_documents;
}

template<typename DocumentPredicate>
inline std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, const DocumentPredicate& predicate) const {
    return FindAllDocuments(query, predicate);
}

template<typename DocumentPredicate>
inline std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&, const Query& query, const DocumentPredicate& predicate) const {
    ConcurrentMap<int, double> document_to_relevance(4);

    std::unordered_set<std::string_view> words;
    std::for_each(query.plus_words.begin(), query.plus_words.end(),
        [this, &words](const std::string_view& word) {
            if (word_to_document_freqs_.count(word) != 0 &&
                word_to_document_freqs_.at(word).size() != 0) {
                words.insert(word);
            }
        });

    std::for_each(std::execution::par, words.begin(), words.end(),
        [this, &document_to_relevance, &predicate](const std::string_view& word) {
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = document_data_.at(document_id);
                if (predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        });

    for (const std::string_view& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto& [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back({
                                        document_id,
                                        relevance,
                                        document_data_.at(document_id).rating
            });
    }

    return matched_documents;
}

template<typename StringCollection>
std::set<std::string, std::less<>> SearchServer::MakeUniqueNonEmptyStringCollection(const StringCollection& collection) const {
    std::set<std::string, std::less<>> strings;
    for (const std::string_view& word : collection) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument(std::string("Words can't contain special characters"));
        }
        if (!word.empty()) {
            strings.insert(std::string(word));
        }
    }
    return strings;
}

// ----------------------------------------------------------------------------

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string& query);
