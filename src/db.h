#ifndef __db_h__
#define __db_h__

#include <string>
#include <stdexcept>
#include <libpq-fe.h>

#include "chunk.h"

namespace PsqlChunks
{

    class DbException : public std::runtime_error
    {
        public:
            DbException(const char * msg)
                :  std::runtime_error(msg)
            {
            }

            DbException(std::string msg)
                :  std::runtime_error(msg)
            {
            }

            virtual ~DbException() throw() {};
    };

    class Db
    {
        protected:
            PGconn * conn;
            bool do_commit;
            unsigned int failed_count;
            bool in_transaction;

            void commit();
            void rollback();
            void begin();

            /**
             * silent: do not log on error
             */
            void executeSql(const char *, bool silent = false);



        private:
            Db(const Db&);
            const Db& operator=(const Db&);

        public:
            Db();
            ~Db();

            bool connect( const char * host, const char * db_name, const char * port, const char * user, const char * passwd);
            void disconnect();

            std::string getErrorMessage();
            bool isConnected();

            void inline setCommit(bool commit)
            {
                do_commit = commit;
            }

            unsigned int inline getFailedCount()
            {
                return failed_count;
            }

            bool setEncoding(const char * enc_name);

            bool runChunk(Chunk & chunk);
            void finish();
            bool cancel(std::string &);
    };
};

#endif
