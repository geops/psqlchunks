#include <string>
#include <cctype>

#include "scanner.h"
#include "debug.h"


using namespace PsqlChunks;

inline bool
is_inline_whitespace( const char ch) {
    // does not include linebreaks
    return (ch == '\t' || ch == ' ');
}


/**
 * check if a stricg starts with another string
 * not unicode compatible
 */
bool
starts_with(const std::string &haystack, const std::string& needle, size_t start_pos, bool ignore_case)
{
    if ((haystack.size()-start_pos) < needle.size()) {
        return false;
    }

    for( size_t c=0;c<needle.size(); c++) {
        if (ignore_case) {
            if ((haystack[start_pos+c] != tolower(needle[c])) &&
                    (haystack[start_pos+c] != toupper(needle[c])) ) {
                return false;
            }
        }
        else {
            if (haystack[start_pos+c] != needle[c]) {
                return false;
            }
        }
    }
    return true;
}


bool
ChunkScanner::has_marker(const std::string &haystack, const std::string& marker,
        size_t start_pos, size_t &end_pos)
{
    if (starts_with(haystack, marker, start_pos, true)) {
        size_t c = marker.size();
        while (((start_pos+c) < haystack.size()) && is_inline_whitespace(haystack[start_pos+c])) {
            c++;
        }
        if (haystack.size() > (start_pos+c)) {
            if (haystack[start_pos+c] == ':') {
                end_pos = start_pos+c;
                while (((end_pos) < haystack.size()) &&
                        ( is_inline_whitespace(haystack[end_pos]) || haystack[end_pos] == ':')) {
                    end_pos++;
                }
                return true;
            }
        }
    }

    return false;
}


ChunkScanner::ChunkScanner()
    :   line_number(1), chunks()
{
}


ChunkScanner::~ChunkScanner()
{
    clear();
}


void
ChunkScanner::clear()
{
    // free the chunks
    for (chunkvector_t::iterator chit = chunks.begin(); chit != chunks.end(); ++chit) {
        Chunk * chunk = *chit;
        delete chunk;
    }
    chunks.clear();
}



ChunkScanner::Content
ChunkScanner::classifyLine( std::string & line, size_t & content_pos)
{
    Content classification = EMPTY;
    int dash_counter = 0;
    content_pos=0;

    for (size_t c=0; c<line.size(); c++) {

        if (line[c] == '-') {
            dash_counter++;
        }
        else {
            if (dash_counter == 2) {
                classification = COMMENT;
            }
            else if (!is_inline_whitespace(line[c])) {
                classification = OTHER;
                content_pos = c;
                break;
            }

            if (classification != COMMENT) {
                dash_counter=0;
            }
            else {
                if (!is_inline_whitespace(line[c])) {
                    content_pos = c;
                    if (has_marker(line, "start", c, content_pos)) {
                        classification = COMMENT_START;
                    }
                    if (has_marker(line, "end", c, content_pos)) {
                        classification = COMMENT_END;
                    }
                    break;
                }
            }
        }

        if (dash_counter == 3) {
            classification = SEP;
            break;
        }
    }
    return classification;
}


void
ChunkScanner::scan( std::istream &is )
{
    Content last_class = EMPTY;
    State state = CAPTURE_SQL;
    Chunk * chunk_ptr = new Chunk(); // TODO: handle bad_alloc

    while (is.good()) {
        std::string line;
        getline(is, line);

        size_t content_pos;
        Content classification = classifyLine(line, content_pos);
        switch (classification) {
            case OTHER:
                state = CAPTURE_SQL;
                break;
            case SEP:
                state = IGNORE;
                break;
            case COMMENT:
                if ((state != CAPTURE_END_COMMENT) && (state != CAPTURE_START_COMMENT) && (state != NEW_CHUNK)) {
                    state = CAPTURE_SQL;
                }
                else if (state == NEW_CHUNK) {
                    state = CAPTURE_START_COMMENT;
                }
                break;
            case COMMENT_START:
                if (last_class == SEP) {
                    state = NEW_CHUNK;
                }
                else if (last_class == COMMENT_START) {
                    state = CAPTURE_START_COMMENT;
                }
                else {
                    state = CAPTURE_SQL;
                }
                break;
            case COMMENT_END:
                if (last_class == SEP) {
                    state = CAPTURE_END_COMMENT;
                }
                else {
                    state = CAPTURE_SQL;
                }
                break;
            case EMPTY:
                break;
        }


        switch (state) {
            case CAPTURE_SQL:
                // append the sql and set the min max line numbers
                chunk_ptr->appendSqlLine(line, line_number);
                break;
            case NEW_CHUNK:
                if (chunk_ptr->hasSql()) {
                    chunks.push_back(chunk_ptr);
                    chunk_ptr = new Chunk(); // TODO: handle bad_alloc
                }
                else {
                    chunk_ptr->clear();
                }
            case CAPTURE_START_COMMENT:
                chunk_ptr->appendStartComment( line.substr(content_pos, std::string::npos));
                break;
            case CAPTURE_END_COMMENT:
                chunk_ptr->appendEndComment( line.substr(content_pos, std::string::npos));
                break;
            case IGNORE:
                break;
        }

        line_number++;
        last_class = classification;
    }

    if (chunk_ptr->hasSql()) {
        chunks.push_back(chunk_ptr);
    }
    else {
        delete chunk_ptr;
    }
}

