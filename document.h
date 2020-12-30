#pragma once

#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

enum class DocumentStatus
{
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

struct DocumentData
{
    int rating;
    DocumentStatus status;
    std::map<std::string_view, double> word_frequency;
};

struct Document {
    int id = 0;
    double relevance = 0.0;
    int rating = 0;

    Document() = default;

    Document(int input_id, double input_relevance, int input_rating);

    std::string Str() const;

    static bool CompareRelevance(const Document& lhs, const Document& rhs);
};

inline bool operator== (const Document& lhv, const Document& rhv) {
    return (lhv.id == rhv.id && lhv.relevance == rhv.relevance && lhv.rating == rhv.rating);
}

void PrintDocument(const Document& document);

void PrintMatchDocumentResult(int document_id, const std::vector<std::string_view>& words, DocumentStatus status);

std::ostream& operator<< (std::ostream& out, const Document& doc);
