This is an utility to run blocks/chunks of sql in a
postgresql database to test if they are valid on the block level.

The program will output a brief description which blocks passed and which
contained errors. In the case of an error psqlchunks tries to give
a detailed description including the SQL code from and around the
failing line.

The usecase for this tool is including database modifcations in a 
continous integraton process to ensure the validity of SQL files
before they are deployed.


Usage
-----

Most of the usage parameters are described in the help text
the program prints when called with

    psqlchunks help


Result:

    Usage :  
    psqlchunks command [options] files
    version: 0.6.0

    use - as filename to read from stdin.
    Definition of a chunk of SQL:
      A chunk of SQL is block of SQL statements to be executed together,
      and is delimited by the following markers:

      -------------------------------------------------------------
      -- start: creating my table
      -------------------------------------------------------------
      create table mytable (myint integer, mytext text);
      -------------------------------------------------------------
      -- end: creating my table
      -------------------------------------------------------------

      The end marker is optional and may be ommited.
      The shortest marker syntac understood by this tool is:

      ----
      -- start: creating my table
      create table mytable (myint integer, mytext text);


    Commands:
      print        print all SQL files and write the formatted output to stdout.
                   This command as the following aliasses: echo, concat.
      help         display this help text
      list         list chunks
      run          run SQL chunks in the database
                   This will not commit the SQL. But be aware that this tool
                   does not parse the SQL statments and will not filter out
                   COMMIT statements from the SQL files. Should there be any
                   in the files, the SQL WILL BE COMMITED and this tool will
                   terminate.

    General:
      -F           hide filenames from output

    Filters:
      -L [lines]   use only chunks which span the given lines.
                   lines is a commaseperated list of line numbers. Example:
                   1,78,345
      -I [regex]   match description comments with a regular expression.
                   (POSIX extended regular expression, case insensitive)
      -S [regex]   SQL has to match this POSIX extended regular expression,
                   also case insensitive.

    SQL Handling:
      -C           commit SQL to the database. Default is performing a rollback
                   after the SQL has been executed. A commit will only be
                   executed if no errors occured. (default: rollback)
      -a           abort execution after first failed chunk. (default: continue)
      -l           number of lines to output before and after failing lines
                   of SQL. (default: 2)
      -E           set the client_encoding of the database connection. This
                   setting is useful when the encoding of sql file differs
                   from the default client_encoding of the database server.

    Connection parameters:
      -d [database name]
      -U [user]
      -W           ask for password (default: don't ask)
      -h [host/socket name]

    Return codes:
      0            no errors
      1            invalid usage of this program
      2            the SQL contains errors
      3            (internal) database error


Limitations
-----------

- no transations inside the SQL file allowed. Will simply fail.
- COPY statements may not be executed


ToDo
----

- capture notices/warnings and print them without messing up the existing output
