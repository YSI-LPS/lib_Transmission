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

#include "mbed.h"
#include "EthernetInterface.h"
#include <sstream>

#define NDEBUG
#define REQUEST_TIMEOUT     100                 // config client bloquante avec timeout sinon limite de transmission a 1072 octets
#define SMTP_SERVER         "129.175.212.70"    // IP sinon utilisation du DNS avec _eth.getHostByName("smtp.u-psud.fr")
#define NTP_SERVER          "129.175.34.43"     // IP sinon utilisation du DNS avec _eth.getHostByName("ntp.u-psud.fr")


/** Transmission class
 */
class Transmission
{
    public:
        typedef enum { SERIAL, TCP, HTTP, ANY } enum_trans_to;
        typedef enum { WHITE, CYAN, MAGENTA_ACCEPT, BLUE_CLIENT, YELLOW_CONNECTING, GREEN_GLOBAL_UP, RED_DISCONNECTED, BLACK_INITIALIZE } enum_trans_status;
        /** make new Transmission instance
        * connected to 
        *
        * @param 
        * @param 
        */
        #if MBED_MAJOR_VERSION > 5
        Transmission(UnbufferedSerial *serial, EthernetInterface *eth, void(*init)(void), void(*processing)(string, Transmission::enum_trans_to));
        #else
        Transmission(Serial *serial, EthernetInterface *eth, void(*init)(void), void(*processing)(string, Transmission::enum_trans_to));
        #endif
        
        /** 
        *
        * @param 
        * @param 
        * @returns none
        */
        string              set(bool SET, const char* IP="", uint16_t PORT=80);
        /** 
        *
        * @param 
        * @param 
        * @returns none
        */
        string              get(void);
        /** 
        *
        * @param 
        * @param 
        * @returns none
        */
        enum_trans_status   recv(void);
        /** 
        *
        * @param 
        * @param 
        * @returns none
        */
        nsapi_error_t       send(const string& BUFFER, const enum_trans_to& TO);
        /** 
        *
        * @param 
        * @param 
        * @returns none
        */
        bool                smtp(const char* MAIL, const char* FROM="", const char* SUBJECT="", const char* DATA="");
        /** 
        *
        * @param 
        * @param 
        * @returns none
        */
        time_t              ntp(const char* ADDRESS="");

    private:
        Thread              _queueThread;
        EventQueue          _queue;
        #if MBED_MAJOR_VERSION > 5
        UnbufferedSerial    *_serial;
        #else
        Serial              *_serial;
        #endif
        EthernetInterface   *_eth;
        TCPSocket           *_clientTCP = NULL;
        TCPSocket           _serverTCP;

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
        struct              typeTRANSMISSION { string serial; enum_trans_status status; bool SET; bool BREAK; bool DHCP; bool CONNECT; string IP; uint16_t PORT; } message = { "", RED_DISCONNECTED, false, false, false, false, "", 80 };
};
#endif