.... some more documentation would be great.


Limitations
-----------

- no transations inside the SQL file allowed. Will simply fail.
- COPY statements may not be executed


ToDo
----

- cancel running queries when SIGINT is received.
  see http://www.postgresql.org/docs/8.4/static/libpq-cancel.html
- capture notices/warnings and print them without messing up the existing output