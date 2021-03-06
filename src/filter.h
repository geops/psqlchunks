#ifndef __filter_h__
#define __filter_h__

#include <vector>
#include <string>

#include <regex.h>

#include "chunk.h"

namespace PsqlChunks
{

    class Filter
    {
        private:
            Filter(const Filter&);
            Filter& operator=(const Filter&);

        public:
            Filter() {};
            virtual ~Filter() {};

            /**
             * returns false if the parameter string is not accepted
             * in this case a error message will be written to the errmsg parameter
             */
            virtual bool setParams(const char * params, std::string &errmsg) = 0;
            virtual bool match(const Chunk& chunk) = 0;
            
    };


    /**
     * a chain/list of filters which will match all filters against the given chunk
     *
     * this class takes ownership of the added filters and will destroy them
     * on destruction
     */
    class FilterChain
    {
        private:
            FilterChain(const FilterChain&);
            FilterChain& operator=(const FilterChain&);

        protected:
            std::vector<Filter*> filters;

        public:
            FilterChain() : filters() {};
            ~FilterChain();

            void addFilter(Filter * filter);

            /** returns true when the chunk matches all filters */
            bool match(const Chunk& chunk);
    };


    /**
     * filters chunks which span the given linenumbers
     */
    class LineFilter : public Filter
    {
        protected:
            std::vector<linenumber_t> linenumbers;

        public:
            LineFilter() : Filter(), linenumbers() {};
            ~LineFilter() {};

            /**
             * param syntax:
             *  "1,6,88"
             */
            bool setParams(const char * params, std::string &errmsg);
            bool match(const Chunk& chunk);


    };

    
    /**
     * baseclass for all regex based filters
     */
    class RegexFilter : public Filter
    {
        private:
            regex_t * re;

            RegexFilter(const RegexFilter&);
            RegexFilter& operator=(const RegexFilter&);

        protected:
            bool matchString(std::string &str);

        public:

            RegexFilter() : Filter(), re(NULL) {}; 
            ~RegexFilter();

            bool setParams(const char * params, std::string &errmsg);
    };


    /** 
     * matches a regex against the start and end-comments of a chunk
     */
    class DescriptionRegexFilter : public RegexFilter
    {
        public:
            bool match(const Chunk& chunk);
    };


    /** 
     * matches a regex against the sql content of a chunk
     */
    class ContentRegexFilter : public RegexFilter
    {
        public:
            bool match(const Chunk& chunk);
    };


};

#endif /* __filter_h__ */
