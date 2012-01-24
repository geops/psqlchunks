#include <sstream>
#include <algorithm>

#include <chunk.h>
#include <debug.h>

using namespace PsqlChunks;

static const char * s_chunk_sep = "-----------------------------------------------------------";
static const char * s_comment_start = "-- ";


// prototypes for local functions
void write_block(std::ostream &stream, const char * block_type, std::string & contents);


Line::Line()
    : number(0), contents("")
{
}


Line::Line(std::string _contents, linenumber_t _number)
    : number(_number), contents(_contents)
{
}


Chunk::Chunk()
    : sql_lines(), start_comment(""), end_comment(""), start_line(0), end_line(0),
      diagnostics(0)
{
}

void
Chunk::clear()
{
    start_comment.clear();
    end_comment.clear();
    start_line = 0;
    end_line = 0;

    delete diagnostics;
    diagnostics = NULL;

    // free the sql lines
    for (linevector_t::iterator lit = sql_lines.begin(); lit != sql_lines.end(); ++lit) {
        Line * line = *lit;
        delete line;
    }
    sql_lines.clear();
}


Chunk::~Chunk()
{
    clear();
}


void
Chunk::appendGeneric(std::string & target, std::string & fragment)
{
    if (!target.empty()) {
        target.append("\n");
    }
    target.append(fragment);
}


void
Chunk::appendSqlLine(std::string linetext, linenumber_t line_number ) {

    Line * p_line = new Line();
    p_line->number = line_number;
    p_line->contents = linetext;

    sql_lines.push_back(p_line);
    addLineNumber(p_line->number);
}


void
Chunk::appendStartComment( std::string  fragment ) {
    appendGeneric(start_comment, fragment);
}


void
Chunk::appendEndComment( std::string fragment ) {
    appendGeneric(end_comment, fragment);
}


void
Chunk::addLineNumber( linenumber_t lno )
{
    if ((start_line == 0) || (lno < start_line)) {
        start_line = lno;
    }

    if (end_line < lno) {
        end_line = lno;
    }
}


std::string
Chunk::getDescription()
{
    std::string desc = start_comment;
    replace(desc.begin(), desc.end(), '\n', ' ');
    return desc;
}


std::string
Chunk::getSql()
{
    std::stringstream sqlstream;

    for (linevector_t::iterator lit = sql_lines.begin(); lit != sql_lines.end(); ++lit) {
        sqlstream << (*lit)->contents << std::endl;
    }
    return sqlstream.str();
}


namespace PsqlChunks
{

    /**
     * write the contents of a chung to a stream
     */
    std::ostream &
    operator<<(std::ostream &stream, Chunk &chunk)
    {
        write_block(stream, "start", chunk.start_comment);

        for (linevector_t::iterator lit = chunk.sql_lines.begin(); lit != chunk.sql_lines.end(); ++lit) {
            stream << (*lit)->contents << std::endl;
        }

        if (chunk.end_comment.empty()) {
            write_block(stream, "end", chunk.start_comment);
        }
        else {
            write_block(stream, "end", chunk.end_comment);
        }

        return stream;
    }


    std::ostream &
    operator<<(std::ostream &stream, Line &line)
    {
        stream << line.contents << std::endl;
        return stream;
    }
};


/**
 * write a block of the following form the the given stream
 *
 * ---------------------------------------------------------
 * -- [block_type]: [contents]
 * -- [more contents]
 * ---------------------------------------------------------
 */
void
write_block(std::ostream &stream, const char * block_type, std::string & contents)
{

    stream << s_chunk_sep << std::endl;
    stream << s_comment_start << block_type << ": ";

    int i = 0;
    std::string line;
    std::istringstream iss(contents, std::istringstream::in);
    while(getline(iss, line)) {
        if (i != 0) {
            stream << s_comment_start;
        }
        stream << line << std::endl;
        i++;
    }
    if (i == 0) {
        stream << std::endl;
    }

    stream << s_chunk_sep << std::endl;
}


