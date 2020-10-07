#include "searchserver.h"

#include <cmath>
#include <iostream>
#include <numeric>

const double MAX_RELEVANCE_ACCURACY = 1e-6;

/* -------------------------------------------------------------------------- */

void Document::Print() const {
    cout << "{ "s
         << "document_id = "s << id << ", "s
         << "relevance = "s << relevance << ", "s
         << "rating = "s << rating
         << " }"s << endl;
}

bool Document::CompareRelevance(const Document& lhs, const Document& rhs) {
    if (abs(lhs.relevance - rhs.relevance) < MAX_RELEVANCE_ACCURACY) {
        return lhs.rating > rhs.rating;
    } else {
        return lhs.relevance > rhs.relevance;
    }
}

/* -------------------------------------------------------------------------- */

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

void PrintMatchDocumentResult(int document_id, const vector<string>& words,
                              DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

/* -------------------------------------------------------------------------- */

string SearchServer::GetStopWords() const
{
    string stop_words;
    for (auto i = stop_words_.begin(); i != stop_words_.end(); ++i) {
        if (i != stop_words_.begin()) {
            stop_words += " "s;
        }
        stop_words += *i;
    }
    return stop_words;
}

int SearchServer::GetDocumentCount() const {
    return static_cast<int>(document_data_.size());
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string &raw_query, int document_id) const {
    Query query = ParseQuery(raw_query, true);
    vector<string> words;
    if (!HasMinusWord(query.minus_words, document_id)) {
        for (auto& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0 ||
                    word_to_document_freqs_.at(word).count(document_id) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).at(document_id)) {
                words.push_back(word);
            }
        }
        sort(words.begin(), words.end());
    }

    return {words, document_data_.at(document_id).status};
}

void SearchServer::AddDocument(int document_id, const string &document, DocumentStatus status, const vector<int> &ratings) {
    if (document_id < 0) {
        throw invalid_argument("Document id must pe positive"s);
    }

    if (document_data_.count(document_id)) {
        throw invalid_argument("Document with id = "s + to_string(document_id) + "already exists"s);
    }

    vector<string> words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        if (word_to_document_freqs_[word].count(document_id) == 0) {
            word_to_document_freqs_[word][document_id] = 0.0;
        }
        word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    document_data_.emplace(document_id, DocumentData{ComputeAverageRating(ratings),
                                                     status});
    document_ids_.push_back(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string &raw_query, const DocumentStatus status) const {
    return FindTopDocuments(raw_query,
    [status](int document_id, DocumentStatus st, int rating) { (void)document_id; (void)rating; return status == st; });
}

vector<Document> SearchServer::FindTopDocuments(const string &raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentId(int index) const
{
    return document_ids_.at(index);
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("The word = "s + word + " contains special symbol"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

bool SearchServer::HasMinusWord(const set<string> minus_words, const int document_id) const {
    return find_if(minus_words.begin(), minus_words.end(),
                   [this, document_id](const string& word) {
        return word_to_document_freqs_.count(word) &&
                word_to_document_freqs_.at(word).count(document_id); })
            != minus_words.end();
}

int SearchServer::ComputeAverageRating(const vector<int> &ratings) {
    return (!ratings.empty())
            ? accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size())
            : 0;
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string text) const {
    if (!IsValidWord(text)) {
        throw invalid_argument("The word = "s + text + " contains special symbol"s);
    }
    bool is_minus = text[0] == '-';
    // Word shouldn't be empty
    if (is_minus) {
        is_minus = true;
        text = text.substr(1);

        if (!IsValidMinusWord(text)) {
            throw invalid_argument("The word = "s + text + " is invalid minus word"s);
        }
    }

    return {text, is_minus, IsStopWord(text)};
}

SearchServer::Query SearchServer::ParseQuery(const string &text, const bool all_words) const
{
    Query query;
    for (const string& word : SplitIntoWords(text)) {
        QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop || all_words) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            } else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string &word) const {
    return log(document_data_.size() * 1.0 / word_to_document_freqs_.at(word).size());
}

bool SearchServer::IsValidWord(const string &word)
{
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(),
                   [](char c) { return c >= '\0' && c < ' '; });
}

bool SearchServer::IsValidMinusWord(const string& word)
{
    return word.size() > 0 && word.at(0) != '-';
}

vector<string> SearchServer::SplitIntoWords(const string &text) const
{
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

