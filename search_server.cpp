#include "search_server.h"

#include <cmath>
#include <numeric>

using namespace std;

SearchServer::SearchServer(const std::string& stop_words) :
    SearchServer(SplitIntoWords(stop_words)) {
}

void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    if (document_id < 0) {
        throw invalid_argument("Document id must pe positive"s);
    }

    if (document_data_.count(document_id)) {
        throw invalid_argument("Document with id = "s + to_string(document_id) + "already exists"s);
    }

    vector<string> words = SplitIntoWordsNoStop(document);
    map<string, double> word_frequency;

    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        if (word_to_document_freqs_[word].count(document_id) == 0) {
            word_to_document_freqs_[word][document_id] = 0.0;
        }
        word_to_document_freqs_[word][document_id] += inv_word_count;

        if (word_frequency.count(word) == 0) {
            word_frequency[word] = 0.0;
        }
        word_frequency[word] += inv_word_count;
    }
    document_data_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings),
                                                     status, word_frequency });
    document_ids_.emplace(document_id);
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    Query query = ParseQuery(raw_query, true);
    vector<string> words;
    if (!HasMinusWord(query.minus_words, document_id)) {
        for (auto& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0 ||
                word_to_document_freqs_.at(word).count(document_id) == 0) {
                continue;
            }
            words.push_back(word);
        }
        sort(words.begin(), words.end());
    }

    return { words, document_data_.at(document_id).status };
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query, const DocumentStatus status) const {
    return FindTopDocuments(raw_query,
        [status](int document_id, DocumentStatus st, int rating) { (void)document_id; (void)rating; return status == st; });
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

void SearchServer::RemoveDocument(int document_id)
{
    if (document_ids_.count(document_id)) {
        const std::map<string, double>& word_frequency = document_data_.at(document_id).word_frequency;
        for (const auto& [word, frequency] : word_frequency) {
            word_to_document_freqs_.at(word).erase(document_id);
            if (word_to_document_freqs_.at(word).size() == 0) {
                word_to_document_freqs_.erase(word);
            }
        }

        document_ids_.erase(document_id);
        document_data_.erase(document_id);
    }
}

void SearchServer::RemoveDocument(const execution::sequenced_policy&, int document_id)
{
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(const execution::parallel_policy&, int document_id)
{
    if (document_ids_.count(document_id)) {
        const map<string, double>& word_frequency = document_data_.at(document_id).word_frequency;

        for_each(execution::par, word_frequency.begin(), word_frequency.end(),
            [this, document_id](const auto& word_freq) {
                word_to_document_freqs_.at(word_freq.first).erase(document_id);
            });

        //vector<const string*> words(word_frequency.size());
        //transform(execution::par,
        //    word_frequency.begin(), word_frequency.end(),
        //    words.begin(), [](const auto& word_freq) { return &word_freq.first; });

        //for_each(execution::par, words.begin(), words.end(),
        //    [this, document_id](const string* word) {
        //        word_to_document_freqs_.at(*word).erase(document_id);
        //    });

        //for (const string* word : words) {
        //    if (word_to_document_freqs_.at(*word).size() == 0) {
        //        word_to_document_freqs_.erase(*word);
        //    }
        //}

        document_ids_.erase(document_id);
        document_data_.erase(document_id);
    }
}

int SearchServer::GetDocumentCount() const {
    return static_cast<int>(document_data_.size());
}

const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const
{
    static const map<string, double> empty;
    return
        (document_data_.count(document_id) == 0)
        ? empty
        : document_data_.at(document_id).word_frequency;
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

    return { text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(const string& text, const bool all_words) const
{
    Query query;
    for (const string& word : SplitIntoWords(text)) {
        QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop || all_words) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            }
            else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(document_data_.size() * 1.0 / word_to_document_freqs_.at(word).size());
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    return (!ratings.empty())
        ? accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size())
        : 0;
}

vector<string> SearchServer::SplitIntoWords(const string& text) const
{
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
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

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
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

// ----------------------------------------------------------------------------

void AddDocument(SearchServer &search_server, int document_id, const string &document, DocumentStatus status, const vector<int> &ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const invalid_argument& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer &search_server, const string &raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const invalid_argument& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer &search_server, const string &query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int document_id : search_server) {
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const invalid_argument& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}
