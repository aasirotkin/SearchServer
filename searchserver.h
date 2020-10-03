#ifndef SEARCHSERVER_H
#define SEARCHSERVER_H

#include <map>
#include <set>
#include <string>
#include <vector>
#include <algorithm>

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
    int id;
    int rating;
    DocumentStatus status;
};

/* -------------------------------------------------------------------------- */

class SearchServer {
public:
    inline static constexpr int INVALID_DOCUMENT_ID = -1;
    inline static constexpr size_t MAX_RESULT_DOCUMENT_COUNT = 5;

    SearchServer() = default;

    SearchServer(const string& stop_words) {
        SetStopWords(stop_words);
    }

    template<typename StopWordsCollection>
    explicit SearchServer(const StopWordsCollection& stop_words) {
        for (auto word : stop_words) {
            if (!word.empty()) {
                stop_words_.insert(word);
            }
        }
    }

    void SetStopWords(const string& text);

    string GetStopWords() const;

    int GetDocumentCount() const;

    [[nodiscard]] bool MatchDocument(const string& raw_query, int document_id,
                                     tuple<vector<string>, DocumentStatus>& result) const;

    [[nodiscard]] bool AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings);

    template<typename KeyMapper>
    [[nodiscard]] bool FindTopDocuments(const string& raw_query, const KeyMapper& mapper,
                                        vector<Document>& result) const {
        Query query;
        if (!ParseQuery(raw_query, query)) {
            return false;
        }
        result = FindAllDocuments(query);

        sort(result.begin(), result.end(),
             [](const Document& lhs, const Document& rhs) {
            return Document::CompareRelevance(lhs, rhs);
        });

        result.erase(
                    remove_if(result.begin(), result.end(),
                              [this, &mapper](const Document& doc) {
            return !mapper(doc.id, document_data_.at(doc.id).status, document_data_.at(doc.id).rating); }),
                    result.end());

        if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
            result.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return true;
    }

    [[nodiscard]] bool FindTopDocuments(const string& raw_query, const DocumentStatus& status,
                                        vector<Document>& result) const;

    [[nodiscard]] bool FindTopDocuments(const string& raw_query,
                                        vector<Document>& result) const;

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

    vector<Document> FindAllDocuments(const Query& query) const;

    static bool IsValidWord(const string& word);

    static bool IsValidMinusWord(const string& word);
};

#endif // SEARCHSERVER_H
