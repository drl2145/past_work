## HTTP Server
This folder contains my code from an http server project.

mdb_lookup_server.c runs a local server that serves a local database text file called 'mdb' (each line of 'mdb' contains an entry). The mdb_lookup_server accepts database queries from a client and then sends the client a list of the entries that match the query.

http_server.c runs a local web server that serves the mdb_lookup_server. A client would connect to the http_server through a web browser. The client could submit queries to the databse through this http_server and the client would receive the results of the query in an html page. The client could also request specific images and other html pages stored locally.