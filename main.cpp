#include "searchserver.h"

#include <iostream>

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

// -------- Начало модульных тестов поисковой системы ----------

// Добавление документов
void TestAddDocuments() {
    const int id_actual = 42;
    const int id_banned = 61;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3, 4, 5};

    SearchServer server;
    server.AddDocument(id_actual, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(id_banned, content, DocumentStatus::BANNED, ratings);
    ASSERT_EQUAL(server.GetDocumentCount(), 2);

    // Сначала убеждаемся, что документы добавлены и могут быть найдены
    const auto found_docs = server.FindTopDocuments("cat"s);
    ASSERT_EQUAL(found_docs.size(), size_t(1));
    ASSERT_EQUAL(found_docs.at(0).id, id_actual);

    // Убедимся, что лишние документы найдены не будут
    const auto found_empty_docs = server.FindTopDocuments("dog"s);
    ASSERT_HINT(found_empty_docs.empty(), "There is not document with dog word"s);
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
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
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
    vector<Document> docs_1 = server.FindTopDocuments("cat or dog in the -garden"s);
    ASSERT_EQUAL(docs_1.size(), size_t(1));
    ASSERT_EQUAL(docs_1.at(0).id, id_1);

    // Убедимся, что минус слово отсекает первый документ
    vector<Document> docs_2 = server.FindTopDocuments("cat or dog in the -city"s);
    ASSERT_EQUAL(docs_2.size(), size_t(1));
    ASSERT_EQUAL(docs_2.at(0).id, id_2);

    // Убедимся, что минус слово работает для обоих документов
    vector<Document> docs_3 = server.FindTopDocuments("rat -in the space"s);
    ASSERT_HINT(docs_3.empty(), "Documents with minus words must be excluded"s);

    // Убедимся, что минус слово которого нет в обоих документах не повлияет на результат
    vector<Document> docs_4 = server.FindTopDocuments("-rat in the space"s);
    ASSERT_EQUAL(docs_4.size(), size_t(2));
    ASSERT_EQUAL(docs_4.at(0).id, id_1);
    ASSERT_EQUAL(docs_4.at(1).id, id_2);
}

// Основная функция находится снизу, это вспомагательная
// данная функция может быть использована только в связке с TestMatchDocument
// у TestMatchDocument дожены быть документы со строкой "cat in the big city"
void TestMatchDocumentStatus(const SearchServer& server, const int id,
                             const DocumentStatus status) {
    const auto [words_1, status_1] = server.MatchDocument("cat -city"s, id);
    ASSERT_HINT(words_1.empty(), "Query contains minus word"s);
    ASSERT_HINT(status_1 == status, "Status must be correct"s);

    const auto [words_2, status_2] = server.MatchDocument("cat city -fake"s, id);
    ASSERT_EQUAL(words_2.size(), size_t(2));
    // Здесь проверяется лексикографический порядок слова
    ASSERT_EQUAL_HINT(words_2.at(0), "cat"s, "Words order must be lexicographical"s);
    ASSERT_EQUAL_HINT(words_2.at(1), "city"s, "Words order must be lexicographical"s);
    ASSERT_HINT(status_2 == status, "Status must be correct"s);
}

// Матчинг документов
void TestMatchDocument() {
    SearchServer server;
    const vector<int> ratings = {1, 2, 3};
    const string content{"cat in the big city"};

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

// Фильтрация с использованием предиката
void TestFilterPredicate() {
    const string content{"kind cat with long tail"s};
    SearchServer server;

    server.AddDocument(1, content, DocumentStatus::ACTUAL, {0, 5, 10});
    server.AddDocument(2, content, DocumentStatus::ACTUAL, {-5, 0, 35});
    server.AddDocument(3, content, DocumentStatus::IRRELEVANT, {-2, -1, -10});

    ASSERT_EQUAL(server.GetDocumentCount(), 3);

    { // Проверяем что можно найти только документы с четным id
        const vector<Document> docs = server.FindTopDocuments(content,
            [](int document_id, DocumentStatus st, int rating) {
                (void)st; (void)rating; return document_id % 2 == 0;});
        ASSERT_EQUAL_HINT(docs.size(), size_t(1), "There is the only one document with even id"s);
        ASSERT_EQUAL_HINT(docs.at(0).id, 2, "It is not even id"s);
    }

    { // Проверяем что можно ничего не найти!
        const vector<Document> docs = server.FindTopDocuments(content,
            [](int document_id, DocumentStatus st, int rating) {
                (void)document_id; (void)st; (void)rating; return false;});
        ASSERT_HINT(docs.empty(), "How could you find something with this predicate? It must be empty"s);
    }

    { // Проверяем что можно найти только документы с положительным рейтингом
        const vector<Document> docs = server.FindTopDocuments(content,
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
    SearchServer server;

    server.SetStopWords("with");

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
    SearchServer server;

    server.SetStopWords("with");

    server.AddDocument(5, "human tail", status, rating_1);
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

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestAddDocuments);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestSortRelevance);
    RUN_TEST(TestRating);
    RUN_TEST(TestFilterPredicate);
    RUN_TEST(TestDocumentsWithStatus);
    RUN_TEST(TestRelevanceValue);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    return 0;
}
