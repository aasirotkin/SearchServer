#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer &search_server)
    : server_(search_server) {

}

std::vector<Document> RequestQueue::AddFindRequest(const std::string &raw_query, DocumentStatus status) {
    return AddFindRequest(raw_query,
    [status](int document_id, DocumentStatus st, int rating) { (void)document_id; (void)rating; return status == st; });
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string &raw_query) {
    return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}

int RequestQueue::GetNoResultRequests() const {
    int no_amount = 0;
    for (const QueryResult& res : requests_) {
        if (res.amount == 0) {
            no_amount++;
        }
    }
    return no_amount;
}
