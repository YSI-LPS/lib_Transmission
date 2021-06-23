/** Transmission class
*
* @purpose       library for Transmission
*
* Use to Transmission
*
* Example:
* @code
* #define MBED_PROJECT    "Transmission"
* 
* #include "lib_Transmission.h"
* 
* #if MBED_MAJOR_VERSION > 5
* UnbufferedSerial    pc(USBTX, USBRX, 230400);
* #else
* Serial              pc(USBTX, USBRX, 230400);
* #endif
* 
* string              transmission_processing(string);
* Transmission        transmission(&pc, &transmission_processing);
* 
* int main(void)
* {
*     while(1) ThisThread::sleep_for(200ms);
* }
* 
* string transmission_processing(string cmd)
* {
*     ostringstream ssend;
*     ssend << fixed;
*     ssend.precision(2);
*     if(cmd.empty());
*     else if(cmd == "*IDN?")
*         ssend << MBED_PROJECT << ", Mbed OS " << MBED_VERSION << ", Version dated, " << __DATE__ << ", " << __TIME__;
*     else if(cmd[cmd.size()-1] == '?')
*         ssend << "incorrect requeste [" << cmd << "]";
*     return ssend.str();
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
#include "USBCDC.h"
#include "EthernetInterface.h"
#include <sstream>

#define TRANSMISSION_DEFAULT_BUFFER_SIZE    1072                // taille des buffers de reception
#define TRANSMISSION_DEFAULT_SMTP_SERVER    "129.175.212.70"    // IP sinon obligation d'utilisation du DNS avec _eth.getHostByName("smtp.u-psud.fr")
#define TRANSMISSION_DEFAULT_NTP_SERVER     "129.175.34.43"     // IP sinon obligation d'utilisation du DNS avec _eth.getHostByName("ntp.u-psud.fr")

/** Transmission class
 */
class Transmission
{
    public:
        /** 
        *
        * @param 
        * @param 
        * @returns 
        */
        typedef enum { USB_DELIVERY, SERIAL_DELIVERY, TCP_DELIVERY, HTTP_DELIVERY, ANY_DELIVERY }
            enum_trans_delivery;
        /** 
        *
        * @param 
        * @param 
        * @returns 
        */
        typedef enum { WHITE_STATUS, CYAN_STATUS, MAGENTA_ACCEPT, BLUE_CLIENT, YELLOW_CONNECTING, GREEN_GLOBAL_UP, RED_DISCONNECTED, BLACK_INITIALIZE }
            enum_trans_status;
        /** 
        *
        * @param 
        * @param 
        * @returns 
        */
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
            USBCDC              *usb,
            EthernetInterface   *eth,
            string              (*processing)(string),
            void                (*ethup)(void) = NULL,
            bool                caseIgnore = true);
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
            USBCDC              *usb,
            string              (*processing)(string),
            bool                caseIgnore = true);
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
            string              (*processing)(string),
            void                (*ethup)(void) = NULL,
            bool                caseIgnore = true);
        /** make new Transmission instance
        * connected to 
        *
        * @param 
        * @param 
        */
        Transmission(
            USBCDC              *usb,
            EthernetInterface   *eth,
            string              (*processing)(string),
            void                (*ethup)(void) = NULL,
            bool                caseIgnore = true);
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
            string              (*processing)(string),
            bool                caseIgnore = true);
        /** make new Transmission instance
        * connected to 
        *
        * @param 
        * @param 
        */
        Transmission(
            EthernetInterface   *eth,
            string              (*processing)(string),
            void                (*ethup)(void) = NULL,
            bool                caseIgnore = true);
        /** make new Transmission instance
        * connected to 
        *
        * @param 
        * @param 
        */
        Transmission(
            USBCDC              *usb,
            string              (*processing)(string),
            bool                caseIgnore = true);
        /** 
        *
        * @param 
        * @param 
        * @returns 
        */
        string              ip(const bool SET, const char* IP="", const uint16_t PORT=80, const char* MASK="255.255.255.0", const char* GATEWAY="192.168.1.1", const uint16_t TIMEOUT=100);
        /** 
        *
        * @param 
        * @param 
        * @returns 
        */
        string              ip(string IP="");
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
        nsapi_error_t       send(const string& BUFFER="", const enum_trans_delivery& DELIVERY=ANY_DELIVERY);
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
        UnbufferedSerial    *_serial = NULL;
        #else
        Serial              *_serial = NULL;
        #endif
        TCPSocket           _serverTCP, *_clientTCP = NULL;
        Thread              _eventThread;
        EventQueue          _queue;
        EthernetInterface   *_eth = NULL;
        USBCDC              *_usb = NULL;
        bool                _caseIgnore = false;

        void                serial_event(void);

        void                eth_state(void);
        bool                eth_connect(void);
        void                eth_event(nsapi_event_t, intptr_t);
        intptr_t            eth_status(const string&, const intptr_t&);
        nsapi_error_t       eth_error(const string& SOURCE, const nsapi_error_t& CODE);

        bool                serverTCP_connect(void);
        void                serverTCP_accept(void);
        void                serverTCP_event(void);

        void                (*_ethup)(void);
        string              (*_processing)(string);
        void                preprocessing(char *, const enum_trans_delivery);
        struct              { enum_trans_status status; bool SET; bool DHCP; bool CONNECT; string IP; uint16_t PORT; string MASK; string GATEWAY; uint16_t TIMEOUT; }
                            message = { RED_DISCONNECTED, false, false, false, "192.168.1.1", 80, "255.255.255.0", "192.168.1.0", 100 };
};
#endif