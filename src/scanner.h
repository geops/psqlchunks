#ifndef __scanner_h__
#define __scanner_h__

#include <string>
#include <iostream>
#include <fstream>

#include "chunk.h"

namespace PsqlChunks
{
    class ChunkScanner
    {
        protected:

            linenumber_t line_number;

            enum Content {
                SEP,        // seperator
                COMMENT,    // sql single line comment
                COMMENT_END,
                COMMENT_START,
                EMPTY,      // empty or only whitespace
                OTHER       // anything, propably sql
            };

            enum State {
                CAPTURE_SQL,
                CAPTURE_START_COMMENT,
                CAPTURE_END_COMMENT,
                NEW_CHUNK,
                IGNORE
            };

            bool hasMarker(const std::string &, const std::string &, size_t , size_t &);
            Content classifyLine( std::string &, size_t &);

        public:
            ChunkScanner();
            ~ChunkScanner();
            void scan( std::istream &);

            /** remove all scanned chunks from memory */
            void clear();

            chunkvector_t chunks;
    };

};



#endif
