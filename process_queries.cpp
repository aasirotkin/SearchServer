#include "process_queries.h"

#include <algorithm>
#include <execution>
#include <iterator>

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> res(queries.size());

    std::transform(
        std::execution::par,
        queries.begin(), queries.end(), res.begin(),
        [&search_server](const std::string query) {
            return search_server.FindTopDocuments(query);
        });

    return res;
}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> hiden_documents = ProcessQueries(search_server, queries);
    std::vector<Document> res;
    for (std::vector<Document>& documents : hiden_documents) {
        std::move(documents.begin(), documents.end(), std::back_inserter(res));
    }
    return res;
}
