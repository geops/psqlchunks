#ifndef __chunk_h__
#define __chunk_h__

#include <string>
#include <vector>
#include <iostream>
#include <stdint.h>

#include <sys/time.h>

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

            /** runtime of the query */
            struct timeval runtime;

            linenumber_t error_line;
            CommandStatus status;
            std::string sqlstate;
            std::string msg_primary;
            std::string msg_detail;
            std::string msg_hint;

            Diagnostics() : error_line(1), status(Ok), sqlstate(""),
                    msg_primary(""), msg_detail(""), msg_hint("")
            {
                runtime.tv_sec = 0;
                runtime.tv_usec = 0;
            };
    };


    class Chunk
    {

        private:
            Chunk(const Chunk&);

        protected:
            linevector_t sql_lines;
            std::string start_comment;
            std::string end_comment;

            void addLineNumber(linenumber_t);

        public:
            /** the line number the contents of the chunk started */
            linenumber_t start_line;

            /** the line number the contents of the chunk ended */
            linenumber_t end_line;

            Diagnostics diagnostics;

            Chunk();
            ~Chunk();

            Chunk& operator=(const Chunk&);

            void appendSqlLine(std::string , linenumber_t);
            void appendStartComment(std::string );
            void appendEndComment(std::string );
            const std::string getSql();

            bool hasSql()
            {
                return !sql_lines.empty();
            }

            const linevector_t & getSqlLines()
            {
                return sql_lines;
            }

            bool failed()
            {
                return diagnostics.status != Diagnostics::Ok;
            }

            /** get a description for the chunk. single line */
            const std::string getDescription();

            void clear();

            friend std::ostream &operator<<(std::ostream &, const PsqlChunks::Chunk&);
    };

    typedef std::vector<Chunk*> chunkvector_t;

};

#endif
