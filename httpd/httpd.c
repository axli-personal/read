// Created November 1999 by J. David Blackstone.
// Modified February 2022 by AoXiang Li.

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>

#include "lib/response.h"
#include "lib/util.h"

#define ISspace(x) isspace((int)(x))

int  startup(u_short *);
void accept_request(int);
int  get_line(int, char *, int);
void send_file(int, FILE *);
void serve_file(int, const char *);

// Accept the request bind to the socket with descriptor CLIENT.
void accept_request(int client)
{
    char buf[1024];

    int n = get_line(client, buf, sizeof(buf));

    // Get METHOD from BUF.
    char method[255];
    int i = 0;
    while (i < sizeof(method) - 1 && !ISspace(buf[i]))
    {
        method[i] = buf[i];
        i++;
    }
    method[i] = '\0';

    if (strcasecmp(method, "GET") != 0)
    {
        unimplemented(client);
        return;
    }

    int j = i;
    while (j < n && ISspace(buf[j]))
    {
        j++;
    }

    // Get URL from BUF.
    char url[255];
    i = 0;
    while (i < sizeof(url) - 1 && j < n && !ISspace(buf[j]))
    {
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';

    // Convert RUL to local PATH.
    char path[512];
    sprintf(path, "htdocs%s", url);
    if (path[strlen(path) - 1] == '/')
    {
        strcat(path, "index.html");
    }

    struct stat st;
    if (stat(path, &st) == -1)
    {
        // Read and discard headers.
        while (n > 0 && buf[0] != '\n')
        {
            n = get_line(client, buf, sizeof(buf));
        }
        not_found(client);
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        strcat(path, "/index.html");
    }

    serve_file(client, path);
}

// Send file RESOURCE to the socket with descriptor CLIENT.
void send_file(int client, FILE * resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
    int  i = 0;
    int  n = 0;
    char c = '\0';

    while (i < size - 1 && c != '\n')
    {
        n = recv(sock, &c, 1, 0);
        if (n < 1) break;

        if (c == '\r')
        {
            n = recv(sock, &c, 1, MSG_PEEK);
            // Check following character.
            if (n > 0 && c == '\n') recv(sock, &c, 1, 0);
            else                    c = '\n';
        }

        buf[i] = c;
        i++;
    }

    buf[i] = '\0';

    return i;
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename)
{
    char buf[1024];
    /* Read and discard headers */
    int n = get_line(client, buf, sizeof(buf));
    while (n > 0 && buf[0] != '\n')
    {
        n = get_line(client, buf, sizeof(buf));
    }

    FILE * resource = fopen(filename, "r");
    if (resource == NULL)
    {
        not_found(client);
    }
    else
    {
        headers(client, filename);
        send_file(client, resource);
    }
    fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{
    int httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
    {
        fatal("socket");
    }

    int on = 1;
    if ((setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0)
    {
        fatal("setsockopt failed");
    }

    struct sockaddr_in name;
    name.sin_family      = AF_INET;
    name.sin_port        = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
    {
        fatal("bind");
    }

    if (*port == 0)
    {
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1) {
            fatal("getsockname");
        }
        *port = ntohs(name.sin_port);
    }

    if (listen(httpd, 5) < 0)
    {
        fatal("listen");
    }

    return httpd;
}

int main()
{
    unsigned short     port            = 4000;
    struct sockaddr_in client_addr;
    socklen_t          client_addr_len = sizeof(client_addr);
    int                server_sock     = -1;
    int                client_sock     = -1;

    server_sock = startup(&port);

    printf("httpd running on port %d\n", port);

    while (1)
    {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock == -1)
        {
            fatal("accept");
        }
        accept_request(client_sock);
        close(client_sock);
    }

    close(server_sock);
}
