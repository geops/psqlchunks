#include <string>
#include <cctype>

#include "scanner.h"
#include "debug.h"


using namespace PsqlChunks;

/**
 * does not include linebreaks
 */
bool inline static
is_inline_whitespace( const char ch) {
    return (ch == '\t' || ch == ' ');
}


/**
 * check if a string starts with another string
 * not unicode compatible
 */
bool static
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
ChunkScanner::hasMarker(const std::string &haystack, const std::string& marker,
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


ChunkScanner::ChunkScanner( std::istream & _strm )
    :   strm(_strm),
        chunkCache(),
        line_number(1),
        stm_last_cls(EMPTY),
        stm_state(CAPTURE_SQL),
        last_nonempty_line(1)
{
}


ChunkScanner::~ChunkScanner()
{
}

bool
ChunkScanner::eof()
{
    return !strm.good();
}



ChunkScanner::Content
ChunkScanner::classifyLine( std::string & line, size_t & content_pos)
{
    Content cls = EMPTY;
    int dash_counter = 0;
    content_pos=0;

    for (size_t c=0; c<line.size(); c++) {

        if (line[c] == '-') {
            dash_counter++;
        }
        else {
            if (dash_counter == 2) {
                cls = COMMENT;
            }
            else if ((dash_counter >= 4) and (line[c] == '[')) {
                // File marker from previous concat
                cls = FILE_MARKER;
                content_pos = c;
                break;
            }
            else if (!is_inline_whitespace(line[c])) {
                cls = OTHER;
                content_pos = c;
                break;
            }

            if (cls != COMMENT) {
                dash_counter=0;
            }
            else {
                if (!is_inline_whitespace(line[c])) {
                    content_pos = c;
                    if (hasMarker(line, "start", c, content_pos)) {
                        cls = COMMENT_START;
                    }
                    if (hasMarker(line, "end", c, content_pos)) {
                        cls = COMMENT_END;
                    }
                    break;
                }
            }
        }

        if (dash_counter >= 3) {
            cls = SEP;
        }
    }
    return cls;
}


bool
ChunkScanner::nextChunk( Chunk &chunk )
{
    chunk.clear();

    if (stm_state == COPY_CACHED) {
        chunk = chunkCache;
        chunkCache.clear();
        stm_state = NEW_CHUNK;
    }

    std::string line;

    while (strm.good()) {

        line.clear();
        getline(strm, line);

        size_t content_pos;
        Content cls = classifyLine(line, content_pos);
        switch (cls) {
            case OTHER:
                if (stm_state == CAPTURE_END_COMMENT) {
                    stm_state = END_CHUNK;
                }
                else {
                    stm_state = CAPTURE_SQL;
                }
                break;
            case FILE_MARKER:
            case EMPTY: // empty lines are always ignored. may be re-added later
            case SEP:
                if (stm_state == CAPTURE_END_COMMENT) {
                    stm_state = END_CHUNK;
                }
                else {
                    stm_state = IGNORE;
                }
                break;
            case COMMENT:
                if ( (stm_state != CAPTURE_END_COMMENT)
                        && (stm_state != CAPTURE_START_COMMENT)
                        && (stm_state != NEW_CHUNK)) {
                    stm_state = CAPTURE_SQL;
                }
                else if (stm_state == NEW_CHUNK) {
                    stm_state = CAPTURE_START_COMMENT;
                }
                break;
            case COMMENT_START:
                if (stm_last_cls == SEP) {
                    stm_state = NEW_CHUNK;
                }
                else if (stm_last_cls == COMMENT_START) {
                    stm_state = CAPTURE_START_COMMENT;
                }
                else {
                    stm_state = CAPTURE_SQL;
                }
                break;
            case COMMENT_END:
                if (stm_last_cls == SEP) {
                    stm_state = CAPTURE_END_COMMENT;
                }
                else {
                    stm_state = CAPTURE_SQL;
                }
                break;
        }

        switch (stm_state) {
            case CAPTURE_SQL:
                // re-add empty lines in case we skipped some inbetween the
                // sql lines
                if (chunk.hasSql()) {
                    for (unsigned int i = 0; i < (line_number - 1 - last_nonempty_line); i++) {
                        chunk.appendSqlLine("", i+last_nonempty_line);
                    }
                }
                // append the sql and set the min max line numbers
                chunk.appendSqlLine(line, line_number);
                break;
            case END_CHUNK:
                if (chunk.hasSql()) {
                    stm_state = COPY_CACHED;
                    chunkCache.clear();
                    if (cls == OTHER) {
                        chunkCache.appendSqlLine(line, line_number);
                    }
                    line_number++;
                    return true;
                }
                break;
            case NEW_CHUNK:
                if (chunk.hasSql()) {
                    stm_state = COPY_CACHED;
                    chunkCache.clear();
                    chunkCache.appendStartComment( line.substr(content_pos, std::string::npos ) );
                    line_number++;
                    return true;
                }
                else {
                    // purge all info from incomplete chunks
                    chunk.clear();
                }
            case CAPTURE_START_COMMENT:
                chunk.appendStartComment( line.substr(content_pos, std::string::npos));
                break;
            case CAPTURE_END_COMMENT:
                chunk.appendEndComment( line.substr(content_pos, std::string::npos));
                break;
            case IGNORE:
                break;
            case COPY_CACHED:
                log_error("statemachine reached COPY_CACHED. this should never happen");
                return false;
        }

        if (stm_state != IGNORE) {
            last_nonempty_line = line_number;
        }

        stm_last_cls = cls;
        line_number++;
    }

    // remove chunk contents if they are incomplete
    if (!chunk.hasSql()) {
        chunk.clear();
        return false;
    }
    return true;
}

