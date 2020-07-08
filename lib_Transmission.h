/** Transmission class
*
* @purpose       library for Transmission
*
* Use to Transmission
*
* Example:
* @code
* #include "mbed.h"
*  
* int main()
* {
*     while(1)
*     {
*     }
* }
* @endcode
* @file          lib_Transmission.h 
* @date          Jun 2020
* @author        Yannic Simon
*/
#ifndef COM_H
#define COM_H

#include "mbed.h"
#include "EthernetInterface.h"
#include <sstream>

#define TCP_CLIENT_TIMEOUT      100                 // configue client bloquante avec timeout sinon limite de transmission a 1072 octets
#define SMTP_SERVER             "129.175.212.70"    // smtp.u-psud.fr oblige a utiliser DNS avec eth.getHostByName("smtp.u-psud.fr")

enum    enumTRANSMISSION    { TCP, SERIAL };
enum    enumTRANSTATUS      { WHITE, CYAN, MAGENTA_ACCEPT, BLUE_CLIENT, YELLOW_CONNECTING, GREEN_GLOBAL_UP, RED_DISCONNECTED, BLACK_INITIALIZE };
struct  typeTRANSMISSION    { string buffer[2]; enumTRANSTATUS status; bool TCP; bool HTTP; bool BREAK; bool DHCP; bool CONNECT; string IP; uint16_t PORT; };

/** Transmission class
 */
class Transmission
{
    public:
        uint16_t _PORT = 80;
        /** make new Transmission instance
        * connected to 
        *
        * @param 
        * @param 
        */
        Transmission(UnbufferedSerial *serial, EthernetInterface *eth, EventQueue *queue, void(*_init)(void), void(*_processing)(string, const enumTRANSMISSION&));
        
        /** 
        *
        * @param 
        * @param 
        * @returns none
        */ 
        /* communication */
        void            set(bool TCP, const char* IP="", uint16_t PORT=80);
        string          get(void);
        enumTRANSTATUS  recv(void);
        nsapi_error_t   send(const string& BUFFER, const enumTRANSMISSION& TYPE);
        bool            smtp(const char* MAIL, const char* FROM="", const char* SUBJECT="", const char* DATA="");
        void            http(void);

    private:
        typeTRANSMISSION    message = { {"", ""}, RED_DISCONNECTED, false, false, false, false, false, "", 80 };
        UnbufferedSerial    *_serial;
        EthernetInterface   *_eth;
        EventQueue          *_queue;
        TCPSocket           *_clientTCP = NULL;
        TCPSocket           _serverTCP;

        void            (*fn_init)(void);
        void            (*fn_processing)(string, const enumTRANSMISSION&);

        /* EthernetInterface */
        bool            eth_connect(void);
        void            eth_event(nsapi_event_t, intptr_t);
        intptr_t        eth_status(const string&, const intptr_t&);
        nsapi_error_t   eth_error(const string& SOURCE, const nsapi_error_t& CODE);

        /* serverTCP */
        bool            serverTCP_connect(void);
        void            serverTCP_accept(void);
        void            serverTCP_event(void);
};

#endif