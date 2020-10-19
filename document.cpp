#include "document.h"

#include <cmath>
#include <iostream>

using namespace std::string_literals;

const double MAX_RELEVANCE_ACCURACY = 1e-6;

Document::Document(int input_id, double input_relevance, int input_rating)
    : id(input_id)
    , relevance(input_relevance)
    , rating(input_rating) {

}

std::string Document::Str() const {
    return "{ "s +
           "document_id = "s + std::to_string(id) + ", "s +
           "relevance = "s + std::to_string(relevance) + ", "s +
           "rating = "s + std::to_string(rating) +
           " }"s;
}

bool Document::CompareRelevance(const Document& lhs, const Document& rhs) {
    if (std::abs(lhs.relevance - rhs.relevance) < MAX_RELEVANCE_ACCURACY) {
        return lhs.rating > rhs.rating;
    } else {
        return lhs.relevance > rhs.relevance;
    }
}

std::ostream& operator<< (std::ostream& out, const Document& doc) {
    out << doc.Str();
    return out;
}
