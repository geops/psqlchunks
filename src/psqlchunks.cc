#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <cstring>
#include <termios.h>
#include <signal.h>

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
#define ANSI_NONE   ""


// Return/exit codes
#define RC_OK           0
#define RC_E_USAGE      1
#define RC_E_SQL        2
#define RC_E_DB         3
#define RC_E_OTHER      4

// number of lines before and after the failing line
// to print when outputing sql after an error
#define DEFAULT_CONTEXT_LINES 2

// these two macros convert macro values to strings
#define STRINGIFY2(x)   #x
#define STRINGIFY(x)    STRINGIFY2(x)

// version number
#define VERSION_MAJOR 0
#define VERSION_MINOR 5
#define VERSION_PATCH 2

static const char * s_fail_sep = "-------------------------------------------------------";

// allow signal handler to access db
static Db * db_ptr = NULL;

enum Command {
    CONCAT,
    LIST,
    RUN
};

enum CommandRc {
    OK,
    BREAK
};


/* prototypes */
void quit(const char * message);
void print_help();
const char * ansi_code(const char * color);
std::string read_password();
int handle_files(char * files[], int nufiles);
void cmd_list_printheader();
CommandRc cmd_list(Chunk & chunk);
CommandRc cmd_concat(const Chunk & chunk);
void cmd_run_print_diagnostics(Chunk & chunk);
CommandRc cmd_run(Chunk & chunk, Db & db);
CommandRc scan(ChunkScanner & scanner, Db & db);
extern void handle_sigint(int sig);


struct Settings {
    const char * db_port;
    const char * db_user;
    const char * db_name;
    const char * db_host;
    bool ask_pass;
    bool commit_sql;
    bool abort_after_failed;
    Command command;
    bool is_terminal;
    unsigned int context_lines;
    bool print_filenames;
    const char * client_encoding;
};
static Settings settings = {
    NULL,       /* db_port */
    NULL,       /* db_user */
    NULL,       /* db_name */
    NULL,       /* db_host */
    false,      /* ask_pass */
    false,      /* commit_sql */
    false,      /* abort_after_failed */
    LIST,       /* command */
    false,      /* colored */
    DEFAULT_CONTEXT_LINES,   /* context_lines */
    true,       /* print_filenames */
    NULL        /* client_encoding */
};


const char *
ansi_code(const char * color)
{
    if (settings.is_terminal) {
        return color;
    }
    return ANSI_NONE;
}


void
quit(const char * message)
{
    printf("%s\nCall with \"help\" for help.\n", message);
    exit(RC_E_USAGE);
}

extern void
handle_sigint(int sig) {
    log_debug("Caught signal %d", sig);
    if (sig == SIGINT) {
        int rc = RC_OK;
        printf("\nReceived SIGINT\n");

        if (db_ptr) {
            std::string errmsg;

            printf("%sCanceling running queries%s\n", ansi_code(ANSI_YELLOW),
                    ansi_code(ANSI_RESET));
            if (!db_ptr->cancel(errmsg)) {
                printf("Canceling failed: %s\n", errmsg.c_str());
                rc = RC_E_DB;
            }
            db_ptr->setCommit(false);
        }
        exit(rc);
    }
}

void
print_help()
{
    printf(
        "Usage :  \n"
        "psqlchunks command [options] files\n"
        "version: " STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "."  STRINGIFY(VERSION_PATCH) "\n"
        "\n"
        "use - as filename to read from stdin.\n"
        "Definition of a chunk of SQL:\n"
        "  A chunk of SQL is block of SQL statements to be executed together,\n"
        "  and is delimited by the following markers:\n"
        "\n"
        "  -------------------------------------------------------------\n"
        "  -- start: creating my table\n"
        "  -------------------------------------------------------------\n"
        "  create table mytable (myint integer, mytext text);\n"
        "  -------------------------------------------------------------\n"
        "  -- end: creating my table\n"
        "  -------------------------------------------------------------\n"
        "\n"
        "  The end marker is optional and may be ommited.\n"
        "  The shortest marker syntac understood by this tool is:\n"
        "\n"
        "  ----\n"
        "  -- start: creating my table\n"
        "  create table mytable (myint integer, mytext text);\n"
        "\n"
        "\n"
        "Commands:\n"
        "  concat       concat all SQL files and write the formatted output to stdout\n"
        "  help         display this help text\n"
        "  list         list chunks\n"
        "  run          run SQL chunks in the database\n"
        "               This will not commit the SQL. But be aware that this tool\n"
        "               does not parse the SQL statments and will not filter out\n"
        "               COMMIT statements from the SQL files. Should there be any\n"
        "               in the files, the SQL WILL BE COMMITED and this tool will\n"
        "               terminate.\n"
        "\n"
        "General:\n"
        "  -F           hide filenames from output\n"
        "\n"
        "SQL Handling:\n"
        "  -C           commit SQL to the database. Default is performing a rollback\n"
        "               after the SQL has been executed. A commit will only be\n"
        "               executed if no errors occured. (default: rollback)\n"
        "  -a           abort execution after first failed chunk. (default: continue)\n"
        "  -l           number of lines to output before and after failing lines\n"
        "               of SQL. (default: " STRINGIFY(DEFAULT_CONTEXT_LINES) ")\n"
        "  -E           set the client_encoding of the database connection. This\n"
        "               setting is useful when the encoding of sql file differs\n"
        "               from the default client_encoding of the database server.\n"
        "\n"
        "Connection parameters:\n"
        "  -d [database name]\n"
        "  -U [user]\n"
        "  -W           ask for password (default: don't ask)\n"
        "  -h [host/socket name]\n"
        "\n"
        "Return codes:\n"
        "  " STRINGIFY(RC_OK)       "            no errors\n"
        "  " STRINGIFY(RC_E_USAGE)  "            invalid usage of this program\n"
        "  " STRINGIFY(RC_E_SQL)    "            the SQL contains errors\n"
        "  " STRINGIFY(RC_E_DB)     "            (internal) database error\n"
        "\n"
    );
}


std::string
read_password()
{
    // get current terminal settings
    termios oldt;
    tcgetattr(STDIN_FILENO, &oldt);

    // hide input
    termios newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    std::string s;
    std::getline(std::cin, s);
    std::cout << std::endl;

    // restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    return s;
}


/****** COMMAND Functions *****/

inline void
cmd_list_printheader()
{
    printf("%s start  |  end   | contents%s\n",
            ansi_code(ANSI_BOLD),
            ansi_code(ANSI_RESET));

}

inline CommandRc
cmd_list(Chunk & chunk)
{
    printf("%8d-%8d: %s\n", chunk.start_line, chunk.end_line, chunk.getDescription().c_str());
    return OK;
}


inline CommandRc
cmd_concat(const Chunk & chunk)
{
    std::cout << chunk << std::endl;
    return OK;
}


inline void
cmd_run_print_diagnostics(Chunk & chunk) {
    if (chunk.failed()) {
        printf( "%s\n"
                "%s> description : %s\n"
                "> sql state   : %s\n",
                s_fail_sep,
                ansi_code(ANSI_BOLD),
                chunk.diagnostics.msg_primary.c_str(),
                chunk.diagnostics.sqlstate.c_str()
        );
        if (chunk.diagnostics.error_line != LINE_NUMBER_NOT_AVAILABLE) {
            printf("> line        : %d\n", chunk.diagnostics.error_line);
        }
        else {
            printf("> line        : not available [chunk %d-%d]\n",
                        chunk.start_line, chunk.end_line);
        }


        if (!chunk.diagnostics.msg_detail.empty()) {
            printf("> details     : %s\n", chunk.diagnostics.msg_detail.c_str());
        }
        if (!chunk.diagnostics.msg_hint.empty()) {
            printf("> hint        : %s\n", chunk.diagnostics.msg_hint.c_str());
        }

        // print sql fragment
        if (chunk.diagnostics.error_line != LINE_NUMBER_NOT_AVAILABLE) {
            printf("> SQL         :%s\n\n", ansi_code(ANSI_RESET));

            // calculate the size of the fragment
            size_t out_start = chunk.start_line;
            size_t out_end = chunk.end_line;
            if (settings.context_lines < (chunk.diagnostics.error_line - chunk.start_line)) {
                out_start = chunk.diagnostics.error_line - settings.context_lines;
            }
            if (settings.context_lines < (chunk.end_line - chunk.diagnostics.error_line)) {
                out_end = chunk.diagnostics.error_line + settings.context_lines;
            }
            log_debug("out_start: %u, out_end: %u", out_start, out_end);

            // output sql
            linevector_t sql_lines = chunk.getSqlLines();
            for (linevector_t::iterator lit = sql_lines.begin(); lit != sql_lines.end(); ++lit) {
                if (((*lit)->number >= out_start) &&
                    ((*lit)->number <= out_end)) {

                    if ((*lit)->number == chunk.diagnostics.error_line) {
                        printf("%s", ansi_code(ANSI_RED));
                    }
                    printf("%s\n", (*lit)->contents.c_str());
                    if ((*lit)->number == chunk.diagnostics.error_line) {
                        printf("%s", ansi_code(ANSI_RESET));
                    }
                }

                if ((*lit)->number >= out_end) {
                    break;
                }
            }
            printf("\n");
        }
        else {
            printf("%s", ansi_code(ANSI_RESET));
        }
        printf("%s\n", s_fail_sep);
    }

}

inline CommandRc
cmd_run(Chunk & chunk, Db & db)
{
    if (settings.is_terminal) {
        printf("RUN   [%d-%d] %s", chunk.start_line, chunk.end_line, chunk.getDescription().c_str());
    }

    bool run_ok = db.runChunk(chunk);
    if (settings.is_terminal) {
        printf("\r");
    }

    if (run_ok) {
        printf("%sOK%s  ", ansi_code(ANSI_GREEN), ansi_code(ANSI_RESET));
    }
    else {
        printf("%sFAIL%s", ansi_code(ANSI_RED), ansi_code(ANSI_RESET));
    }
    printf("  [%d-%d] %s\n", chunk.start_line, chunk.end_line, chunk.getDescription().c_str());

    if (!run_ok) {
        cmd_run_print_diagnostics(chunk);
        if (settings.abort_after_failed) {
            printf("Chunk failed. Aborting.\n");
            return BREAK;
        }
    }

    return OK;
}


void
print_header(const char * filename)
{
    if (settings.print_filenames) {
        printf("\n----[ File: %s%s%s\n", ansi_code(ANSI_BLUE), filename,
                ansi_code(ANSI_RESET));
    }

    switch (settings.command) {
        case LIST:
            cmd_list_printheader();
            break;
        default:
            break;
    }
}


CommandRc
scan(ChunkScanner & scanner, Db & db)
{
    Chunk chunk;
    CommandRc crc = OK;

    while (scanner.nextChunk(chunk)) {
        switch (settings.command) {
            case CONCAT:
                crc = cmd_concat(chunk);
                break;
            case LIST:
                crc = cmd_list(chunk);
                break;
            case RUN:
                crc = cmd_run(chunk, db);
                break;
        }

        if (crc != OK) {
            break;
        }
    }
    return crc;
}


int
handle_files(char * files[], int nufiles)
{
    CommandRc crc = OK;
    int rc = RC_OK;
    Db db;

    // allow signal handlers to access db
    db_ptr = &db;

    try {
        // setup the database connection if the command
        // requires one
        if (settings.command == RUN) {
            const char * password = NULL;
            std::string prompt_passwd;

            if (settings.ask_pass) {
                printf("Password: ");
                prompt_passwd = read_password();
                password = prompt_passwd.c_str();
            }

            bool connected = db.connect(settings.db_host, settings.db_name,
                                        settings.db_port, settings.db_user, password);
            if (!connected) {
                fprintf(stderr, "%s\n", db.getErrorMessage().c_str());
                rc = RC_E_USAGE;
            }
            else {
                if (settings.client_encoding != NULL) {
                    if (!db.setEncoding(settings.client_encoding)) {
                        fprintf(stderr, "Could not set encoding to %s.\n", settings.client_encoding);
                        rc = RC_E_USAGE;
                    }
                }
            }

            db.setCommit(settings.commit_sql);
        }

        if (rc == RC_OK) {
            for( int i = 0; ((i < nufiles) && (crc == OK)); i++ ) {
                if (strcmp(files[i], "-") == 0) {
                    // read from stdin
                    print_header("stdin");
                    ChunkScanner chunkscanner(std::cin);
                    crc = scan(chunkscanner, db);
                }
                else {
                    print_header(files[i]);

                    // open the file
                    std::ifstream is;
                    is.open(files[i]);

                    if (is.fail()) {
                        fprintf(stderr, "Could not open file \"%s\".\n", files[i]);
                        rc = RC_E_USAGE;
                        break;
                    }
                    ChunkScanner chunkscanner(is);
                    crc = scan(chunkscanner, db);
                }
            }
        }
    }
    catch (DbException &e) {
        printf("Fatal error: %s\n", e.what());
        rc = RC_E_DB;
    }

    // end message
    if ((rc == RC_OK) && (settings.command == RUN)) {
        if (db.getFailedCount() == 0) {
            printf("\nAll chunks passed.\n");
            if (settings.commit_sql) {
                printf("%sCommit%s\n", ansi_code(ANSI_YELLOW), ansi_code(ANSI_RESET));
            }
            else {
                printf("%sRollback%s\n", ansi_code(ANSI_YELLOW), ansi_code(ANSI_RESET));
            }
        }
        else {
            printf("\n%d chunks failed.\n", db.getFailedCount());
            rc = RC_E_SQL;
            printf("%sRollback%s\n", ansi_code(ANSI_YELLOW), ansi_code(ANSI_RESET));
        }
    }

    db_ptr = NULL;
    return rc;
}


int
main(int argc, char * argv[] )
{
    // register signal handler
    struct sigaction sigint_act, o_sigint_act;
    sigint_act.sa_handler = handle_sigint;
    sigemptyset(&sigint_act.sa_mask);
    sigint_act.sa_flags = 0;
    if (sigaction(SIGINT, &sigint_act, &o_sigint_act) != 0) {
        log_error("could not register sigint handler");
        return RC_E_OTHER;
    }

    // use is_terminal output if run in a shell
    if (isatty(fileno(stdout)) == 1) {
        settings.is_terminal = true;
        // disable output buffering
        setvbuf (stdout, NULL, _IONBF, BUFSIZ);
    };

    // read options
    char opt;
    while ( (opt = getopt(argc, argv, "l:p:U:d:h:WCaFE:")) != -1) {
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
            case 'l':
                {
                    std::stringstream context_lines_ss;
                    context_lines_ss << optarg;

                    int context_lines_i;
                    context_lines_ss >> context_lines_i;
                    if (context_lines_ss.fail()) {
                        quit("Illegal value for context lines");
                    }
                    if (context_lines_i < 0) {
                        quit("Illegal value for context lines. Context lines must be positive.");
                    }
                    settings.context_lines = static_cast<unsigned int>(context_lines_i);
                    log_debug("context_lines: %ud", settings.context_lines);
                }
                break;
            case 'h':
                settings.db_host = optarg;
                break;
            case 'W':
                settings.ask_pass = true;
                break;
            case 'C':
                settings.commit_sql = true;
                break;
            case 'a':
                settings.abort_after_failed = true;
                break;
            case 'F':
                settings.print_filenames = false;
                break;
            case 'E': /* client_encoding */
                settings.client_encoding = optarg;
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
