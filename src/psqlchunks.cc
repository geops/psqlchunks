#include <iostream>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <cstring>
#include <termios.h>

#include "scanner.h"
#include "db.h"
#include "debug.h"

using namespace std;
using namespace PsqlChunks;

// http://en.wikipedia.org/wiki/ANSI_escape_code
#define ANSI_RED    "\e[31m"
#define ANSI_GREEN  "\e[32m"
#define ANSI_YELLOW "\e[33m"
#define ANSI_BLUE   "\e[34m"
#define ANSI_BOLD   "\e[1m"
#define ANSI_RESET  "\e[m"


// Return/exit codes
#define RC_OK           0
#define RC_E_USAGE      1
#define RC_E_SQL        2
#define RC_E_DB         3


// number of lines before and after the failing line
// to print when outputing sql after an error
#define ERROR_CONTEXT 2

static const char * s_fail_sep = "-------------------------------------------------------";


/* prototypes */
void quit(const char * message);
void print_help();


enum Command {
    CONCAT,
    LIST,
    RUN
};

struct Settings {
    const char * db_port;
    const char * db_user;
    const char * db_name;
    const char * db_host;
    bool ask_pass;
    bool commit_sql;
    bool abort_after_failed;
    Command command;
    bool colored;
};
static Settings settings = {
    NULL,
    NULL,
    NULL,
    NULL,
    false,
    false,
    false,
    LIST,
    false
};


void inline
set_ansi(const char * color)
{
    if (settings.colored) {
        printf("%s", color);
    }
}


void
quit(const char * message)
{
    printf("%s\nCall with \"help\" for help.\n", message);
    exit(RC_E_USAGE);
}


void
print_help()
{
    printf(
        "Usage :  \n"
        "psqlchunks command [options] files\n"
        "use - as filename to read from stdin.\n"
        "\n"
        "Commands:\n"
        "  concat   concat all sql files and write the output to stdout.\n"
        "  help\n"
        "  list     list chunks\n"
        "  run      run/test chunks in the database\n"
        "\n"
        "SQL Handling:\n"
        "  -c    commit SQL to the database. Default is performing a rollback\n"
        "        after the SQL has been executed. A commit will only be executed\n"
        "        if no errors occured.\n"
        "  -a    abort execution after first failed chunk.\n"
        "\n"
        "Connection parameters:\n"
        "  -d [database name]\n"
        "  -U [user]\n"
        "  -W       ask for password\n"
        "  -h [host/socket name]\n"
        "\n"
        "Return codes:\n"
        "  0     no errors\n"
    );
}


std::string
read_password()
{
    // hide input
    termios oldt;
    tcgetattr(STDIN_FILENO, &oldt);
    termios newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    std::string s;
    std::getline(std::cin, s);
    std::cout << std::endl;
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    return s;
}


int
run_sql(chunkvector_t & chunks)
{
    const char * password = NULL;
    int rc = RC_OK;
    std::string prompt_passwd;

    if (settings.ask_pass) {
        printf("Password: ");
        prompt_passwd = read_password();
        password = prompt_passwd.c_str();
    }

    log_debug("running chunks");

    try {
        Db database(settings.db_host, settings.db_name, settings.db_port, settings.db_user, password);
        if (!database.isConnected()) {
            std::cerr << database.getErrorMessage() << std::endl;
            return RC_E_USAGE;
        }

        database.setCommit(settings.commit_sql);

        // run all chunks
        for (chunkvector_t::iterator chit = chunks.begin(); chit != chunks.end(); ++chit) {
            Chunk * chunk = *chit;

            bool run_ok = database.runChunk(chunk);
            if (run_ok) {
                set_ansi(ANSI_GREEN);
                printf("OK  ");
                set_ansi(ANSI_RESET);
            }
            else {
                set_ansi(ANSI_RED);
                printf("FAIL");
                set_ansi(ANSI_RESET);
            }

            printf("  [%d-%d] %s\n", chunk->start_line, chunk->end_line, chunk->getDescription().c_str());

            if (!run_ok && chunk->hasDiagnostics()) {
                printf( "%s\n", s_fail_sep);
                set_ansi(ANSI_BOLD);
                printf( "> description : %s\n"
                        "> line        : %d\n"
                        "> sql state   : %s\n",
                        chunk->diagnostics->msg_primary.c_str(), chunk->diagnostics->error_line,
                        chunk->diagnostics->sqlstate.c_str()
                );


                if (!chunk->diagnostics->msg_detail.empty()) {
                    printf("> details     : %s\n", chunk->diagnostics->msg_detail.c_str());
                }
                if (!chunk->diagnostics->msg_hint.empty()) {
                    printf("> hint        : %s\n", chunk->diagnostics->msg_hint.c_str());
                }

                printf("> SQL         :\n\n");
                set_ansi(ANSI_RESET);

                // sql
                size_t out_start = chunk->diagnostics->error_line - ERROR_CONTEXT;
                size_t out_end = chunk->diagnostics->error_line + ERROR_CONTEXT;
                linevector_t sql_lines = chunk->getSqlLines();
                for (linevector_t::iterator lit = sql_lines.begin(); lit != sql_lines.end(); ++lit) {
                    if (((*lit)->number >= out_start) &&
                        ((*lit)->number <= out_end)) {

                        if ((*lit)->number == chunk->diagnostics->error_line) {
                            set_ansi(ANSI_RED);
                        }
                        printf("%s\n", (*lit)->contents.c_str());
                        if ((*lit)->number == chunk->diagnostics->error_line) {
                            set_ansi(ANSI_RESET);
                        }
                    }

                    if ((*lit)->number >= out_end) {
                        break;
                    }
                }
                printf("\n%s\n", s_fail_sep);
            }

            if (!run_ok && settings.abort_after_failed) {
                printf("Chunk failed. Aborting.\n");
                break;
            }
        }

        if (database.getFailedCount() == 0) {
            printf("\nAll chunks passed.\n");
            if (settings.commit_sql) {
                set_ansi(ANSI_YELLOW);
                printf("Commit.");
                set_ansi(ANSI_RESET);
                printf("\n");
            }
        }
        else {
            printf("\n%d chunks failed.\n", database.getFailedCount());
            rc = RC_E_SQL;
            if (settings.commit_sql) {
                set_ansi(ANSI_YELLOW);
                printf("Rollback.");
                set_ansi(ANSI_RESET);
                printf("\n");
            }
        }
    }
    catch (DbException &e) {
        printf("Fatal error: %s\n", e.what());
        rc = RC_E_DB;
    }

    return rc;
}


int
handle_files(char * files[], int nufiles)
{
    ChunkScanner chunkscanner;
    for( int i = 0; i < nufiles; i++ ) {
        if (strcmp(files[i], "-") == 0) {
            // read from stdin
            chunkscanner.scan(std::cin);
        }
        else {
            // open the file
            std::ifstream is;
            is.open(files[i]);

            if (is.fail()) {
                fprintf(stderr, "Could not open file \"%s\".\n", files[i]);
                return RC_E_USAGE;
            }
            chunkscanner.scan(is);
        }
    }

    int rc = RC_OK;
    switch (settings.command) {
        case CONCAT:
            for (chunkvector_t::iterator chit = chunkscanner.chunks.begin(); chit != chunkscanner.chunks.end(); ++chit) {
                Chunk * chunk = *chit;
                std::cout << *chunk << std::endl;
            }
            break;

        case LIST:
            set_ansi(ANSI_BOLD);
            printf(" start  |  end   | contents\n");
            set_ansi(ANSI_RESET);
            for (chunkvector_t::iterator chit = chunkscanner.chunks.begin(); chit != chunkscanner.chunks.end(); ++chit) {
                Chunk * chunk = *chit;
                printf("%8d-%8d: %s\n", chunk->start_line, chunk->end_line, chunk->getDescription().c_str());
            }
            break;

        case RUN:
            rc = run_sql(chunkscanner.chunks);
            break;

    }


    return rc;
}


int
main(int argc, char * argv[] )
{
    char opt;

    // use colored output if run in a shell
    if (isatty(fileno(stdout)) == 1) {
        settings.colored = true;
    };

    // read options
    while ( (opt = getopt(argc, argv, "p:U:d:h:Wca")) != -1) {
        switch (opt) {
            case 'p': /* port */
                settings.db_port = optarg;
                break;
            case 'U': /* username */
                settings.db_user = optarg;
                break;
            case 'd':
                settings.db_name = optarg;
                break;
            case 'h':
                settings.db_host = optarg;
                break;
            case 'W':
                settings.ask_pass = true;
                break;
            case 'c':
                settings.commit_sql = true;
                break;
            case 'a':
                settings.abort_after_failed = true;
                break;
            default:
                quit("Unknown option.");
        }
    }


    // command
    if (optind >= argc) {
        quit("No command specified.");
    }
    if (strcmp(*(argv+optind), "concat") == 0) {
        settings.command = CONCAT;
    }
    else if (strcmp(*(argv+optind), "list") == 0) {
        settings.command = LIST;
    }
    else if (strcmp(*(argv+optind), "run") == 0) {
        settings.command = RUN;
    }
    else if (strcmp(*(argv+optind), "help") == 0) {
        print_help();
        return RC_OK;
    }
    else {
        quit("Unknown command");
    }


    // check for input files
    int fileind = optind+1;
    if (fileind >= argc) {
        quit("No input file(s) given.");
    }

    return handle_files(argv+fileind, argc-fileind);
}
