#include "test_example_functions.h"

#include "paginator.h"
#include "process_queries.h"
#include "request_queue.h"
#include "remove_duplicates.h"
#include "search_server.h"

#include <chrono>
#include <execution>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace std;

// -----------------------------------------------------------------------------

template <typename Type>
ostream& operator<< (ostream& out, const vector<Type>& vect) {
    out << "{ "s;
    for (auto it = vect.begin(); it != vect.end(); ++it) {
        if (it != vect.begin()) {
            out << ", ";
        }
        out << *it;
    }
    out << " }";
    return out;
}

// -----------------------------------------------------------------------------

template <typename Func>
void RunTestImpl(Func& func, const string& func_name) {
    func();
    cerr << func_name << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// -----------------------------------------------------------------------------

void AssertImpl(bool value, const string& value_str, const string& file,
                const string& func, unsigned line, const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << value_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(a) AssertImpl((a), #a, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(a, hint) AssertImpl((a), #a, __FILE__, __FUNCTION__, __LINE__, hint)

// -----------------------------------------------------------------------------

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

// -----------------------------------------------------------------------------

enum class ErrorCode {
    OUT_OF_RANGE,
    INVALID_ARGUMENT
};

const string ErrorCodeName(ErrorCode code) {
    switch(code) {
    case ErrorCode::OUT_OF_RANGE: return "out_of_range"s;
    case ErrorCode::INVALID_ARGUMENT: return "invalid_argument"s;
    default: break;
    }
    ASSERT_HINT(false, "invalid exception"s);
    return "invalid exception"s;
}

const string ErrorCodeHint(ErrorCode code) {
    return "Must be "s + ErrorCodeName(code) + " exception"s;
}

template <typename Func>
void AssertTrowImpl(Func& func, ErrorCode code,
                    const string& file, const string& func_name, unsigned line) {
    try
    {
        func();
        AssertImpl(false, "Task failed successfully"s, file, func_name, line, ErrorCodeHint(code));
    }
    catch(const out_of_range&) {
        AssertImpl(code == ErrorCode::OUT_OF_RANGE, ErrorCodeName(code), file, func_name, line, ErrorCodeHint(code));
    }
    catch(const invalid_argument&) {
        AssertImpl(code == ErrorCode::INVALID_ARGUMENT, ErrorCodeName(code), file, func_name, line, ErrorCodeHint(code));
    }
    catch(...) {
        AssertImpl(false, ErrorCodeName(code), file, func_name, line, ErrorCodeHint(code));
    }
    cerr << func_name << " OK" << endl;
}

#define ASSERT_OUT_OF_RANGE(a) AssertTrowImpl((a), ErrorCode::OUT_OF_RANGE, __FILE__, #a, __LINE__)

#define ASSERT_INVALID_ARGUMENT(a) AssertTrowImpl((a), ErrorCode::INVALID_ARGUMENT, __FILE__, #a, __LINE__)

// -----------------------------------------------------------------------------

template <typename UnitOfTime>
class AssertDuration {
public:
    using Clock = std::chrono::steady_clock;

    AssertDuration(int64_t max_duration, const std::string file, const std::string function, unsigned line)
        : max_dur_(max_duration)
        , file_(file)
        , function_(function)
        , line_(line) {
    }

    ~AssertDuration() {
        const auto dur = Clock::now() - start_time_;
        const auto converted_dur = std::chrono::duration_cast<UnitOfTime>(dur).count();
        if (converted_dur > max_dur_) {
            cerr << "Assert duration fail: "s << file_ << " "s << function_ << ": "s << line_ << endl;
            cerr << "Process duration is "s << converted_dur << " while max duration is " << max_dur_ << endl;
            cerr << "So the function worked longer on "s << converted_dur - max_dur_ << endl;
            abort();
        }
    }

private:
    int64_t max_dur_;
    std::string file_;
    std::string function_;
    unsigned line_;
    const Clock::time_point start_time_ = Clock::now();
};

#define ASSERT_DURATION_MILLISECONDS(x) AssertDuration<std::chrono::milliseconds> UNIQUE_VAR_NAME_PROFILE(x, __FILE__, __FUNCTION__, __LINE__)
#define ASSERT_DURATION_SECONDS(x) AssertDuration<std::chrono::seconds> UNIQUE_VAR_NAME_PROFILE(x, __FILE__, __FUNCTION__, __LINE__)

// -------- Начало модульных тестов поисковой системы ----------

// Добавление документов
void TestAddDocuments() {
    const int id_actual = 42;
    const int id_banned = 61;
    const int id_empty = 14;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3, 4, 5};

    SearchServer server;
    server.AddDocument(id_actual, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(id_banned, content, DocumentStatus::BANNED, ratings);
    server.AddDocument(id_empty, ""s, DocumentStatus::ACTUAL, ratings);
    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 3, "Only 2 documents have been added"s);

    { // Сначала убеждаемся, что документы добавлены и могут быть найдены
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL_HINT(found_docs.size(), size_t(1), "Only one not empty document with ACTUAL status has been added"s);
        ASSERT_EQUAL_HINT(found_docs.at(0).id, id_actual, "This is not document with ACTUAL status"s);
    }

    { // Убедимся, что лишние документы найдены не будут
        const auto found_docs = server.FindTopDocuments("dog"s);
        ASSERT_HINT(found_docs.empty(), "There is not document with dog word"s);
    }

    { // Проверяем, что если знак минус написан между словами, то документ найден будет
        const auto docs = server.FindTopDocuments("cat in-the city"s);
        ASSERT(docs.size() == size_t(1));
    }
}

// Тест проверяет, что стоп слова правильно добавляются
void TestStopWords() {
    { // Проверяем, что стоп слова не дублируются
        SearchServer server("in at in the"s);
        ASSERT_EQUAL(server.GetStopWords(), "at in the"s);
    }
    { // Проверяем, что лишние пробелы в стоп словах не обрабатываются
        SearchServer server("       in    at    the      "s);
        ASSERT_EQUAL(server.GetStopWords(), "at in the"s);
    }
    { // Проверяем, как считываюся стоп слова из vector
        vector<string> stop_words = {"in"s, "at"s, "the"s, "in"s, "the"s};
        SearchServer server(stop_words);
        ASSERT_EQUAL(server.GetStopWords(), "at in the"s);
    }
    { // Проверяем, как считываюся стоп слова из set
        set<string> stop_words = {"in"s, "at"s, "the"s, "in"s, "the"s};
        SearchServer server(stop_words);
        ASSERT_EQUAL(server.GetStopWords(), "at in the"s);
    }
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), size_t(1));
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_HINT(found_docs.empty(), "Stop words must be excluded from documents"s);
    }
}

// Поддержка минус-слов
void TestMinusWords() {
    SearchServer server;
    const int id_1 = 42;
    const int id_2 = 51;
    const DocumentStatus status = DocumentStatus::ACTUAL;
    const vector<int> ratings = {1, 2, 3};

    server.AddDocument(id_1, "cat in the city"s, status, ratings);
    server.AddDocument(id_2, "dog in the garden"s, status, ratings);
    ASSERT_EQUAL(server.GetDocumentCount(), 2);

    // Убедимся, что минус слово отсекает второй документ
    const auto docs_1 = server.FindTopDocuments("cat or dog in the -garden"s);
    ASSERT_EQUAL(docs_1.size(), size_t(1));
    ASSERT_EQUAL(docs_1.at(0).id, id_1);

    // Убедимся, что минус слово отсекает первый документ
    const auto docs_2 = server.FindTopDocuments("cat or dog in the -city"s);
    ASSERT_EQUAL(docs_2.size(), size_t(1));
    ASSERT_EQUAL(docs_2.at(0).id, id_2);

    // Убедимся, что минус слово работает для обоих документов
    const auto docs_3 = server.FindTopDocuments("rat -in the space"s);
    ASSERT_HINT(docs_3.empty(), "Documents with minus words must be excluded"s);

    // Убедимся, что минус слово которого нет в обоих документах не повлияет на результат
    const auto docs_4 = server.FindTopDocuments("-rat in the space"s);
    ASSERT_EQUAL(docs_4.size(), size_t(2));
    ASSERT_EQUAL(docs_4.at(0).id, id_1);
    ASSERT_EQUAL(docs_4.at(1).id, id_2);
}

// Основная функция находится сверху, это вспомагательная
// данная функция может быть использована только в связке с TestMatchDocument
// у TestMatchDocument дожены быть документы со строкой "cat in the big city"
void TestMatchDocumentStatus(const SearchServer& server, const int id,
                             const DocumentStatus status) {
    { // Убедимся, что при наличии минус слова ничего найдено не будет
        const auto [words, status_out] = server.MatchDocument("cat -city"s, id);
        ASSERT_HINT(words.empty(), "Query contains minus word"s);
        ASSERT_HINT(status_out == status, "Status must be correct"s);
    }

    { // Убедимся, что наличие минус слова, которого нет в документе, не повлияет на результат
        const auto [words, status_out] = server.MatchDocument("cat city -fake"s, id);
        ASSERT_EQUAL(words.size(), size_t(2));
        // Здесь проверяется лексикографический порядок слова
        ASSERT_EQUAL_HINT(words.at(0), "cat"s, "Words order must be lexicographical"s);
        ASSERT_EQUAL_HINT(words.at(1), "city"s, "Words order must be lexicographical"s);
        ASSERT_HINT(status_out == status, "Status must be correct"s);
    }

    { // Убедимся, что знак минус между словами не считается минус словом
        const auto [words, status_out] = server.MatchDocument("cat in the big-city"s, id);
        ASSERT_EQUAL(words.size(), size_t(3));
        ASSERT_HINT(status_out == status, "Status must be correct"s);
    }
}

// Матчинг документов
void TestMatchDocument() {
    SearchServer server;
    const vector<int> ratings = {1, 2, 3};
    const string content{"cat in the big city"s};

    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 0, "The server must be empty yet"s);

    server.AddDocument(64, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(12, content, DocumentStatus::BANNED, ratings);
    server.AddDocument(51, content, DocumentStatus::IRRELEVANT, ratings);
    server.AddDocument(75, content, DocumentStatus::REMOVED, ratings);

    ASSERT_EQUAL(server.GetDocumentCount(), 4);

    TestMatchDocumentStatus(server, 64, DocumentStatus::ACTUAL);
    TestMatchDocumentStatus(server, 12, DocumentStatus::BANNED);
    TestMatchDocumentStatus(server, 51, DocumentStatus::IRRELEVANT);
    TestMatchDocumentStatus(server, 75, DocumentStatus::REMOVED);
}

// Проверка на равенство чисел
bool InTheVicinity(const double d1, const double d2, const double delta) {
    return abs(d1 - d2) < delta;
}

// Проверка, что одно число больше другого
bool MoreThan(const double d1, const double d2, const double delta) {
    return d1 - d2 > delta;
}

// Сортировка документов по релевантности
void TestSortRelevance() {
    const double delta = 1e-6;
    const DocumentStatus status = DocumentStatus::ACTUAL;
    const vector<int> rating{-2, -3, 7};
    const string query{"kind cat with long tail"s};
    SearchServer server;

    server.AddDocument(6, "human tail"s, status, rating);
    server.AddDocument(5, "old angry fat dog with short tail"s, status, rating);
    server.AddDocument(4, "nasty cat beautiful tail"s, status, rating);
    server.AddDocument(3, "not beautiful cat"s, status, rating);
    server.AddDocument(2, "huge fat parrot"s, status, rating);
    server.AddDocument(1, "removed cat"s, status, rating);

    const auto docs = server.FindTopDocuments(query);
    for (size_t i = 0; i + 1 < docs.size(); ++i) {
        ASSERT_HINT(MoreThan(docs.at(i).relevance, docs.at(i + 1).relevance, delta) ||
                    InTheVicinity(docs.at(i).relevance, docs.at(i + 1).relevance, delta),
                    "Found documents must be sorted by relevance and than by rating"s);
    }
}

// Вычисление рейтинга документов
void TestRating() {
    const DocumentStatus status = DocumentStatus::ACTUAL;
    const string content = "cat in the city"s;

    { // Проверяем, что рейтинг считается правильно и документы сортируются по рейтингу при равной релевантности
        SearchServer server;

        server.AddDocument(1, content, status, {0});
        server.AddDocument(2, content, status, {0, 5, 10});
        server.AddDocument(3, content, status, {-2, -1, 0});
        server.AddDocument(4, content, status, {-5, 0, 35});
        server.AddDocument(5, content, status, {-7, -3, -5});
        server.AddDocument(6, content, status, {-7, -2});
        ASSERT_EQUAL(server.GetDocumentCount(), 6);

        const auto docs = server.FindTopDocuments(content, status);
        ASSERT_EQUAL_HINT(docs.size(), size_t(5), "Maximus documents count equals to 5"s);
        ASSERT_EQUAL_HINT(docs.at(0).rating, 10, "In this test documenst must be sorted by rating"s);
        ASSERT_EQUAL_HINT(docs.at(1).rating, 5, "In this test documenst must be sorted by rating"s);
        ASSERT_EQUAL_HINT(docs.at(2).rating, 0, "In this test documenst must be sorted by rating"s);
        ASSERT_EQUAL_HINT(docs.at(3).rating, -1, "In this test documenst must be sorted by rating"s);
        ASSERT_EQUAL_HINT(docs.at(4).rating, -4, "In this test documenst must be sorted by rating"s);
    }

    { // Проверяем, что при отсутствии рейтинга, рейтинг будет равен 0 по умолчанию
        SearchServer server;

        server.AddDocument(1, content, status, {});

        const auto docs = server.FindTopDocuments(content, status);

        ASSERT_EQUAL_HINT(docs.size(), size_t(1), "Only one document has been added"s);
        ASSERT_EQUAL_HINT(docs.at(0).rating, 0, "If there is not ratings average rating must be 0"s);
    }

    { // Убедимся, что огромным количеством рейтинга Сервер не испугать
        SearchServer server;
        vector<int> ratings;
        const int ratings_size = 1000;
        for (int i = 0; i < ratings_size; ++i) {
            ratings.push_back(i);
        }
        const int average = (ratings.front() + ratings.back()) / 2;

        server.AddDocument(1, content, status, ratings);

        const auto docs = server.FindTopDocuments(content, status);

        ASSERT_EQUAL_HINT(docs.size(), size_t(1), "Only one document has been added"s);
        ASSERT_EQUAL_HINT(docs.at(0).rating, average, "Server has been defeated by huge amount of ratings"s);
    }
}

// Фильтрация с использованием предиката
void TestFilterPredicate() {
    const string content{"kind cat with long tail"s};
    SearchServer server;

    server.AddDocument(1, content, DocumentStatus::ACTUAL, {0, 5, 10});
    server.AddDocument(2, content, DocumentStatus::ACTUAL, {-5, 0, 35});
    server.AddDocument(3, content, DocumentStatus::IRRELEVANT, {-2, -1, -10});

    ASSERT_EQUAL(server.GetDocumentCount(), 3);

    { // Проверяем что можно найти только документы с четным id
        const auto docs = server.FindTopDocuments(content,
            [](int document_id, DocumentStatus st, int rating) {
                (void)st; (void)rating; return document_id % 2 == 0;});
        ASSERT_EQUAL_HINT(docs.size(), size_t(1), "There is the only one document with even id"s);
        ASSERT_EQUAL_HINT(docs.at(0).id, 2, "It is not even id"s);
    }

    { // Проверяем что можно ничего не найти!
        const auto docs = server.FindTopDocuments(content,
            [](int document_id, DocumentStatus st, int rating) {
                (void)document_id; (void)st; (void)rating; return false;});
        ASSERT_HINT(docs.empty(), "How could you find something with this predicate? It must be empty"s);
    }

    { // Проверяем что можно найти только документы с положительным рейтингом
        const auto docs = server.FindTopDocuments(content,
            [](int document_id, DocumentStatus st, int rating) {
                (void)document_id; (void)st; return rating > 0;});
        ASSERT_EQUAL_HINT(docs.size(), size_t(2), "There is only two documents with positive rating"s);
        // Предполагается, что релевантности равны, то есть сортировка осуществляется по рейтингу
        ASSERT_EQUAL_HINT(docs.at(0).id, 2, "Documents must be sorted by rating"s);
        ASSERT_EQUAL_HINT(docs.at(1).id, 1, "Documents must be sorted by rating"s);
    }
}

// Основная функция находится снизу, это вспомагательная
void TestDocumentsWithStatusProcess(const SearchServer& server,
                                    const string& content,
                                    const int id,
                                    const DocumentStatus status,
                                    const string& hint) {
    const auto docs = server.FindTopDocuments(content, status);
    ASSERT_EQUAL_HINT(docs.size(), size_t(1), hint);
    ASSERT_EQUAL_HINT(docs.at(0).id, id, hint);
}

// Поиск документа с заданным статусом
void TestDocumentsWithStatus() {
    const string content{"kind cat with long tail"};
    SearchServer server("with"s);

    server.AddDocument(11, content, DocumentStatus::ACTUAL, {0, 5, 10});
    server.AddDocument(21, content, DocumentStatus::BANNED, {-5, 0, 35});
    server.AddDocument(31, content, DocumentStatus::IRRELEVANT, {-2, -1, 0});

    // Проверяем, что ничего не будет найдено по несуществующему статусу
    const auto status_does_not_exist = server.FindTopDocuments(content, DocumentStatus::REMOVED);
    ASSERT_HINT(status_does_not_exist.empty(), "REMOVED status hasn't been added yet"s);

    server.AddDocument(41, content, DocumentStatus::REMOVED, {-7, -3, -5});

    // Проверяем последовательно каждый статус
    TestDocumentsWithStatusProcess(server, content, 11, DocumentStatus::ACTUAL, "Actual document, id = 11"s);
    TestDocumentsWithStatusProcess(server, content, 21, DocumentStatus::BANNED, "Banned document, id = 21"s);
    TestDocumentsWithStatusProcess(server, content, 31, DocumentStatus::IRRELEVANT, "Irrelevant document, id = 31"s);
    TestDocumentsWithStatusProcess(server, content, 41, DocumentStatus::REMOVED, "Removed document, id = 41"s);
}

// Корректное вычисление релевантности найденных документов
void TestRelevanceValue() {
    const double delta = 1e-6;
    const vector<int> rating_1{-2, -3, 7};
    const vector<int> rating_2{1, 2, 3};
    const DocumentStatus status = DocumentStatus::ACTUAL;
    const string query{"kind cat with long tail"s};
    SearchServer server("with"s);

    server.AddDocument(5, "human tail"s, status, rating_1);
    //tail 1/2
    server.AddDocument(2, "old angry fat dog with short tail"s, status, rating_1);
    //tail tf = 1/6
    server.AddDocument(1, "nasty cat beautiful tail"s, status, rating_2);
    //cat tf = 1/4
    //tail tf = 1/4
    server.AddDocument(4, "not beautiful cat"s, status, rating_1);
    //cat 1/3
    server.AddDocument(3, "huge fat parrot"s, status, rating_1);
    //no word from the query
    server.AddDocument(6, "removed cat"s, DocumentStatus::REMOVED, rating_1);
    //removed document

    //idf:
    //kind - doesn't occur
    //cat - log(6/3)
    //with - stop word
    //long - doesn't occur
    //tail - log(6/3)

    //1 - 1/4 * log(6/3) + 1/4 * log(6/3) = 0.34657359027997264
    //2 - 1/6 * log(6/3) = 0.11552453009332421
    //3 - 0 = 0
    //4 - 1/3 * log(6/3) = 0.23104906018664842
    //5 - 1/2 * log(6/3) = 0.34657359027997264
    //6 - 0 = 0
    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 6, "Only 6 documents have been added"s);

    const auto docs = server.FindTopDocuments(query, status);
    ASSERT_EQUAL_HINT(docs.size(), size_t(4), "Not all of the documents have words from the query"s);

    ASSERT(InTheVicinity(docs.at(0).relevance, 0.3465735, delta));
    ASSERT_EQUAL_HINT(docs.at(0).id, 1, "Two documents have equal relevance, but their ratings are different"s);

    ASSERT(InTheVicinity(docs.at(1).relevance, 0.3465735, delta));
    ASSERT_EQUAL_HINT(docs.at(1).id, 5, "Two documents have equal relevance, but their ratings are different"s);

    ASSERT(InTheVicinity(docs.at(2).relevance, 0.2310490, delta));
    ASSERT_EQUAL(docs.at(2).id, 4);

    ASSERT(InTheVicinity(docs.at(3).relevance, 0.1155245, delta));
    ASSERT_EQUAL(docs.at(3).id, 2);
}

// Проверка метода возврата частот
void TestGetWordFrequencies() {
    const double delta = 1e-6;
    SearchServer server("you are in the has this oh my"s);

    server.AddDocument(5, "Hello Kitty you are in the city"s, DocumentStatus::ACTUAL, { 1 });
    // 3 words
    // All have tf = 1/3
    server.AddDocument(10, "Sweety pretty Kitty has lost in this city oh my god poor Kitty"s, DocumentStatus::ACTUAL, { 2 });
    // 8 words (7 unique words)
    // Sweety pretty lost city god tf = 1/8
    // Kitty tf = 2/8 = 1/4

    ASSERT_HINT(server.GetWordFrequencies(0).empty(), "Server doesn't has id = 0, result must be empty"s);

    {
        map<string, double> word_frequency_id_5 = server.GetWordFrequencies(5);

        ASSERT_EQUAL_HINT(word_frequency_id_5.size(), size_t(3), "Document with id = 5 has 3 words"s);

        ASSERT_HINT(word_frequency_id_5.count("Hello"s) == 1, "Document with id = 5 has 1 word 'Hello'"s);
        ASSERT_HINT(InTheVicinity(word_frequency_id_5.at("Hello"s), 1.0 / 3.0, delta), "The word 'Hello' has frequency 1/3"s);

        ASSERT_HINT(word_frequency_id_5.count("Kitty"s) == 1, "Document with id = 5 has 1 word 'Kitty'"s);
        ASSERT_HINT(InTheVicinity(word_frequency_id_5.at("Kitty"s), 1.0 / 3.0, delta), "The word 'Kitty' has frequency 1/3"s);

        ASSERT_HINT(word_frequency_id_5.count("city"s) == 1, "Document with id = 5 has 1 word 'city'"s);
        ASSERT_HINT(InTheVicinity(word_frequency_id_5.at("city"s), 1.0 / 3.0, delta), "The word 'city' has frequency 1/3"s);
    }

    {
        map<string, double> word_frequency_id_10 = server.GetWordFrequencies(10);

        ASSERT_EQUAL_HINT(word_frequency_id_10.size(), size_t(7), "Document with id = 10 has 7 unique words"s);

        ASSERT_HINT(word_frequency_id_10.count("Sweety"s) == 1, "Document with id = 5 has 1 word 'Sweety'"s);
        ASSERT_HINT(InTheVicinity(word_frequency_id_10.at("Sweety"s), 1.0 / 8.0, delta), "The word 'Sweety' has frequency 1/8"s);

        ASSERT_HINT(word_frequency_id_10.count("pretty"s) == 1, "Document with id = 5 has 1 word 'pretty'"s);
        ASSERT_HINT(InTheVicinity(word_frequency_id_10.at("pretty"s), 1.0 / 8.0, delta), "The word 'pretty' has frequency 1/8"s);

        ASSERT_HINT(word_frequency_id_10.count("lost"s) == 1, "Document with id = 5 has 1 word 'lost'"s);
        ASSERT_HINT(InTheVicinity(word_frequency_id_10.at("lost"s), 1.0 / 8.0, delta), "The word 'lost' has frequency 1/8"s);

        ASSERT_HINT(word_frequency_id_10.count("city"s) == 1, "Document with id = 5 has 1 word 'city'"s);
        ASSERT_HINT(InTheVicinity(word_frequency_id_10.at("city"s), 1.0 / 8.0, delta), "The word 'city' has frequency 1/8"s);

        ASSERT_HINT(word_frequency_id_10.count("god"s) == 1, "Document with id = 5 has 1 word 'god'"s);
        ASSERT_HINT(InTheVicinity(word_frequency_id_10.at("god"s), 1.0 / 8.0, delta), "The word 'god' has frequency 1/8"s);

        ASSERT_HINT(word_frequency_id_10.count("Kitty"s) == 1, "Document with id = 5 has 1 word 'Kitty'"s);
        ASSERT_HINT(InTheVicinity(word_frequency_id_10.at("Kitty"s), 2.0 / 8.0, delta), "The word 'Kitty' has frequency 1/8"s);
    }
}

// Проверка метода удаления документа
void TestRemoveDocument() {
    const double delta = 1e-6;
    SearchServer server("and with as"s);

    AddDocument(server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(server, 4, "kind dog bite fat rat"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(server, 6, "fluffy snake or cat"s, DocumentStatus::ACTUAL, { 1, 2 });

    AddDocument(server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    // nasty tf = 1/4
    AddDocument(server, 3, "angry rat with black hat"s, DocumentStatus::ACTUAL, { 1, 2 });
    // black tf = 1/4
    AddDocument(server, 5, "fat fat cat"s, DocumentStatus::ACTUAL, { 1, 2 });
    // cat tf = 1/3
    AddDocument(server, 7, "sharp as hedgehog"s, DocumentStatus::ACTUAL, { 1, 2 });
    // sharp tf = 1/2

    // kind - doesn't occur
    // nasty - log(4)
    // black - log(4)
    // cat - log(4)
    // sharp - log(4)

    // 7 - 1/2 * log(4) = 0.6931471805599453
    // 5 - 1/3 * log(4) = 0.46209812037329684
    // 1 - 1/4 * log(4) = 0.34657359027997264
    // 3 - 1/4 * log(4) = 0.34657359027997264

    server.RemoveDocument(0);
    server.RemoveDocument(execution::par, 8);

    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 7, "Nothing has been removed, yet!"s);

    server.RemoveDocument(2);
    server.RemoveDocument(execution::seq, 4);
    server.RemoveDocument(execution::par, 6);

    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 4, "3 documents have been removed"s);

    // Check document_data_
    ASSERT_HINT(server.GetWordFrequencies(2).empty(), "Server doesn't has id = 2, result must be empty"s);
    ASSERT_HINT(server.GetWordFrequencies(4).empty(), "Server doesn't has id = 4, result must be empty"s);
    ASSERT_HINT(server.GetWordFrequencies(6).empty(), "Server doesn't has id = 6, result must be empty"s);

    // Check document_ids_
    for (int id : server) {
        ASSERT_HINT(id % 2 == 1, "Only odd ids has been left"s);
    }

    // Check word_to_document_freqs_
    const auto docs = server.FindTopDocuments("kind nasty black sharp cat"s);
    ASSERT_HINT(docs.size() == 4, "All documents must be found"s);

    ASSERT_EQUAL_HINT(docs.at(0).id, 7, "Max relevance has doc with id 7"s);
    ASSERT_HINT(InTheVicinity(docs.at(0).relevance, 0.6931471805599453, delta), "Wrong relevance"s);

    ASSERT_EQUAL_HINT(docs.at(1).id, 5, "Second relevance has doc with id 5"s);
    ASSERT_HINT(InTheVicinity(docs.at(1).relevance, 0.46209812037329684, delta), "Wrong relevance"s);

    ASSERT_EQUAL_HINT(docs.at(2).id, 1, "Third relevance has doc with id 1"s);
    ASSERT_HINT(InTheVicinity(docs.at(2).relevance, 0.34657359027997264, delta), "Wrong relevance"s);

    ASSERT_EQUAL_HINT(docs.at(3).id, 3, "Forth relevance has doc with id 3"s);
    ASSERT_HINT(InTheVicinity(docs.at(3).relevance, 0.34657359027997264, delta), "Wrong relevance"s);
}

// Проверка функции определения дубликатов
void TestFindDuplicateIds() {
    SearchServer search_server("and with"s);

    AddDocument(search_server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    AddDocument(search_server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // дубликат документа 2, будет удалён
    AddDocument(search_server, 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // отличие только в стоп-словах, считаем дубликатом
    AddDocument(search_server, 4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // множество слов такое же, считаем дубликатом документа 1
    AddDocument(search_server, 5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // добавились новые слова, дубликатом не является
    AddDocument(search_server, 6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
    AddDocument(search_server, 7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, { 1, 2 });

    // есть не все слова, не является дубликатом
    AddDocument(search_server, 8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // слова из разных документов, не является дубликатом
    AddDocument(search_server, 9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    vector<int> duplicates = FindDuplicateIds(search_server);

    ASSERT_EQUAL_HINT(duplicates.size(), 4, "4 duplicates must have been found"s);
    ASSERT_EQUAL_HINT(duplicates, vector<int>({ 3, 4, 5, 7 }), "Wrong duplicats sequence"s);
}

// -----------------------------------------------------------------------------

// Проверка формирования исключения в конструкторе
void TestSeachServerConstuctorException() {
    SearchServer server("in the \x12"s);
}

// Проверка формирования исключений при добавлении документа с отрицательным id
void TestAddDocumentsNegativeId() {
    SearchServer server;
    server.AddDocument(-1, "cat in the city"s, DocumentStatus::ACTUAL, {0});
}

// Проверка формирования исключений при добавлении документа с существующим id
void TestAddDocumentsExistingId() {
    SearchServer server;
    server.AddDocument(0, "cat in the city"s, DocumentStatus::ACTUAL, {0});
    server.AddDocument(0, "cat in the city"s, DocumentStatus::ACTUAL, {0});
}

// Проверка формирования исключений при добавлении документа со специальными символами
void TestAddDocumentsSpecialSymbols() {
    SearchServer server;
    server.AddDocument(0, "cat in the ci\x12ty"s, DocumentStatus::ACTUAL, {0});
}

// Проверка формирования исключений при наличии спецсимволов в методе MatchDocument
void TestMatchDocumentSpecialSymbols() {
    SearchServer server;
    server.AddDocument(0, "cat in the big city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto result = server.MatchDocument("cat city \x12"s, 0);
}

// Проверка формирования исключений при наличии двух минусов подряд в методе MatchDocument
void TestMatchDocumentDoubleMinus() {
    SearchServer server;
    server.AddDocument(0, "cat in the big city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto result = server.MatchDocument("cat --city"s, 0);
}

// Проверка формирования исключений при наличии знака минус без слова в методе MatchDocument
void TestMatchDocumentMinusWithoutWord() {
    SearchServer server;
    server.AddDocument(0, "cat in the big city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto result = server.MatchDocument("cat - city"s, 0);
}

// Проверка формирования исключений при наличии спецсимволов в методе FindTopDocuments
void TestFindTopDocumentsSpecialSymbols() {
    SearchServer server;
    server.AddDocument(0, "cat in the big city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto result = server.FindTopDocuments("cat city \x12"s);
}

// Проверка формирования исключений при наличии двух минусов подряд в методе FindTopDocuments
void TestFindTopDocumentsDoubleMinus() {
    SearchServer server;
    server.AddDocument(0, "cat in the big city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto result = server.FindTopDocuments("cat --city"s);
}

// Проверка формирования исключений при наличии знака минус без слова в методе FindTopDocuments
void TestFindTopDocumentsMinusWithoutWord() {
    SearchServer server;
    server.AddDocument(0, "cat in the big city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto result = server.FindTopDocuments("cat - city"s);
}

// Проверка выброса исключения
void TestSeachServerExceptions() {
    ASSERT_INVALID_ARGUMENT(TestSeachServerConstuctorException);
    ASSERT_INVALID_ARGUMENT(TestAddDocumentsNegativeId);
    ASSERT_INVALID_ARGUMENT(TestAddDocumentsExistingId);
    ASSERT_INVALID_ARGUMENT(TestAddDocumentsSpecialSymbols);
    ASSERT_INVALID_ARGUMENT(TestMatchDocumentSpecialSymbols);
    ASSERT_INVALID_ARGUMENT(TestMatchDocumentDoubleMinus);
    ASSERT_INVALID_ARGUMENT(TestMatchDocumentMinusWithoutWord);
    ASSERT_INVALID_ARGUMENT(TestFindTopDocumentsSpecialSymbols);
    ASSERT_INVALID_ARGUMENT(TestFindTopDocumentsDoubleMinus);
    ASSERT_INVALID_ARGUMENT(TestFindTopDocumentsMinusWithoutWord);
}

// -----------------------------------------------------------------------------

// Проверка работы функции распараллеливающей обработку нескольких запросов
void TestProcessQueries()
{
    SearchServer server;

    server.AddDocument(0, "Emperor penguins spend their entire lives on Antarctic ice and in its waters"s, DocumentStatus::ACTUAL, { 0 });
    server.AddDocument(1, "A bald eagle's white head may make it look bald"s, DocumentStatus::ACTUAL, { 0 });
    server.AddDocument(2, "The great horned owl has no horns!"s, DocumentStatus::ACTUAL, { 0 });
    server.AddDocument(3, "Flamingos are famous for their bright pink feathers"s, DocumentStatus::ACTUAL, { 0 });
    server.AddDocument(4, "Snowy white tundra swans breed in the Arctic"s, DocumentStatus::ACTUAL, { 0 });
    server.AddDocument(5, "American crows range from southern Canada throughout the United States"s, DocumentStatus::ACTUAL, { 0 });

    vector<string> queries;
    queries.push_back("Canada pink penguins"s); // 3 documents, id 0, 3 and 5
    queries.push_back("Emperor eagle's"s); // 2 documents, id 0 and 1
    queries.push_back("bald owl"s); // 2 documents, id 1 and 2
    queries.push_back("great Flamingos"s); // 2 documents, id 2 and 3
    queries.push_back("famous swans"s); // 2 documents, id 3 and 4
    queries.push_back("Snowy crows"s); // 2 documents, id 4 and 5

    vector<vector<Document>> result = ProcessQueries(server, queries);

    ASSERT_EQUAL_HINT(result.size(), 6, "Result size equals to queries amount, so it must be 6!"s);

    ASSERT_EQUAL_HINT(result.at(0).size(), 3, "On first query must have been found 3 documents"s);
    ASSERT_EQUAL_HINT(result.at(1).size(), 2, "On second query must have been found 2 documents"s);
    ASSERT_EQUAL_HINT(result.at(2).size(), 2, "On third query must have been found 2 documents"s);
    ASSERT_EQUAL_HINT(result.at(3).size(), 2, "On forth query must have been found 2 documents"s);
    ASSERT_EQUAL_HINT(result.at(4).size(), 2, "On fifth query must have been found 2 documents"s);
    ASSERT_EQUAL_HINT(result.at(5).size(), 2, "On sixth query must have been found 2 documents"s);
}

// Проверка работы функции распараллеливающей обработку нескольких запросов и возвращающей результат в "плоском" виде
void TestProcessQueriesJoined()
{
    SearchServer server;

    server.AddDocument(0, "Emperor penguins spend their entire lives on Antarctic ice and in its waters"s, DocumentStatus::ACTUAL, { 0 });
    // 13 words - sixth relevance
    server.AddDocument(1, "A bald eagle's fair head may make it look bald"s, DocumentStatus::ACTUAL, { 0 });
    // 10 words - forth relevance
    server.AddDocument(2, "The great horned owl has no horns!"s, DocumentStatus::ACTUAL, { 0 });
    // 7 words - first relevance
    server.AddDocument(3, "Flamingos are famous for their bright pink feathers"s, DocumentStatus::ACTUAL, { 0 });
    // 8 words - second relevance
    server.AddDocument(4, "Snowy white tundra swans breed in the Arctic"s, DocumentStatus::ACTUAL, { 0 });
    // 8 words - third relevance
    server.AddDocument(5, "American crows range from southern Canada throughout the United States"s, DocumentStatus::ACTUAL, { 0 });
    // 10 words - fifth relevance

    vector<string> queries;
    queries.push_back("Canada pink penguins"s); // 3 documents, id 0, 3 and 5
    queries.push_back("Emperor eagle's"s); // 2 documents, id 0 and 1
    queries.push_back("fair owl"s); // 2 documents, id 1 and 2
    queries.push_back("great Flamingos"s); // 2 documents, id 2 and 3
    queries.push_back("famous swans"s); // 2 documents, id 3 and 4
    queries.push_back("Snowy crows"s); // 2 documents, id 4 and 5

    vector<Document> result = ProcessQueriesJoined(server, queries);

    ASSERT_EQUAL_HINT(result.size(), 13, "13 documents must have been found"s);

    vector<int> right_id = { 3, 5, 0, 1, 0, 2, 1, 2, 3, 3, 4, 4, 5 };
    for (int i = 0; i < 13; ++i) {
        ASSERT_EQUAL_HINT(result.at(i).id, right_id.at(i), "Wrond id number"s);
    }
}

// -----------------------------------------------------------------------------

// Проверка работы Пагинатора
void TestPaginator() {
    SearchServer server;
    server.AddDocument(0, "cat in the city"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(1, "dog in the city"s, DocumentStatus::IRRELEVANT, {2});
    server.AddDocument(2, "mouse in the city"s, DocumentStatus::ACTUAL, {3});
    server.AddDocument(3, "dolphin in the city"s, DocumentStatus::REMOVED, {4});
    server.AddDocument(4, "lion in the city"s, DocumentStatus::ACTUAL, {5});
    server.AddDocument(5, "human in the city"s, DocumentStatus::BANNED, {6});
    server.AddDocument(6, "beaver in the city"s, DocumentStatus::ACTUAL, {7});
    server.AddDocument(7, "child in the city"s, DocumentStatus::ACTUAL, {8});
    vector<Document> docs = server.FindTopDocuments("city"s);
    Paginator pag = Paginate(docs, 2);
    ASSERT_EQUAL(pag.size(), size_t(3));
    for (const auto& page : pag) {
        size_t sz = page.size();
        ASSERT_HINT(sz > 0 && sz <= 2, "Page size must be 1 or 2"s);
    }
}

// Проверка работа очереди запросов
void TestRequestQueue() {
    SearchServer server;
    RequestQueue queue(server);
    for (int i = 0; i < 1440; i++) {
        queue.AddFindRequest("empty"s);
    }
    ASSERT_EQUAL_HINT(queue.GetNoResultRequests(), 1440, "1440 empty requests were made"s);
    server.AddDocument(0, "cat"s, DocumentStatus::ACTUAL, {1});
    for (int i = 0; i < 10; i++) {
        queue.AddFindRequest("cat"s);
    }
    ASSERT_EQUAL_HINT(queue.GetNoResultRequests(), 1430, "1430 empty requests were made"s);
}

// -----------------------------------------------------------------------------

// Функции для генерации случайных слов и случайных наборов слов

string GenerateWord(mt19937& generator, int max_word_lenght) {
    int lenght = uniform_int_distribution(1, max_word_lenght)(generator);
    string word;
    word.reserve(lenght);
    for (int i = 0; i < lenght; ++i) {
        word.push_back(uniform_int_distribution(97, 122)(generator));
    }
    return word;
}

vector<string> GenerateWords(mt19937& generator, int word_count, int max_length) {
    vector<string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i) {
        words.push_back(GenerateWord(generator, max_length));
    }
    sort(words.begin(), words.end());
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}

string GeneratePhrase(mt19937& generator, const vector<string>& words, int max_word_count_in_query, double minus_frequency = 0.0) {
    const int word_count = uniform_int_distribution(1, max_word_count_in_query)(generator);
    string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        if (uniform_real_distribution<>(0, 1)(generator) < minus_frequency) {
            query.push_back('-');
        }
        query += words[uniform_int_distribution<int>(0, words.size() - 1)(generator)];
    }
    return query;
}

vector<string> GeneratePhrases(mt19937& generator, const vector<string>& words, int query_count, int max_word_count_in_query) {
    vector<string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GeneratePhrase(generator, words, max_word_count_in_query));
    }
    return queries;
}

// -----------------------------------------------------------------------------

// Проверка скорости метода удаления документа
void TestRemoveDocumentSpeed() {
    mt19937 generator;
    vector<string> words = GenerateWords(generator, 10'000, 25);
    vector<string> phrases = GeneratePhrases(generator, words, 10'000, 100);

    SearchServer server;
    size_t max_id = phrases.size();
    for (size_t i = 0; i < max_id; ++i) {
        server.AddDocument(i, phrases.at(i), DocumentStatus::ACTUAL, { 0 });
    }

    ASSERT_DURATION_MILLISECONDS(500);
    for (size_t i = 0; i < max_id; ++i) {
        server.RemoveDocument(execution::par, i);
    }

    ASSERT_EQUAL(server.GetDocumentCount(), 0);
}

// Проверка скорости метода матчинга документа
void TestMatchDocumentSpeed() {
    mt19937 generator;
    vector<string> words = GenerateWords(generator, 10'000, 10);
    vector<string> phrases = GeneratePhrases(generator, words, 10'000, 70);

    SearchServer server;
    size_t max_id = phrases.size();
    for (size_t i = 0; i < max_id; ++i) {
        server.AddDocument(i, phrases.at(i), DocumentStatus::ACTUAL, { 0 });
    }

    const string query = GeneratePhrase(generator, words, 500, 0.1);

    ASSERT_DURATION_MILLISECONDS(1000);
    int words_count = 0;
    for (size_t i = 0; i < max_id; ++i) {
        const auto [words, status] = server.MatchDocument(execution::par, query, i);
        //cout << words.size() << endl;
        //words_count += words.size();
    }
    //cout << words_count << endl;
}

// --------- Окончание модульных тестов поисковой системы -----------

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestAddDocuments);
    RUN_TEST(TestStopWords);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestSortRelevance);
    RUN_TEST(TestRating);
    RUN_TEST(TestFilterPredicate);
    RUN_TEST(TestDocumentsWithStatus);
    RUN_TEST(TestRelevanceValue);
    RUN_TEST(TestGetWordFrequencies);
    RUN_TEST(TestRemoveDocument);
    RUN_TEST(TestFindDuplicateIds);
    RUN_TEST(TestSeachServerExceptions);
    RUN_TEST(TestProcessQueries);
    RUN_TEST(TestProcessQueriesJoined);
    RUN_TEST(TestPaginator);
    RUN_TEST(TestRequestQueue);

#ifndef _DEBUG
    RUN_TEST(TestRemoveDocumentSpeed);
    RUN_TEST(TestMatchDocumentSpeed);
#endif
}
