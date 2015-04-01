#ifndef Socket_h
#define Socket_h

#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> // internet address family (for sockaddr_in)
#include <arpa/inet.h> // definitions for internet operations
#include <netdb.h>
#include <unistd.h>
#include <signal.h>

#include <errno.h>
#include <fcntl.h>

#include <set>
#include <sstream>
#include <iostream>

#include "Exception.h"
#include "Message.h"
#include "HTTPMessage.h"

#define SOCKET_ERROR(x) 10000+x
#define MAX_WORD_LENGTH 500

class Socket;
typedef std::set<int> SocketCollection;
/**
 * General object providing all useful method to connect/bind/send/receive
 * information through system sockets.
 *
 * \author Laurent Forthomme <laurent.forthomme@cern.ch>
 * \date 23 Mar 2015
 */
class Socket
{
  public:
    inline Socket() {;}
    Socket(int port);
    virtual ~Socket();
    
    inline void SetPort(int port) { fPort=port; }
    /// Retrieve the port used for this socket
    inline int GetPort() const { return fPort; }
  
    /**
     * Set the socket to accept connections from clients
     * \brief Accept connections from outside
     */
    void AcceptConnections(Socket& socket);
    void SelectConnections();
    
    inline void SetSocketId(int sid) { fSocketId=sid; }
    inline int GetSocketId() const { return fSocketId; }

    void DumpConnected() const;
    
  protected:
    /**
     * Launch all mandatory operations to set the socket to be used
     * \brief Start the socket
     * \return Success of the operation
     */
    bool Start();
    /// Terminates the socket and all attached communications
    void Stop();
    /**
     * \brief Bind a name to a socket
     * \return Success of the operation
     */
    void Bind();
    void PrepareConnection();
    /**
     * Set the socket to listen to any message coming from outside
     * \brief Listen to incoming messages
     */
    void Listen(int maxconn);
    
    /**
     * \brief Send a message on a socket
     */
    void SendMessage(Message message, int id=-1);
    /**
     * \brief Receive a message from a socket
     * \return Received message as a std::string
     */
    Message FetchMessage(int id=-1);
    
    int fPort;
    char fBuffer[MAX_WORD_LENGTH];
    SocketCollection fSocketsConnected;
    /// Master file descriptor list
    fd_set fMaster;
    /// Temp file descriptor list for select()
    fd_set fReadFds;
    
  private:
    /**
     * \brief Create an endpoint for communication
     */
    void Create();
    void Configure();
    void SetNonBlock(bool nb);
    
    //struct sockaddr_in6 fAddress;
    struct sockaddr_in fAddress;
    
    /**
     * A file descriptor for this socket, if \a Create was performed beforehand.
     */
    int fSocketId;
};

#endif