#ifndef SEARCHSERVER_H
#define SEARCHSERVER_H

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

/* -------------------------------------------------------------------------- */

struct Document {
    int id = 0;
    double relevance = 0.0;
    int rating = 0;

    Document() = default;

    Document(int input_id, double input_relevance, int input_rating) :
        id(input_id),
        relevance(input_relevance),
        rating(input_rating) {}

    void Print() const;

    static bool CompareRelevance(const Document& lhs, const Document& rhs);
};

/* -------------------------------------------------------------------------- */

enum class DocumentStatus
{
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

/* -------------------------------------------------------------------------- */

struct DocumentData
{
    int rating;
    DocumentStatus status;
};

/* -------------------------------------------------------------------------- */

class SearchServer {
public:
    inline static constexpr int INVALID_DOCUMENT_ID = -1;
    inline static constexpr size_t MAX_RESULT_DOCUMENT_COUNT = 5;

    SearchServer() = default;

    template<typename StopWordsCollection>
    explicit SearchServer(const StopWordsCollection& stop_words) :
        stop_words_(MakeUniqueNonEmptyStringCollection(stop_words)) {
    }

    explicit SearchServer(const string& stop_words) :
        SearchServer(SplitIntoWords(stop_words)) {
    }

    string GetStopWords() const;

    int GetDocumentCount() const;

    optional<tuple<vector<string>, DocumentStatus>> MatchDocument(const string& raw_query, int document_id) const;

    [[nodiscard]] bool AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings);

    template<typename DocumentPredicate>
    optional<vector<Document>> FindTopDocuments(const string& raw_query, const DocumentPredicate& predicate) const {
        Query query;
        if (!ParseQuery(raw_query, query)) {
            return nullopt;
        }
        vector<Document> result = FindAllDocuments(query, predicate);

        sort(result.begin(), result.end(),
             [](const Document& lhs, const Document& rhs) {
            return Document::CompareRelevance(lhs, rhs);
        });

        if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
            result.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return result;
    }

    optional<vector<Document>> FindTopDocuments(const string& raw_query, const DocumentStatus status) const;

    optional<vector<Document>> FindTopDocuments(const string& raw_query) const;

    int GetDocumentId(int index) const;

private:
    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> document_data_;
    vector<int> document_ids_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    [[nodiscard]] bool SplitIntoWordsNoStop(const string& text, vector<string>& words) const;

    bool HasMinusWord(const set<string> minus_words, const int document_id) const;

    static int ComputeAverageRating(const vector<int>& ratings);

    [[nodiscard]] bool ParseQueryWord(string text, QueryWord& query_word) const;

    [[nodiscard]] bool ParseQuery(const string& text, Query& query,
                                  const bool all_words = false) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const;

    template<typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, const DocumentPredicate& predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = document_data_.at(document_id);
                if (predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                                            document_id,
                                            relevance,
                                            document_data_.at(document_id).rating
                                        });
        }
        return matched_documents;
    }

    static bool IsValidWord(const string& word);

    static bool IsValidMinusWord(const string& word);

    vector<string> SplitIntoWords(const string& text) const;

    template<typename StringCollection>
    set<string> MakeUniqueNonEmptyStringCollection(const StringCollection& collection) const {
        set<string> strings;
        for (const string& word : collection) {
            if (!IsValidWord(word)) {
                throw invalid_argument("Words can't contain special characters"s);
            }
            if (!word.empty()) {
                strings.insert(word);
            }
        }
        return strings;
    }
};

#endif // SEARCHSERVER_H
