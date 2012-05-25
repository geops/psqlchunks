#include <sstream>
#include <cstdlib>

#include "filter.h"
#include "debug.h"


using namespace PsqlChunks;

void
FilterChain::addFilter(Filter * filter)
{
    filters.push_back(filter);
}


bool
FilterChain::match(const Chunk &chunk)
{
    for (std::vector<Filter*>::const_iterator filterit = filters.begin(); filterit != filters.end(); ++filterit) {
        if (!(*filterit)->match(chunk)) {
            return false;
        }
    }
    return true;
}


FilterChain::~FilterChain()
{
    for (std::vector<Filter*>::iterator filterit = filters.begin(); filterit != filters.end(); ++filterit) {
        delete (*filterit);
    }
}


bool 
LineFilter::setParams(const char * params, std::string &errmsg)
{
    std::stringstream pstream(params);
    std::string numberstring;

    // split the params at the commas
    while(std::getline(pstream, numberstring, ',')) {
        std::stringstream numberstream(numberstring);
        linenumber_t number;

        numberstream >> number;
        if (numberstream.fail()) {
            // illegal value
            linenumbers.clear();

            std::stringstream errstream;
            errstream << "Not a number: " << numberstring;
            errmsg = errstream.str();

            return false;
        }
        linenumbers.push_back(number);

        log_debug("LineFilter: number = %d", number);
    }

    if (linenumbers.empty()) {
        errmsg = "No linenumbers given.";
    }

    return !linenumbers.empty();
}


bool
LineFilter::match(const Chunk& chunk)
{
    for(std::vector<linenumber_t>::const_iterator number = linenumbers.begin(); number != linenumbers.end(); ++number) {
        if ((chunk.start_line <= *number) && (chunk.end_line >= *number)) {
            return true;
        }
    }
    return false;
}


bool
RegexFilter::matchString(std::string &str) 
{
    if (re) {
        return (regexec(re, str.c_str(), 0, NULL, 0) == 0);
    }
    
    return false;
}

bool 
RegexFilter::setParams(const char * params, std::string &errmsg)
{
    if (!re) {
        re = new regex_t;
    }
    else {
        regfree(re);
    }
    int errcode = regcomp(re, params, REG_EXTENDED | REG_ICASE);
    if (errcode != 0) {
        size_t len = regerror(errcode, re, NULL, 0);
        char * buff = new char[len];
        if(buff) {
            regerror(errcode, re, buff, len);
            errmsg = buff;
            delete[] buff;
        }
        else {
            errmsg = "Invalid regex";
        }
        regfree(re);
        delete re;
        re = NULL;

        return false;
    }
    return true;
}

RegexFilter::~RegexFilter()
{
    if (re) {
        regfree(re);
        delete re;
    }
}

bool
DescriptionRegexFilter::match(const Chunk& chunk)
{
    std::string desc = chunk.getDescription();
    return matchString(desc);
}


bool
ContentRegexFilter::match(const Chunk& chunk)
{
    std::string sql = chunk.getSql();
    return matchString(sql);
}
