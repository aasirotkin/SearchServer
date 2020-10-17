#include "searchserver.h"

#include <deque>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace std;

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
    catch(const out_of_range& error) {
        AssertImpl(code == ErrorCode::OUT_OF_RANGE, ErrorCodeName(code), file, func_name, line, ErrorCodeHint(code));
    }
    catch(const invalid_argument& error) {
        AssertImpl(code == ErrorCode::INVALID_ARGUMENT, ErrorCodeName(code), file, func_name, line, ErrorCodeHint(code));
    }
    catch(...) {
        AssertImpl(false, ErrorCodeName(code), file, func_name, line, ErrorCodeHint(code));
    }
    cerr << func_name << " OK" << endl;
}

#define ASSERT_OUT_OF_RANGE(a) AssertTrowImpl((a), ErrorCode::OUT_OF_RANGE, __FILE__, #a, __LINE__)

#define ASSERT_INVALID_ARGUMENT(a) AssertTrowImpl((a), ErrorCode::INVALID_ARGUMENT, __FILE__, #a, __LINE__)

// -------- Начало модульных тестов поисковой системы ----------

// Добавление документов
void TestAddDocuments() {
    const int id_actual = 42;
    const int id_banned = 61;
    const int id_empty = 14;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3, 4, 5};

    SearchServer server;
    (void)server.AddDocument(id_actual, content, DocumentStatus::ACTUAL, ratings);
    (void)server.AddDocument(id_banned, content, DocumentStatus::BANNED, ratings);
    (void)server.AddDocument(id_empty, ""s, DocumentStatus::ACTUAL, ratings);
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
        (void)server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
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

    (void)server.AddDocument(id_1, "cat in the city"s, status, ratings);
    (void)server.AddDocument(id_2, "dog in the garden"s, status, ratings);
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

    (void)server.AddDocument(64, content, DocumentStatus::ACTUAL, ratings);
    (void)server.AddDocument(12, content, DocumentStatus::BANNED, ratings);
    (void)server.AddDocument(51, content, DocumentStatus::IRRELEVANT, ratings);
    (void)server.AddDocument(75, content, DocumentStatus::REMOVED, ratings);

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

    (void)server.AddDocument(6, "human tail"s, status, rating);
    (void)server.AddDocument(5, "old angry fat dog with short tail"s, status, rating);
    (void)server.AddDocument(4, "nasty cat beautiful tail"s, status, rating);
    (void)server.AddDocument(3, "not beautiful cat"s, status, rating);
    (void)server.AddDocument(2, "huge fat parrot"s, status, rating);
    (void)server.AddDocument(1, "removed cat"s, status, rating);

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

        (void)server.AddDocument(1, content, status, {0});
        (void)server.AddDocument(2, content, status, {0, 5, 10});
        (void)server.AddDocument(3, content, status, {-2, -1, 0});
        (void)server.AddDocument(4, content, status, {-5, 0, 35});
        (void)server.AddDocument(5, content, status, {-7, -3, -5});
        (void)server.AddDocument(6, content, status, {-7, -2});
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

        (void)server.AddDocument(1, content, status, ratings);

        const auto docs = server.FindTopDocuments(content, status);

        ASSERT_EQUAL_HINT(docs.size(), size_t(1), "Only one document has been added"s);
        ASSERT_EQUAL_HINT(docs.at(0).rating, average, "Server has been defeated by huge amount of ratings"s);
    }
}

// Фильтрация с использованием предиката
void TestFilterPredicate() {
    const string content{"kind cat with long tail"s};
    SearchServer server;

    (void)server.AddDocument(1, content, DocumentStatus::ACTUAL, {0, 5, 10});
    (void)server.AddDocument(2, content, DocumentStatus::ACTUAL, {-5, 0, 35});
    (void)server.AddDocument(3, content, DocumentStatus::IRRELEVANT, {-2, -1, -10});

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

    (void)server.AddDocument(11, content, DocumentStatus::ACTUAL, {0, 5, 10});
    (void)server.AddDocument(21, content, DocumentStatus::BANNED, {-5, 0, 35});
    (void)server.AddDocument(31, content, DocumentStatus::IRRELEVANT, {-2, -1, 0});

    // Проверяем, что ничего не будет найдено по несуществующему статусу
    const auto status_does_not_exist = server.FindTopDocuments(content, DocumentStatus::REMOVED);
    ASSERT_HINT(status_does_not_exist.empty(), "REMOVED status hasn't been added yet"s);

    (void)server.AddDocument(41, content, DocumentStatus::REMOVED, {-7, -3, -5});

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

    (void)server.AddDocument(5, "human tail"s, status, rating_1);
    //tail 1/2
    (void)server.AddDocument(2, "old angry fat dog with short tail"s, status, rating_1);
    //tail tf = 1/6
    (void)server.AddDocument(1, "nasty cat beautiful tail"s, status, rating_2);
    //cat tf = 1/4
    //tail tf = 1/4
    (void)server.AddDocument(4, "not beautiful cat"s, status, rating_1);
    //cat 1/3
    (void)server.AddDocument(3, "huge fat parrot"s, status, rating_1);
    //no word from the query
    (void)server.AddDocument(6, "removed cat"s, DocumentStatus::REMOVED, rating_1);
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

// Проверка метода возврата id номера
void TestGetDocumentId() {
    SearchServer server;
    (void)server.AddDocument(0, "cat in the city"s, DocumentStatus::ACTUAL, {0});
    (void)server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {0});
    (void)server.AddDocument(2, "cat in the city"s, DocumentStatus::ACTUAL, {0});
    (void)server.AddDocument(3, "cat in the city"s, DocumentStatus::ACTUAL, {0});
    (void)server.AddDocument(4, "cat in the city"s, DocumentStatus::ACTUAL, {0});
    (void)server.AddDocument(5, "cat in the city"s, DocumentStatus::ACTUAL, {0});
    ASSERT(server.GetDocumentId(4) == 4);
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

// Проверка формирования исключений при получении id по отрицательному индексу
void TestGetDocumentIdNegativeIndex() {
    SearchServer server;
    server.GetDocumentId(-1);
}

// Проверка формирования исключений при получении id по индексу превышающему число документов
void TestGetDocumentIdIndexMoreThanDocumentCount() {
    SearchServer server;
    server.AddDocument(0, "cat in the city"s, DocumentStatus::ACTUAL, {0});
    server.GetDocumentId(1);
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

// -----------------------------------------------------------------------------

// Проверка выброса исключения
void TestSeachServerExceptions() {
    ASSERT_INVALID_ARGUMENT(TestSeachServerConstuctorException);
    ASSERT_INVALID_ARGUMENT(TestAddDocumentsNegativeId);
    ASSERT_INVALID_ARGUMENT(TestAddDocumentsExistingId);
    ASSERT_INVALID_ARGUMENT(TestAddDocumentsSpecialSymbols);
    ASSERT_OUT_OF_RANGE(TestGetDocumentIdNegativeIndex);
    ASSERT_OUT_OF_RANGE(TestGetDocumentIdIndexMoreThanDocumentCount);
    ASSERT_INVALID_ARGUMENT(TestMatchDocumentSpecialSymbols);
    ASSERT_INVALID_ARGUMENT(TestMatchDocumentDoubleMinus);
    ASSERT_INVALID_ARGUMENT(TestMatchDocumentMinusWithoutWord);
    ASSERT_INVALID_ARGUMENT(TestFindTopDocumentsSpecialSymbols);
    ASSERT_INVALID_ARGUMENT(TestFindTopDocumentsDoubleMinus);
    ASSERT_INVALID_ARGUMENT(TestFindTopDocumentsMinusWithoutWord);
}

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
    RUN_TEST(TestGetDocumentId);
    RUN_TEST(TestSeachServerExceptions);
}

// --------- Окончание модульных тестов поисковой системы -----------

template <typename Iterator>
class IteratorRange {
public:
    explicit IteratorRange(Iterator begin, Iterator end)
        : begin_(begin)
        , end_(end)
        , size_(end - begin) {

    }

    Iterator begin() const {
        return begin_;
    }

    Iterator end() const {
        return end_;
    }

    size_t size() const {
        return size_;
    }

private:
    Iterator begin_;
    Iterator end_;
    size_t size_;
};

ostream& operator<< (ostream& out, const Document& doc) {
    out << doc.Str();
    return out;
}

template <typename Iterator>
ostream& operator<< (ostream& out, const IteratorRange<Iterator>& range) {
    for (auto it : range) {
        out << it;
    }
    return out;
}

template <typename Iterator>
class Paginator {
public:
    explicit Paginator(Iterator begin, Iterator end, int size) {
        while (begin != end) {
            int dist = end - begin;
            int shift = (dist < size) ? dist : size;
            pages_.push_back(IteratorRange(begin, begin + shift));
            begin = begin + shift;
        }
    }

    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }

private:
    vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server)
        : server_(search_server) {

    }

    template <typename DocumentPredicate>
    vector<Document> AddFindRequest(const string& raw_query, DocumentPredicate document_predicate) {
        vector<Document> docs = server_.FindTopDocuments(raw_query, document_predicate);
        requests_.push_back({docs.size()});
        if (requests_.size() > sec_in_day_) {
            requests_.pop_front();
        }
        return docs;
    }

    vector<Document> AddFindRequest(const string& raw_query, DocumentStatus status) {
        return AddFindRequest(raw_query,
        [status](int document_id, DocumentStatus st, int rating) { (void)document_id; (void)rating; return status == st; });
    }

    vector<Document> AddFindRequest(const string& raw_query) {
        return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
    }

    int GetNoResultRequests() const {
        int no_amount = 0;
        for (const QueryResult& res : requests_) {
            if (res.amount == 0) {
                no_amount++;
            }
        }
        return no_amount;
    }
private:
    struct QueryResult {
        size_t amount;
    };
    deque<QueryResult> requests_;
    const static int sec_in_day_ = 1440;
    const SearchServer& server_;
};

int main() {
    TestSearchServer();
    return 0;
}
