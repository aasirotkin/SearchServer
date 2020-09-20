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
    int id;
    double relevance;
    int rating;

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
    void SetStopWords(const string& text);

    int GetDocumentCount() const;

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const;

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings);

    template<typename KeyMapper>
    vector<Document> FindTopDocuments(const string& raw_query, const KeyMapper& mapper) const {
        const Query query = ParseQuery(raw_query);
        vector<Document> matched_documents = FindAllDocuments(query);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
            return Document::CompareRelevance(lhs, rhs);
        });

        matched_documents.erase(
                    remove_if(matched_documents.begin(), matched_documents.end(),
                              [this, &mapper](const Document& doc) {
            return !mapper(doc.id, document_data_.at(doc.id).status, document_data_.at(doc.id).rating); }),
                    matched_documents.end());

        if (matched_documents.size() > max_result_document_count_) {
            matched_documents.resize(max_result_document_count_);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query, const DocumentStatus& status) const;

    vector<Document> FindTopDocuments(const string& raw_query) const;

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

    const size_t max_result_document_count_{5};
    int document_count_{0};
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> document_data_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const;

    bool HasMinusWord(const set<string> minus_words, const int document_id) const;

    static int ComputeAverageRating(const vector<int>& ratings);

    QueryWord ParseQueryWord(string text) const;

    Query ParseQuery(const string& text, const bool all_words = false) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const;

    vector<Document> FindAllDocuments(const Query& query) const;
};

#endif // SEARCHSERVER_H
