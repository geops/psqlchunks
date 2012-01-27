#ifndef __chunk_h__
#define __chunk_h__

#include <string>
#include <vector>
#include <iostream>
#include <stdint.h>

#define LINE_NUMBER_NOT_AVAILABLE   0

namespace PsqlChunks
{

    typedef uint32_t linenumber_t;

    class Line
    {
        public:
            linenumber_t number;
            std::string contents;

            Line();
            Line(std::string _contents, linenumber_t _number);

            friend std::ostream &operator<<(std::ostream &, PsqlChunks::Line&);
    };

    typedef std::vector<Line*> linevector_t;

    class Diagnostics {

        public:

            enum CommandStatus {Ok, Fail};

            linenumber_t error_line;
            CommandStatus status;
            std::string sqlstate;
            std::string msg_primary;
            std::string msg_detail;
            std::string msg_hint;

            Diagnostics() : error_line(0), status(Ok), sqlstate(""), msg_primary(""),
                    msg_detail(""), msg_hint("") {};

    };


    class Chunk
    {

        private:
            Chunk(const Chunk&);
            Chunk& operator=(const Chunk&);

        protected:
            linevector_t sql_lines;
            std::string start_comment;
            std::string end_comment;

            void appendGeneric(std::string &, std::string &);
            void addLineNumber(linenumber_t);


        public:
            /** the line number the contents of the chunk started */
            linenumber_t start_line;

            /** the line number the contents of the chunk ended */
            linenumber_t end_line;

            Diagnostics * diagnostics;

            Chunk();
            ~Chunk();

            void appendSqlLine(std::string , linenumber_t);
            void appendStartComment(std::string );
            void appendEndComment(std::string );
            std::string getSql();

            bool hasSql()
            {
                return !sql_lines.empty();
            }

            linevector_t & getSqlLines()
            {
                return sql_lines;
            }

            bool hasDiagnostics()
            {
                return diagnostics != NULL;
            }

            /** get a description for the chunk. single line */
            std::string getDescription();

            void clear();

                // http://stackoverflow.com/questions/7850125/convert-this-pointer-to-string
                // http://stackoverflow.com/questions/1255366/how-can-i-append-data-to-a-stdstring-in-hex-format

            friend std::ostream &operator<<(std::ostream &, PsqlChunks::Chunk&);
    };

    typedef std::vector<Chunk*> chunkvector_t;

};

#endif
