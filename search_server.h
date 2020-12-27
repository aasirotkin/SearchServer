#pragma once

#include "document.h"
#include "log_duration.h"

#include <algorithm>
#include <execution>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

class SearchServer {
public:
    inline static constexpr size_t MAX_RESULT_DOCUMENT_COUNT = 5;

    SearchServer() = default;

    template<typename StopWordsCollection>
    explicit SearchServer(const StopWordsCollection& stop_words);
    explicit SearchServer(const std::string& stop_words);

    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;

    template<typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, const DocumentPredicate& predicate) const;
    std::vector<Document> FindTopDocuments(const std::string& raw_query, const DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);
    void RemoveDocument(const std::execution::parallel_policy&, int document_id);

    int GetDocumentCount() const;
    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;
    std::string GetStopWords() const;

    const std::set<int>::const_iterator begin() const {
        return document_ids_.begin();
    }

    const std::set<int>::const_iterator end() const {
        return document_ids_.end();
    }

private:
    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

private:
    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, const DocumentPredicate& predicate) const;

    QueryWord ParseQueryWord(std::string text) const;
    Query ParseQuery(const std::string& text, const bool all_words = false) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string& word) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);

    std::vector<std::string> SplitIntoWords(const std::string& text) const;
    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    template<typename StringCollection>
    std::set<std::string> MakeUniqueNonEmptyStringCollection(const StringCollection& collection) const;

    bool HasMinusWord(const std::set<std::string> minus_words, const int document_id) const;
    bool IsStopWord(const std::string& word) const;
    static bool IsValidWord(const std::string& word);
    static bool IsValidMinusWord(const std::string& word);

private:
    std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> document_data_;
    std::set<int> document_ids_;
};

template<typename StopWordsCollection>
SearchServer::SearchServer(const StopWordsCollection& stop_words) :
    stop_words_(MakeUniqueNonEmptyStringCollection(stop_words)) {
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, const DocumentPredicate& predicate) const {
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
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, const DocumentPredicate& predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string& word : query.plus_words) {
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

    for (const std::string& word : query.minus_words) {
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

template<typename StringCollection>
std::set<std::string> SearchServer::MakeUniqueNonEmptyStringCollection(const StringCollection& collection) const {
    std::set<std::string> strings;
    for (const std::string& word : collection) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument(std::string("Words can't contain special characters"));
        }
        if (!word.empty()) {
            strings.insert(word);
        }
    }
    return strings;
}

// ----------------------------------------------------------------------------

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string& query);
