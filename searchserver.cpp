#include "searchserver.h"

#include <cmath>
#include <iostream>

const double MAX_RELEVANCE_ACCURACY = 10e-6;

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
    }
    else {
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
            words.push_back(word);
            word = "";
        } else {
            word += c;
        }
    }
    words.push_back(word);

    words.erase(remove(words.begin(), words.end(), ""s), words.end());

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

void SearchServer::SetStopWords(const string &text) {
    for (const string& word : SplitIntoWords(text)) {
        stop_words_.insert(word);
    }
}

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

optional<tuple<vector<string>, DocumentStatus>> SearchServer::MatchDocument(const string &raw_query, int document_id) const {
    Query query;
    if (!ParseQuery(raw_query, query, true)) {
        return nullopt;
    }

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

    return tuple{words, document_data_.at(document_id).status};
}

bool SearchServer::AddDocument(int document_id, const string &document, DocumentStatus status, const vector<int> &ratings) {
    if (document_id < 0 || document_data_.count(document_id)) {
        return false;
    }

    vector<string> words;
    if (!SplitIntoWordsNoStop(document, words)) {
        return false;
    }

    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    document_data_.emplace(document_id, DocumentData{document_id,
                                                     ComputeAverageRating(ratings),
                                                     status});
    document_ids_.push_back(document_id);
    return true;
}

bool SearchServer::FindTopDocuments(const string &raw_query, const DocumentStatus &status,
                                                vector<Document>& result) const {
    return FindTopDocuments(raw_query,
    [&status](int document_id, DocumentStatus st, int rating) { (void)document_id; (void)rating; return status == st; },
    result);
}

bool SearchServer::FindTopDocuments(const string &raw_query,
                                                vector<Document>& result) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL, result);
}

int SearchServer::GetDocumentId(int index) const
{
    if (index >= 0 && index < GetDocumentCount()) {
        return document_ids_.at(index);
    }
    return INVALID_DOCUMENT_ID;
}

bool SearchServer::SplitIntoWordsNoStop(const string& text, vector<string>& words) const {
    for (const string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            words.clear();
            return false;
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return true;
}

bool SearchServer::HasMinusWord(const set<string> minus_words, const int document_id) const {
    return find_if(minus_words.begin(), minus_words.end(),
                   [this, document_id](const string& word) {
        return word_to_document_freqs_.count(word) &&
                word_to_document_freqs_.at(word).count(document_id); })
            != minus_words.end();
}

int SearchServer::ComputeAverageRating(const vector<int> &ratings) {
    if (!ratings.empty()) {
        double rating_sum = 0.0;
        int size = static_cast<int>(ratings.size());
        for (const int rating : ratings) {
            rating_sum += static_cast<double>(rating) / size;
        }
        return static_cast<int>(rating_sum);
    }
    return 0;
}

bool SearchServer::ParseQueryWord(string text, QueryWord &query_word) const {
    if (!IsValidWord(text)) {
        return false;
    }
    bool is_minus = text[0] == '-';
    // Word shouldn't be empty
    if (is_minus) {
        is_minus = true;
        text = text.substr(1);

        if (!IsValidMinusWord(text)) {
            return false;
        }
    }

    query_word = {text, is_minus, IsStopWord(text)};
    return true;
}

bool SearchServer::ParseQuery(const string &text, SearchServer::Query& query,
                              const bool all_words) const
{
    for (const string& word : SplitIntoWords(text)) {
        QueryWord query_word;
        if (!ParseQueryWord(word, query_word)) {
            return false;
        }
        if (!query_word.is_stop || all_words) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            } else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return true;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string &word) const {
    return log(document_data_.size() * 1.0 / word_to_document_freqs_.at(word).size());
}

vector<Document> SearchServer::FindAllDocuments(const SearchServer::Query &query) const {
    map<int, double> document_to_relevance;
    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            document_to_relevance[document_id] += term_freq * inverse_document_freq;
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

