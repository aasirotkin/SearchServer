#include "searchserver.h"

#include <iostream>
#include <limits>

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
    const int id_empty = 14;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3, 4, 5};

    SearchServer server;
    (void)server.AddDocument(id_actual, content, DocumentStatus::ACTUAL, ratings);
    (void)server.AddDocument(id_banned, content, DocumentStatus::BANNED, ratings);
    (void)server.AddDocument(id_empty, ""s, DocumentStatus::ACTUAL, ratings);
    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 3, "Only 2 documents have been added"s);

    { // Сначала убеждаемся, что документы добавлены и могут быть найдены
        const auto result = server.FindTopDocuments("cat"s);
        ASSERT(result.has_value());
        vector<Document> found_docs = result.value();
        ASSERT_EQUAL_HINT(found_docs.size(), size_t(1), "Only one not empty document with ACTUAL status has been added"s);
        ASSERT_EQUAL_HINT(found_docs.at(0).id, id_actual, "This is not document with ACTUAL status"s);
    }

    { // Убедимся, что лишние документы найдены не будут
        const auto result = server.FindTopDocuments("dog"s);
        ASSERT(result.has_value());
        vector<Document> found_docs = result.value();
        ASSERT_HINT(found_docs.empty(), "There is not document with dog word"s);
    }
}

// Дополнительные тесты на отсеивание неправильных id и спецслов
void TestAddDocumentsSpecialWordsAndWrongId() {
    SearchServer server;
    ASSERT(server.AddDocument(-1, "cat in the city"s, DocumentStatus::ACTUAL, {0}) == false);
    ASSERT(server.AddDocument(0, "cat in the city"s, DocumentStatus::ACTUAL, {0}) == true);
    ASSERT(server.AddDocument(0, "cat in the city"s, DocumentStatus::ACTUAL, {0}) == false);
    ASSERT(server.AddDocument(1, "cat in the ci\x12ty"s, DocumentStatus::ACTUAL, {0}) == false);
    ASSERT_EQUAL(server.GetDocumentCount(), 1);
}

// Дополнительные тесты на отсеивание в случае наличия неправильных минус слов или спецслов в методе FindTopDocuments
void TestFindTopDocumentsWrongQuery() {
    SearchServer server;
    void(server.AddDocument(0, "cat in the city"s, DocumentStatus::ACTUAL, {0}));
    void(server.AddDocument(1, "dog in the town"s, DocumentStatus::ACTUAL, {0}));

    { // Проверяем, что если минус слово содержит лишний минус, то ничего не будет найдено
        const auto result = server.FindTopDocuments("cat --city"s);
        ASSERT(!result.has_value());
    }

    { // Проверяем, что если после знака минус ничего нет, то ничего не будет найдено
        const auto result = server.FindTopDocuments("cat -"s);
        ASSERT(!result.has_value());
    }

    { // Проверяем, что если знак минус написан между словами, то документ найден будет
        const auto result = server.FindTopDocuments("cat in-the city"s);
        ASSERT(result.has_value());
        vector<Document> docs = result.value();
        ASSERT(docs.size() == size_t(1));
    }

    { // Проверяем, что если есть спец слово, то ничего не будет найдено
        const auto result = server.FindTopDocuments("cat \x12"s);
        ASSERT(!result.has_value());
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
        const auto result = server.FindTopDocuments("in"s);
        ASSERT(result.has_value());
        vector<Document> found_docs = result.value();
        ASSERT_EQUAL(found_docs.size(), size_t(1));
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server("in the"s);
        ASSERT(server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings) == true);
        const auto result = server.FindTopDocuments("in"s);
        ASSERT(result.has_value());
        vector<Document> found_docs = result.value();
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
    const auto result_1 = server.FindTopDocuments("cat or dog in the -garden"s);
    ASSERT(result_1.has_value());
    vector<Document> docs_1 = result_1.value();
    ASSERT_EQUAL(docs_1.size(), size_t(1));
    ASSERT_EQUAL(docs_1.at(0).id, id_1);

    // Убедимся, что минус слово отсекает первый документ
    const auto result_2 = server.FindTopDocuments("cat or dog in the -city"s);
    ASSERT(result_2.has_value());
    vector<Document> docs_2 = result_2.value();
    ASSERT_EQUAL(docs_2.size(), size_t(1));
    ASSERT_EQUAL(docs_2.at(0).id, id_2);

    // Убедимся, что минус слово работает для обоих документов
    const auto result_3 = server.FindTopDocuments("rat -in the space"s);
    ASSERT(result_3.has_value());
    vector<Document> docs_3 = result_3.value();
    ASSERT_HINT(docs_3.empty(), "Documents with minus words must be excluded"s);

    // Убедимся, что минус слово которого нет в обоих документах не повлияет на результат
    const auto result_4 = server.FindTopDocuments("-rat in the space"s);
    ASSERT(result_4.has_value());
    vector<Document> docs_4 = result_4.value();
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
        const auto result = server.MatchDocument("cat -city"s, id);
        ASSERT_HINT(result.has_value(), "This query is fine"s);
        const auto [words, status_out] = result.value();
        ASSERT_HINT(words.empty(), "Query contains minus word"s);
        ASSERT_HINT(status_out == status, "Status must be correct"s);
    }

    { // Убедимся, что наличие минус слова, которого нет в документе, не повлияет на результат
        const auto result = server.MatchDocument("cat city -fake"s, id);
        ASSERT_HINT(result.has_value(), "This query is fine"s);
        const auto [words, status_out] = result.value();
        ASSERT_EQUAL(words.size(), size_t(2));
        // Здесь проверяется лексикографический порядок слова
        ASSERT_EQUAL_HINT(words.at(0), "cat"s, "Words order must be lexicographical"s);
        ASSERT_EQUAL_HINT(words.at(1), "city"s, "Words order must be lexicographical"s);
        ASSERT_HINT(status_out == status, "Status must be correct"s);
    }

    { // Убедимся, что при наличии спецсимволов в запросе ничего найдено не будет
        const auto result = server.MatchDocument("cat city \x12"s, id);
        ASSERT_HINT(!result.has_value(), "Query contains special symbols"s);
    }

    { // Убедимся, что если минус слово содержит два минуса, то ничего найдено не будет
        const auto result = server.MatchDocument("cat --city"s, id);
        ASSERT_HINT(!result.has_value(), "Minus word contains double minus sign"s);
    }

    { // Убедимся, что если после знака минус нет слова, то ничего найдено не будет
        const auto result = server.MatchDocument("cat - city"s, id);
        ASSERT_HINT(!result.has_value(), "Minus sign without word"s);
    }

    { // Убедимся, что знак минус между словами не считается минус словом
        const auto result = server.MatchDocument("cat in the big-city"s, id);
        ASSERT_HINT(result.has_value(), "This minus sign is between words, so it is fine"s);
        const auto [words, status_out] = result.value();
        ASSERT_EQUAL(words.size(), size_t(3));
        ASSERT_HINT(status_out == status, "Status must be correct"s);
    }
}

// Матчинг документов
void TestMatchDocument() {
    SearchServer server;
    const vector<int> ratings = {1, 2, 3};
    const string content{"cat in the big city"};

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

    const auto result = server.FindTopDocuments(query);
    ASSERT(result.has_value());
    vector<Document> docs = result.value();
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

        const auto result = server.FindTopDocuments(content, status);
        ASSERT(result.has_value());
        vector<Document> docs = result.value();
        ASSERT_EQUAL_HINT(docs.size(), size_t(5), "Maximus documents count equals to 5"s);
        ASSERT_EQUAL_HINT(docs.at(0).rating, 10, "In this test documenst must be sorted by rating"s);
        ASSERT_EQUAL_HINT(docs.at(1).rating, 5, "In this test documenst must be sorted by rating"s);
        ASSERT_EQUAL_HINT(docs.at(2).rating, 0, "In this test documenst must be sorted by rating"s);
        ASSERT_EQUAL_HINT(docs.at(3).rating, -1, "In this test documenst must be sorted by rating"s);
        ASSERT_EQUAL_HINT(docs.at(4).rating, -4, "In this test documenst must be sorted by rating"s);
    }

    { // Проверяем, что при отсутствии рейтинга, рейтинг будет равен 0 по умолчанию
        SearchServer server;

        ASSERT(server.AddDocument(1, content, status, {}) == true);

        const auto result = server.FindTopDocuments(content, status);
        ASSERT(result.has_value());
        vector<Document> docs = result.value();

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

        const auto result = server.FindTopDocuments(content, status);
        ASSERT(result.has_value());
        vector<Document> docs = result.value();

        ASSERT_EQUAL_HINT(docs.size(), size_t(1), "Only one document has been added"s);
        ASSERT_EQUAL_HINT(docs.at(0).rating, average, "Server has been defeated by huge amount of ratings"s);
    }

    { // Проверяем поведение для экстремально больших чисел рейтинга
        SearchServer server;
        const int halh_max = 0.5 * numeric_limits<int>::max();
        const int quarter_max = 0.5 * halh_max;
        const int average = quarter_max + 1;
        // q = quarter_max
        // 2q = halh_max

        vector<int> ratings_1 = {halh_max, quarter_max, quarter_max, quarter_max, 5};
        // (2q + q + q + q + 5) / 5 = q + 1

        vector<int> ratings_2 = {halh_max, quarter_max, quarter_max, 5, quarter_max};
        // (2q + q + q + 5 + q) / 5 = q + 1

        vector<int> ratings_3 = {5, halh_max, quarter_max, quarter_max, quarter_max};
        // (5 + 2q + q + q + q) / 5 = q + 1

        (void)server.AddDocument(1, content, status, ratings_1);
        (void)server.AddDocument(2, content, status, ratings_2);
        (void)server.AddDocument(3, content, status, ratings_3);

        const auto result = server.FindTopDocuments(content, status);
        ASSERT(result.has_value());
        vector<Document> docs = result.value();

        ASSERT_EQUAL_HINT(docs.size(), size_t(3), "Only three documents have been added"s);
        ASSERT_EQUAL_HINT(docs.at(0).rating, average, "Overflow has been occured"s);
        ASSERT_EQUAL_HINT(docs.at(1).rating, average, "Overflow has been occured"s);
        ASSERT_EQUAL_HINT(docs.at(2).rating, average, "Overflow has been occured"s);
    }

    { // Проверяем поведение для экстремально маленьких чисел рейтинга
        SearchServer server;
        const int halh_min = 0.5 * numeric_limits<int>::min();
        const int quarter_min = 0.5 * halh_min;
        const int average = quarter_min + 1;
        // q = quarter_min
        // 2q = halh_min

        vector<int> ratings_1 = {halh_min, quarter_min, quarter_min, quarter_min, 5};
        // (2q + q + q + q + 5) / 5 = q + 1

        vector<int> ratings_2 = {halh_min, quarter_min, quarter_min, 5, quarter_min};
        // (2q + q + q + 5 + q) / 5 = q + 1

        vector<int> ratings_3 = {5, halh_min, quarter_min, quarter_min, quarter_min};
        // (5 + 2q + q + q + q) / 5 = q + 1

        (void)server.AddDocument(1, content, status, ratings_1);
        (void)server.AddDocument(2, content, status, ratings_2);
        (void)server.AddDocument(3, content, status, ratings_3);

        const auto result = server.FindTopDocuments(content, status);
        ASSERT(result.has_value());
        vector<Document> docs = result.value();

        ASSERT_EQUAL_HINT(docs.size(), size_t(3), "Only three documents have been added"s);
        ASSERT_EQUAL_HINT(docs.at(0).rating, average, "Underflow has been occured"s);
        ASSERT_EQUAL_HINT(docs.at(1).rating, average, "Underflow has been occured"s);
        ASSERT_EQUAL_HINT(docs.at(2).rating, average, "Underflow has been occured"s);
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
        const auto result = server.FindTopDocuments(content,
            [](int document_id, DocumentStatus st, int rating) {
                (void)st; (void)rating; return document_id % 2 == 0;});
        ASSERT(result.has_value());
        vector<Document> docs = result.value();
        ASSERT_EQUAL_HINT(docs.size(), size_t(1), "There is the only one document with even id"s);
        ASSERT_EQUAL_HINT(docs.at(0).id, 2, "It is not even id"s);
    }

    { // Проверяем что можно ничего не найти!
        const auto result = server.FindTopDocuments(content,
            [](int document_id, DocumentStatus st, int rating) {
                (void)document_id; (void)st; (void)rating; return false;});
        ASSERT(result.has_value());
        vector<Document> docs = result.value();
        ASSERT_HINT(docs.empty(), "How could you find something with this predicate? It must be empty"s);
    }

    { // Проверяем что можно найти только документы с положительным рейтингом
        const auto result = server.FindTopDocuments(content,
            [](int document_id, DocumentStatus st, int rating) {
                (void)document_id; (void)st; return rating > 0;});
        ASSERT(result.has_value());
        vector<Document> docs = result.value();
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
    const auto result = server.FindTopDocuments(content, status);
    ASSERT(result.has_value());
    vector<Document> docs = result.value();
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
    const auto result = server.FindTopDocuments(content, DocumentStatus::REMOVED);
    ASSERT(result.has_value());
    vector<Document> status_does_not_exist = result.value();
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

    const auto result = server.FindTopDocuments(query, status);
    ASSERT(result.has_value());
    vector<Document> docs = result.value();
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
    ASSERT_EQUAL(server.GetDocumentId(-1), SearchServer::INVALID_DOCUMENT_ID);
    ASSERT_EQUAL(server.GetDocumentId(6), SearchServer::INVALID_DOCUMENT_ID);
    ASSERT_EQUAL(server.GetDocumentId(10), SearchServer::INVALID_DOCUMENT_ID);
    ASSERT(server.GetDocumentId(4) == 4);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestAddDocuments);
    RUN_TEST(TestAddDocumentsSpecialWordsAndWrongId);
    RUN_TEST(TestFindTopDocumentsWrongQuery);
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
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    return 0;
}
