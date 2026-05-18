/** Transmission class
*
* @purpose       library for Transmission
*
* Use to Transmission
*
* Example:
* @code
* #include "lib_Transmission.h"
* 
* #define MBED_PROJECT      "Transmission"
* #define SERIAL_BAUDRATE   230400
* 
* string      processing(string);
* 
* struct {
*             const string        idn;
*             UnbufferedSerial    *ss;
*             USBCDC              *usb;
*             EthernetInterface   *eth;
*             Transmission        *tr;
* } uc = {
*             MBED_PROJECT + ((string)" Rev. ") + MBED_STRINGIFY(TARGET_NAME) + " " + __DATE__ + " " + __TIME__ + " MbedOS " + to_string(MBED_VERSION),
*             #if MBED_MAJOR_VERSION > 5
*             new UnbufferedSerial(USBTX, USBRX, SERIAL_BAUDRATE),
*             #else
*             new Serial(USBTX, USBRX, SERIAL_BAUDRATE),
*             #endif
*             new USBCDC(false),
*             new EthernetInterface,
*             new Transmission(uc.ss, uc.usb, uc.eth, &processing)
* };
* 
* int main(void)
* {
*     uc.tr->ip(true, "192.168.1.25");
*     while(1) uc.tr->recv();
* }
* 
* string processing(string cmd)
* {
*     ostringstream ssend;
*     ssend << fixed;
*     ssend.precision(2);
*     if(cmd.empty());
*     else if(cmd == "*IDN?\n")
*         return uc.idn;
*     else if(cmd == "MAC?\n")
*         ssend << "MAC\t[" << (uc.eth->get_mac_address()?uc.eth->get_mac_address():"00:00:00:00:00:00") << "]";
*     else if(cmd.find("?") != string::npos)
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

#include <sstream>
#include "mbed.h"
#include "USBCDC.h"
#include "EthernetInterface.h"

#define TRANSMISSION_BUFFER_SIZE    (2 * MBED_CONF_LWIP_TCP_MSS)    // MBED_CONF_LWIP_TCP_MSS == 536 pas suffisant avec les entetes HTTP souvent > 536 octets
#define TRANSMISSION_SMTP_SERVER    "129.175.212.70"                // IP sinon obligation d'utilisation du DNS avec _eth.getHostByName("smtp.u-psud.fr")
#define TRANSMISSION_NTP_SERVER     "129.175.34.43"                 // IP sinon obligation d'utilisation du DNS avec _eth.getHostByName("ntp.u-psud.fr")

/** Transmission class
 */
class Transmission
{
    public:
        /** enumerator of the different possible delivery of the transmission library
        */
        typedef enum { USB_DELIVERY, SERIAL_DELIVERY, TCP_DELIVERY, HTTP_DELIVERY, ANY_DELIVERY }
            enum_trans_delivery;
        /** enumerator of the different ethernet connexion status of the transmission library
        */
        typedef enum { WHITE_STATUS, CYAN_STATUS, MAGENTA_ACCEPT, BLUE_CLIENT, YELLOW_CONNECTING, GREEN_GLOBAL_UP, RED_DISCONNECTED, BLACK_INITIALIZE }
            enum_trans_status;
        /** List of http headers returns
        */
        struct { const char *RETURN_OK; const char *RETURN_NO_CONTENT; const char *RETURN_RESET_CONTENT; const char *RETURN_MOVED; const char *RETURN_FOUND; const char *RETURN_SEE_OTHER; const char *RETURN_REDIRECT; const char *RETURN_NOT_FOUND; }
            http = { "HTTP/1.1 200 OK\r\n", "HTTP/1.1 204 No Content\r\n", "HTTP/1.1 205 Reset Content\r\n", "HTTP/1.1 301 Moved Permanently\r\n", "HTTP/1.1 302 Found\r\n", "HTTP/1.1 303 See Other\r\n", "HTTP/1.1 307 Temporary Redirect\r\n", "HTTP/1.1 404 Not Found\r\n" };
        /** make new Transmission instance
        *
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
            string              (*processing)(string) = nullptr,
            void                (*ethup)(void) = nullptr,
            bool                TermChar = true,
            bool                caseIgnore = true);
        /** make new Transmission instance
        *
        * @param 
        */
        Transmission(
            #if MBED_MAJOR_VERSION > 5
            UnbufferedSerial    *serial,
            #else
            Serial              *serial,
            #endif
            USBCDC              *usb,
            string              (*processing)(string) = nullptr,
            bool                TermChar = true,
            bool                caseIgnore = true);
        /** make new Transmission instance
        *
        * @param 
        */
        Transmission(
            #if MBED_MAJOR_VERSION > 5
            UnbufferedSerial    *serial,
            #else
            Serial              *serial,
            #endif
            EthernetInterface   *eth,
            string              (*processing)(string) = nullptr,
            void                (*ethup)(void) = nullptr,
            bool                TermChar = true,
            bool                caseIgnore = true);
        /** make new Transmission instance
        *
        * @param 
        */
        Transmission(
            USBCDC              *usb,
            EthernetInterface   *eth,
            string              (*processing)(string) = nullptr,
            void                (*ethup)(void) = nullptr,
            bool                TermChar = true,
            bool                caseIgnore = true);
        /** make new Transmission instance
        *
        * @param 
        */
        Transmission(
            #if MBED_MAJOR_VERSION > 5
            UnbufferedSerial    *serial,
            #else
            Serial              *serial,
            #endif
            string              (*processing)(string) = nullptr,
            bool                TermChar = true,
            bool                caseIgnore = true);
        /** make new Transmission instance
        *
        * @param 
        */
        Transmission(
            EthernetInterface   *eth,
            string              (*processing)(string) = nullptr,
            void                (*ethup)(void) = nullptr,
            bool                TermChar = true,
            bool                caseIgnore = true);
        /** make new Transmission instance
        *
        * @param 
        */
        Transmission(
            USBCDC              *usb,
            string              (*processing)(string) = nullptr,
            bool                TermChar = true,
            bool                caseIgnore = true);
        /** Configure the TCP connection
        *
        * @param set enable for the tcp connection
        * @returns MAC adress
        */
        string              ip(const bool set, const char* ip="", const uint16_t port=80, const char* mask="255.255.255.0", const char* gateway="192.168.1.1", const uint16_t timeout=100);
        /** Return ip config
        *
        * @param ip="" If the specified ip is the one configured gives the complete ip configuration
        * @returns ip config
        */
        string              ip(string ip="");
        /** Return client config
        *
        * @returns ip:port config
        */
        string              client(void);
        /** scans the reception buffers of transmission TCP and USB
        *
        * @returns enumerator of the different ethernet connexion status of the transmission library
        */
        enum_trans_status   recv(void);
        /** send the buffer to the specified transmission delivery
        *
        * @param ssend sent over transmission
        * @param delivery of the transmission
        * @returns 
        */
        nsapi_error_t       send(const string& ssend, const enum_trans_delivery& delivery=ANY_DELIVERY);
        /** send a request to a server at the specified port
        *
        * @param ssend sent to server
        * @param server ip
        * @param port port
        * @returns server response
        */
        string              get(const string& ssend, const string& server, const int& port=80);
        /** send an email to an smtp server
        *
        * @param mail is the recipient's email
        * @param from this is the sender's email
        * @param subject this is the subject of the email
        * @param data this is the content of the email
        * @param server="129.175.212.70" this is an ip from an smtp server
        * @returns indicates if the smtp transaction was successful
        */
        bool                smtp(string mail, string from, string subject, string data, const char* server=TRANSMISSION_SMTP_SERVER);
        /** ask an smtp server if email exist
        *
        * @param mail is the recipient's email
        * @param server="129.175.212.70" this is an ip from an smtp server
        * @returns indicates if the smtp transaction was successful
        */
        bool                exist(string mail, const char* server=TRANSMISSION_SMTP_SERVER);
        /** time request to an ntp server
        *
        * @param server="129.175.34.43" this is an ip from an ntp server
        * @returns time
        */
        time_t              ntp(const char* server=TRANSMISSION_NTP_SERVER);

    private:
        #if MBED_MAJOR_VERSION > 5
        UnbufferedSerial    *_serial = nullptr;
        #else
        Serial              *_serial = nullptr;
        #endif
        TCPSocket           _serverTCP, *_clientTCP = nullptr;
        Thread              *_evenThread = nullptr;
        EventQueue          _queue;
        EthernetInterface   *_eth = nullptr;
        USBCDC              *_usb = nullptr;
        bool                _caseIgnore = false;
        bool                _TermChar = false;

        void                serial_event(void);

        void                eth_state(void);
        bool                eth_connect(void);
        void                eth_event(nsapi_event_t, intptr_t);
        intptr_t            eth_status(const string&, const intptr_t&);
        nsapi_error_t       eth_error(const string& SOURCE, const nsapi_error_t& CODE);

        bool                serverTCP_connect(void);
        void                serverTCP_accept(void);

        void                (*_ethup)(void);
        string              (*_processing)(string);
        void                preprocessing(char *, const enum_trans_delivery);
        struct              { enum_trans_status status; bool SET; bool DHCP; bool CONNECT; string IP; uint16_t PORT; string MASK; string GATEWAY; uint16_t TIMEOUT; }
                            message = { RED_DISCONNECTED, false, false, false, "192.168.1.1", 80, "255.255.255.0", "192.168.1.0", 100 };
};
#endif
