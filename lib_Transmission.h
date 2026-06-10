/** Transmission class

@purpose       library for Transmission

Use to Transmission

Example:
@code
#include "lib_Transmission.h"
 
struct uc_t {
    static constexpr int SERIAL_BAUDRATE = 230400;
    const string PROJECT;
    const string idn;
    UnbufferedSerial *ss;
    USBCDC *usb;
    EthernetInterface *eth;
    Transmission *tr;
    int color[8];
    BusOut *leds;
    const Transmission::tr_status_t init;
    Transmission::tr_status_t state;
    
    static string fn_processing(string cmd, Transmission::_clientTCP_t *client=nullptr);
    static void fn_ethup(void);
    uc_t():
        PROJECT("SensorIP"),
        idn(PROJECT + ((string)" Rev. ") + MBED_STRINGIFY(TARGET_NAME) + " " + __DATE__ + " " + __TIME__ + " MbedOS " + to_string(MBED_VERSION)),
        ss(new UnbufferedSerial(USBTX, USBRX, SERIAL_BAUDRATE)),
        usb(new USBCDC(false)),
        eth(new EthernetInterface),
        tr(new Transmission(ss, usb, eth, &fn_processing, &fn_ethup)),
        #if (defined(TARGET_LPC1768) || defined(TARGET_K64F))
        color{ 0, 1, 2, 3, 4, 5, 6, 7 },
        #elif (defined(TARGET_NUCLEO_H743ZI2) || defined(TARGET_NUCLEO_F767ZI) || defined(TARGET_NUCLEO_F429ZI)|| defined(TARGET_NUCLEO_F746ZG)|| defined(TARGET_NUCLEO_F756ZG))
        color{ 7, 6, 3, 2, 5, 1, 4, 0 },
        #else
        #error color not defined for this target
        #endif
        leds(new BusOut(LED1, LED2, LED3)),
        init(Transmission::WHITE_STATUS),
        state(Transmission::RED_DISCONNECTED)
    {}
} uc;
 
int main(void)
{
    uc.leds->write(uc.color[uc.init]);
    ThisThread::sleep_for(500ms);
    uc.leds->write(uc.color[uc.state]);
    uc.tr->ip(true);//, "192.168.1.25");
    while(true) uc.leds->write(uc.color[uc.state = uc.tr->recv()]);
}

void uc_t::fn_ethup()
{
    printf("ETHUP\n");
}

string uc_t::fn_processing(string cmd, Transmission::_clientTCP_t *client)
{
    ostringstream ssend;
    ssend << fixed;
    ssend.precision(2);
    if(cmd.empty());
    else if(cmd == "*IDN?\n")
        return uc.idn;
    else if(cmd == "MAC?\n")
        ssend << "MAC\t[" << (uc.eth->get_mac_address()?uc.eth->get_mac_address():"00:00:00:00:00:00") << "]";
    else if(cmd.find("?") != string::npos)
        ssend << "?";
    return ssend.str();
}
@endcode
@file          lib_Transmission.h 
@date          Jun 2020
@author        Yannic Simon
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
        enum tr_delivery_t {
            USB_DELIVERY,
            SERIAL_DELIVERY,
            TCP_DELIVERY,
            HTTP_DELIVERY,
            ANY_DELIVERY
        };
        /** enumerator of the different ethernet connexion status of the transmission library
        */
        enum tr_status_t {
            WHITE_STATUS,
            CYAN_STATUS,
            MAGENTA_ACCEPT,
            BLUE_CLIENT,
            YELLOW_CONNECTING,
            GREEN_GLOBAL_UP,
            RED_DISCONNECTED,
            BLACK_INITIALIZE
        };
        /** List of http headers returns
        */
        struct _returnHTTP_t {
            const char *RETURN_OK = "HTTP/1.1 200 OK\r\n";
            const char *RETURN_NO_CONTENT = "HTTP/1.1 204 No Content\r\n";
            const char *RETURN_RESET_CONTENT = "HTTP/1.1 205 Reset Content\r\n";
            const char *RETURN_MOVED = "HTTP/1.1 301 Moved Permanently\r\n";
            const char *RETURN_FOUND = "HTTP/1.1 302 Found\r\n";
            const char *RETURN_SEE_OTHER = "HTTP/1.1 303 See Other\r\n";
            const char *RETURN_REDIRECT = "HTTP/1.1 307 Temporary Redirect\r\n";
            const char *RETURN_NOT_FOUND = "HTTP/1.1 404 Not Found\r\n";
        } http;
        /** structure clientTCP
        */
        struct _clientTCP_t {
            static constexpr int MAX_TCP_CLIENTS = MBED_CONF_LWIP_TCP_SOCKET_MAX - 2; // 1 socket for serverTCP 2 for clientsTCP 1 for temporary socket
            TCPSocket *socket = nullptr;
            volatile bool event = true;
        };
    private:
        struct _linkTCP_t {
            tr_status_t status;
            bool SET;
            bool DHCP;
            bool SERVER;
            string IP;
            uint16_t PORT;
            string MASK;
            string GATEWAY;
            uint16_t TIMEOUT;
            _linkTCP_t():
                status(RED_DISCONNECTED),
                SET(false),
                DHCP(false),
                SERVER(false),
                IP("192.168.1.1"),
                PORT(80),
                MASK("255.255.255.0"),
                GATEWAY("192.168.1.0"),
                TIMEOUT(100)
            {}
        };

        struct _clientsTCP_t {
            volatile int active;
            volatile int event;
            _clientTCP_t list[_clientTCP_t::MAX_TCP_CLIENTS];
            _clientsTCP_t():
                active(0),
                event(0)
            {}
        };

        _linkTCP_t          _linkTCP;
        _clientsTCP_t       _clientsTCP;
        TCPSocket           _serverTCP;
        EventQueue          _queue;
        #if MBED_MAJOR_VERSION > 5
        UnbufferedSerial    *_serial = nullptr;
        #else
        Serial              *_serial = nullptr;
        #endif
        Thread              *_evenThread = nullptr;

        // Transmission constructor order, don't change
        USBCDC              *_usb = nullptr;
        EthernetInterface   *_eth = nullptr;
        string              (*_processing)(string, _clientTCP_t *);
        void                (*_ethup)(void);
        bool                _TermChar = false;
        bool                _caseIgnore = false;

        void                serial_event(void);

        bool                eth_connect(void);
        void                eth_event(nsapi_event_t, intptr_t);
        intptr_t            eth_status(const string&, const intptr_t&);
        nsapi_error_t       eth_error(const string& SOURCE, const nsapi_error_t& CODE);

        bool                serverTCP_connect(void);
        void                serverTCP_accept(void);
        void                serverTCP_close(void);

        void                clientTCP_event(_clientTCP_t &client);
        void                clientTCP_close(_clientTCP_t &client);
        
        void                preprocessing(char *, const tr_delivery_t, _clientTCP_t *client=nullptr);
    public:
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
            string              (*processing)(string, _clientTCP_t *) = nullptr,
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
            string              (*processing)(string, _clientTCP_t *) = nullptr,
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
            string              (*processing)(string, _clientTCP_t *) = nullptr,
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
            string              (*processing)(string, _clientTCP_t *) = nullptr,
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
            string              (*processing)(string, _clientTCP_t *) = nullptr,
            bool                TermChar = true,
            bool                caseIgnore = true);
        /** make new Transmission instance
        *
        * @param 
        */
        Transmission(
            EthernetInterface   *eth,
            string              (*processing)(string, _clientTCP_t *) = nullptr,
            void                (*ethup)(void) = nullptr,
            bool                TermChar = true,
            bool                caseIgnore = true);
        /** make new Transmission instance
        *
        * @param 
        */
        Transmission(
            USBCDC              *usb,
            string              (*processing)(string, _clientTCP_t *) = nullptr,
            bool                TermChar = true,
            bool                caseIgnore = true);
        /** Configure the TCP connection
        *
        * @param set enable for the tcp connection
        * @returns MAC adress
        */
        string ip(const bool set, const char* ip="", const uint16_t port=80, const char* mask="255.255.255.0", const char* gateway="192.168.1.1", const uint16_t timeout=100);
        /** Return ip config
        *
        * @param ip="" If the specified ip is the one configured gives the complete ip configuration
        * @returns ip config
        */
        string ip(string ip="");
        /** Return client config
        *
        * @returns ip:port config
        */
        string client(void);
        /** scans the reception buffers of transmission TCP and USB
        *
        * @returns enumerator of the different ethernet connexion status of the transmission library
        */
        tr_status_t recv(void);
        /** send the buffer to the specified transmission delivery
        *
        * @param ssend sent over transmission
        * @param delivery of the transmission
        * @returns nsapi_error_t code
        */
        nsapi_error_t send(const string& ssend, const tr_delivery_t delivery=ANY_DELIVERY, _clientTCP_t *client=nullptr);
        /** send a request to a server at the specified port
        *
        * @param ssend sent to server
        * @param server ip
        * @param port port
        * @returns server response
        */
        string get(const string& ssend, const string& server, const int& port=80);
        /** send an email to an smtp server
        *
        * @param mail is the recipient's email
        * @param from this is the sender's email
        * @param subject this is the subject of the email
        * @param data this is the content of the email
        * @param server="129.175.212.70" this is an ip from an smtp server
        * @returns indicates if the smtp transaction was successful
        */
        bool smtp(string mail, string from, string subject, string data, const char* server=TRANSMISSION_SMTP_SERVER);
        /** ask an smtp server if email exist
        *
        * @param mail is the recipient's email
        * @param server="129.175.212.70" this is an ip from an smtp server
        * @returns indicates if the smtp transaction was successful
        */
        bool exist(string mail, const char* server=TRANSMISSION_SMTP_SERVER);
        /** time request to an ntp server
        *
        * @param server="129.175.34.43" this is an ip from an ntp server
        * @returns time
        */
        time_t ntp(const char* server=TRANSMISSION_NTP_SERVER);
};
#endif
