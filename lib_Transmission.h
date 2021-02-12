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
#ifndef TRANSMISSION_H
#define TRANSMISSION_H
#define NDEBUG

#include "mbed.h"
#include "EthernetInterface.h"
#include <sstream>

#define TRANSMISSION_DEFAULT_BUFFER_SIZE    1072                // taille des buffers de reception serial et tcp
#define TRANSMISSION_DEFAULT_SMTP_SERVER    "129.175.212.70"    // IP sinon obligation d'utilisation du DNS avec _eth.getHostByName("smtp.u-psud.fr")
#define TRANSMISSION_DEFAULT_NTP_SERVER     "129.175.34.43"     // IP sinon obligation d'utilisation du DNS avec _eth.getHostByName("ntp.u-psud.fr")

/** Transmission class
 */
class Transmission
{
    public:
        typedef enum { SERIAL, TCP, HTTP, ANY }
            enum_trans_to;
        typedef enum { WHITE, CYAN, MAGENTA_ACCEPT, BLUE_CLIENT, YELLOW_CONNECTING, GREEN_GLOBAL_UP, RED_DISCONNECTED, BLACK_INITIALIZE }
            enum_trans_status;
        struct { const char *RETURN_OK; const char *RETURN_NO_CONTENT; const char *RETURN_MOVED; const char *RETURN_FOUND; const char *RETURN_SEE_OTHER; const char *RETURN_REDIRECT; const char *RETURN_NOT_FOUND; }
            http = { "HTTP/1.1 200 OK\r\n", "HTTP/1.1 204 No Content\r\n", "HTTP/1.1 301 Moved Permanently\r\n", "HTTP/1.1 302 Found\r\n", "HTTP/1.1 303 See Other\r\n", "HTTP/1.1 307 Temporary Redirect\r\n", "HTTP/1.1 404 Not Found\r\n" };
        /** make new Transmission instance
        * connected to 
        *
        * @param 
        * @param 
        */
        Transmission(
            #if MBED_MAJOR_VERSION > 5
            UnbufferedSerial    *serial,
            #else
            Serial              *serial,
            #endif
            EthernetInterface   *eth, 
            void(*init)(void), 
            void(*processing)(string, Transmission::enum_trans_to));
        
        /** 
        *
        * @param 
        * @param 
        * @returns 
        */
        string              ip(const bool& SET, const char* IP="", const uint16_t& PORT=80, const uint16_t& TIMEOUT=100);
        /** 
        *
        * @param 
        * @param 
        * @returns 
        */
        string              ip(void);
        /** 
        *
        * @param 
        * @param 
        * @returns 
        */
        enum_trans_status   recv(void);
        /** 
        *
        * @param 
        * @param 
        * @returns 
        */
        nsapi_error_t       send(const string& BUFFER="", const enum_trans_to& TO=ANY);
        /** 
        *
        * @param 
        * @param 
        * @returns 
        */
        bool                smtp(const char* MAIL, const char* FROM="", const char* SUBJECT="", const char* DATA="", const char* SERVER=TRANSMISSION_DEFAULT_SMTP_SERVER);
        /** 
        *
        * @param 
        * @param 
        * @returns 
        */
        time_t              ntp(const char* SERVER=TRANSMISSION_DEFAULT_NTP_SERVER);

    private:
        #if MBED_MAJOR_VERSION > 5
        UnbufferedSerial    *_serial;
        #else
        Serial              *_serial;
        #endif
        TCPSocket           _serverTCP, *_clientTCP = NULL;
        EventQueue          *_queue;
        EthernetInterface   *_eth;

        /* Serial */
        void                serial_event(void);

        /* EthernetInterface */
        void                eth_state(void);
        bool                eth_connect(void);
        void                eth_event(nsapi_event_t, intptr_t);
        intptr_t            eth_status(const string&, const intptr_t&);
        nsapi_error_t       eth_error(const string& SOURCE, const nsapi_error_t& CODE);

        /* serverTCP */
        bool                serverTCP_connect(void);
        void                serverTCP_accept(void);
        void                serverTCP_event(void);

        /* Transmission */
        void                (*_init)(void);
        void                (*_processing)(string, enum_trans_to);
        struct              { enum_trans_status status; bool SET; bool BREAK; bool DHCP; bool CONNECT; string IP; uint16_t TIMEOUT; uint16_t PORT; }
                            message = { RED_DISCONNECTED, false, false, false, false, "", 100, 80 };
};
#endif