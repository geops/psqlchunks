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

            std::istream & strm;
            Chunk chunkCache;

            linenumber_t line_number;

            enum Content {
                SEP,        // seperator
                FILE_MARKER,//
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
                IGNORE,
                COPY_CACHED
            };

            bool hasMarker(const std::string &, const std::string &, size_t , size_t &);
            Content classifyLine( std::string &, size_t &);

            // state machine 
            Content stm_last_cls;
            State stm_state;
            linenumber_t last_nonempty_line;


        public:
            ChunkScanner(std::istream &);
            ~ChunkScanner();
            //void scan( std::istream &);


            /** read next chunk
             *
             * returns false on failure
             */
            bool nextChunk( Chunk& );

            bool eof();

            //chunkvector_t chunks;
    };

};



#endif
