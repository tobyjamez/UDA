//! $LastChangedRevision: 353 $
//! $LastChangedDate: 2013-11-18 15:32:28 +0000 (Mon, 18 Nov 2013) $
//! $LastChangedBy: dgm $
//! $HeadURL: https://fussvn.fusion.culham.ukaea.org.uk/svnroot/IDAM/development/source/client/Readin.c $

// Create the Client Side XDR File Stream Reader Function
//
// Change History
//
// 21Mar2007	dgm	DB_Socket renamed clientSocket
// 31Jan2011	dgm	Windows sockets implementation
//----------------------------------------------------------------

#include "Readin.h"

#include <unistd.h>

#include "UpdateSelectParms.h"
#include "idamErrorLog.h"

int idamClientReadin(void* iohandle, char* buf, int count)
{
    int rc, serrno;
    fd_set rfds;
    struct timeval tv;

    int maxloop = 0;

    errno = 0;

    /* Wait till it is possible to read from socket */

    idamUpdateSelectParms(clientSocket, &rfds, &tv);

    while ((select(clientSocket + 1, &rfds, NULL, NULL, &tv) <= 0) && maxloop++ < MAXLOOP) {
        idamUpdateSelectParms(clientSocket, &rfds, &tv);        // Keep trying ...
    }

// Read from it, checking for EINTR, as happens if called from IDL

#ifndef _WIN32
    while (((rc = (int) read(clientSocket, buf, count)) == -1) && (errno == EINTR)) { }
#else
    while ((( rc=recv( clientSocket, buf, count, 0)) == SOCKET_ERROR ) && (errno == EINTR)) {}

    // if rc == 0 then socket is closed => server fail?

#endif

    serrno = errno;

// As we have waited to be told that there is data to be read, if nothing
// arrives, then there must be an error

    if (!rc) {
        rc = -1;
        if (serrno != 0 && serrno != EINTR) addIdamError(&idamerrorstack, SYSTEMERRORTYPE, "idamClientReadin", rc, "");
        addIdamError(&idamerrorstack, CODEERRORTYPE, "idamClientReadin", rc,
                     "No Data waiting at Socket when Data Expected!");
    }

    return rc;
}
