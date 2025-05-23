#include "lib_Transmission.h"

Transmission::Transmission(
    #if MBED_MAJOR_VERSION > 5
    UnbufferedSerial    *serial,
    #else
    Serial              *serial,
    #endif
    USBCDC              *usb,
    EthernetInterface   *eth,
    string              (*processing)(string),
    void                (*ethup)(void),
    bool                TermChar,
    bool                caseIgnore)
    :_serial(serial), _usb(usb), _eth(eth), _processing(processing), _ethup(ethup), _TermChar(TermChar), _caseIgnore(caseIgnore)
{
    if(_serial) _serial->attach(callback(this, &Transmission::serial_event));
    _evenThread = new Thread(osPriorityNormal, OS_STACK_SIZE);
    _evenThread->start(callback(&_queue, &EventQueue::dispatch_forever));
}

Transmission::Transmission(
    #if MBED_MAJOR_VERSION > 5
    UnbufferedSerial    *serial,
    #else
    Serial              *serial,
    #endif
    USBCDC              *usb,
    string              (*processing)(string),
    bool                TermChar,
    bool                caseIgnore)
    :_serial(serial), _usb(usb), _processing(processing), _TermChar(TermChar), _caseIgnore(caseIgnore)
{
    if(_serial) _serial->attach(callback(this, &Transmission::serial_event));
    _evenThread = new Thread(osPriorityNormal, OS_STACK_SIZE);
    _evenThread->start(callback(&_queue, &EventQueue::dispatch_forever));
}

Transmission::Transmission(
    #if MBED_MAJOR_VERSION > 5
    UnbufferedSerial    *serial,
    #else
    Serial              *serial,
    #endif
    EthernetInterface   *eth,
    string              (*processing)(string),
    void                (*ethup)(void),
    bool                TermChar,
    bool                caseIgnore)
    :_serial(serial), _eth(eth), _processing(processing), _ethup(ethup), _TermChar(TermChar), _caseIgnore(caseIgnore)
{
    if(_serial) _serial->attach(callback(this, &Transmission::serial_event));
    _evenThread = new Thread(osPriorityNormal, OS_STACK_SIZE);
    _evenThread->start(callback(&_queue, &EventQueue::dispatch_forever));
}

Transmission::Transmission(
    #if MBED_MAJOR_VERSION > 5
    UnbufferedSerial    *serial,
    #else
    Serial              *serial,
    #endif
    string              (*processing)(string),
    bool                TermChar,
    bool                caseIgnore)
    :_serial(serial), _processing(processing), _TermChar(TermChar), _caseIgnore(caseIgnore)
{
    if(_serial) _serial->attach(callback(this, &Transmission::serial_event));
    _evenThread = new Thread(osPriorityNormal, OS_STACK_SIZE);
    _evenThread->start(callback(&_queue, &EventQueue::dispatch_forever));
}

Transmission::Transmission(
    USBCDC              *usb,
    EthernetInterface   *eth,
    string              (*processing)(string),
    void                (*ethup)(void),
    bool                TermChar,
    bool                caseIgnore)
    :_usb(usb), _eth(eth), _processing(processing), _ethup(ethup), _TermChar(TermChar), _caseIgnore(caseIgnore)
{
    _evenThread = new Thread(osPriorityNormal, OS_STACK_SIZE);
    _evenThread->start(callback(&_queue, &EventQueue::dispatch_forever));
}

Transmission::Transmission(
    EthernetInterface   *eth,
    string              (*processing)(string),
    void                (*ethup)(void),
    bool                TermChar,
    bool                caseIgnore)
    :_eth(eth), _processing(processing), _ethup(ethup), _TermChar(TermChar), _caseIgnore(caseIgnore)
{
    _evenThread = new Thread(osPriorityNormal, OS_STACK_SIZE);
    _evenThread->start(callback(&_queue, &EventQueue::dispatch_forever));
}
    
Transmission::Transmission(
    USBCDC              *usb,
    string              (*processing)(string),
    bool                TermChar,
    bool                caseIgnore)
    :_usb(usb), _processing(processing), _TermChar(TermChar), _caseIgnore(caseIgnore)
{}

string Transmission::ip(string ip)
{
    if(!_eth) return "0.0.0.0:0";
    SocketAddress socket;
    _eth->get_ip_address(&socket);
    string address((string)(socket.get_ip_address()?socket.get_ip_address():"0.0.0.0") + ":" + to_string(message.PORT));
    if(ip == address)
    {
        _eth->get_netmask(&socket);
        address += " " + (string)(socket.get_ip_address()?socket.get_ip_address():"0.0.0.0");
        _eth->get_gateway(&socket);
        address += " " + (string)(socket.get_ip_address()?socket.get_ip_address():"0.0.0.0");
    }
    return address;
}

string Transmission::ip(const bool set, const char* ip, const uint16_t port, const char* mask, const char* gateway, const uint16_t timeout)
{
    if(!_eth) return "00:00:00:00:00:00";
    if(!message.CONNECT)
    {
        eth_error("Ethernet_blocking", _eth->set_blocking(false));
        eth_error("Ethernet_dhcp", _eth->set_dhcp(true));
        eth_error("Ethernet_connect", _eth->connect());
        eth_error("Ethernet_disconnect", _eth->disconnect());
    }
    if(message.SET && set)
    {
        if(message.PORT != port)
        {
            message.CONNECT = false;
            _serverTCP.sigio(NULL);
            eth_error("serverTCP_close", _serverTCP.close());
        }
        eth_error("Ethernet_disconnect", _eth->disconnect());
    }
    message.SET = set;
    message.IP = ip;
    message.PORT = port;
    message.MASK = mask;
    message.GATEWAY = gateway;
    message.TIMEOUT = timeout;
    message.DHCP = message.IP.empty();
    eth_connect();
    string MAC(_eth->get_mac_address()?_eth->get_mac_address():"00:00:00:00:00:00");
    for(char &c : MAC) if((c >= 'a') && (c <= 'z')) c += 'A'-'a';
    return MAC;
}

string Transmission::client(void)
{
    if(message.status == BLUE_CLIENT)
    {
        SocketAddress peer;
        _clientTCP->getpeername(&peer);
        string speer(peer.get_ip_address());
        return speer + ":" + to_string(peer.get_port());
    }
    return "0.0.0.0:0";
}

bool Transmission::eth_connect(void)
{
    if(!_eth) return false;
    if(message.SET)
    {
        switch(_eth->get_connection_status())
        {
            case NSAPI_STATUS_DISCONNECTED:
                eth_error("Ethernet_blocking", _eth->set_blocking(false));
                eth_error("Ethernet_dhcp", _eth->set_dhcp(message.DHCP));
                if(!message.DHCP) eth_error("Ethernet_static", _eth->set_network(SocketAddress(message.IP.c_str()), SocketAddress(message.MASK.c_str()), SocketAddress(message.GATEWAY.c_str())));
                _eth->attach(callback(this, &Transmission::eth_event));
                eth_error("Ethernet_connect", _eth->connect()); break;
            case NSAPI_STATUS_GLOBAL_UP: return message.CONNECT;break;
            default:                                            break;
        }
    }
    else if(_eth->get_connection_status() != NSAPI_STATUS_DISCONNECTED) eth_error("Ethernet_disconnect", _eth->disconnect());
    return false;
}

void Transmission::eth_event(nsapi_event_t status, intptr_t param)
{
    eth_status("Ethernet_event", param);
    switch(param)
    {
        case NSAPI_STATUS_DISCONNECTED: message.status = RED_DISCONNECTED;
                                        break;
        case NSAPI_STATUS_CONNECTING:if(message.status == BLUE_CLIENT) eth_error("clientTCP_disconnect", _clientTCP->close());
                                        message.status = YELLOW_CONNECTING;
                                        break;
        case NSAPI_STATUS_GLOBAL_UP:    message.status = GREEN_GLOBAL_UP;
                                        if(!message.CONNECT) serverTCP_connect(); else serverTCP_accept();
                                        break;
        default:                        break;
    }
}

bool Transmission::serverTCP_connect(void)
{
    if(!message.CONNECT)
        if(eth_error("serverTCP_open", _serverTCP.open(_eth)) == NSAPI_ERROR_OK)
            if(eth_error("serverTCP_bind", _serverTCP.bind(message.PORT)) == NSAPI_ERROR_OK)
                if(eth_error("serverTCP_listen", _serverTCP.listen()) == NSAPI_ERROR_OK)
                {
                    _serverTCP.set_blocking(false);
                    _serverTCP.sigio(callback(this, &Transmission::serverTCP_accept));
                    message.CONNECT = true;
                    if(_ethup) _queue.call(_ethup);
                }
    return message.CONNECT;
}

void Transmission::serverTCP_accept(void)
{
    if(message.status == GREEN_GLOBAL_UP)
    {
        nsapi_error_t ack = NSAPI_ERROR_WOULD_BLOCK;
        message.status = MAGENTA_ACCEPT;
        _clientTCP = _serverTCP.accept(&ack);
        switch(ack)
        {
            case NSAPI_ERROR_OK:
                _clientTCP->set_timeout(message.TIMEOUT);   // config client bloquante avec timeout sinon limite de transmission a 1072 octets
                message.status = BLUE_CLIENT;
            break;
            case NSAPI_ERROR_NO_CONNECTION:
                eth_state();
                serverTCP_accept();
            break;
            default:
                eth_state();
                if(ack < NSAPI_ERROR_WOULD_BLOCK) eth_error("serverTCP_accept", ack);
            break;
        }
    }
}

void Transmission::eth_state(void)
{
    switch(_eth->get_connection_status())
    {
        case NSAPI_STATUS_DISCONNECTED: message.status = RED_DISCONNECTED;  break;
        case NSAPI_STATUS_CONNECTING:   message.status = YELLOW_CONNECTING; break;
        case NSAPI_STATUS_GLOBAL_UP:    message.status = GREEN_GLOBAL_UP;   break;
        default:                                                            break;
    }
}

void Transmission::serial_event(void)
{
    static char buffer[MBED_CONF_LWIP_TCP_MSS] = {0};
    static uint16_t size = 0;
    _serial->read(&buffer[size++], 1);
    if((size == (MBED_CONF_LWIP_TCP_MSS-1)) || ((buffer[size-1] == '\n') && _TermChar))
    {
        buffer[size] = '\0';
        size = 0;
        if(_processing) _queue.call(this, &Transmission::preprocessing, buffer, SERIAL_DELIVERY);
    }
}

Transmission::enum_trans_status Transmission::recv(void)
{
    if(_usb)
    {
        if(_usb->ready())
        {
            static uint8_t buffer[MBED_CONF_LWIP_TCP_MSS] = {0};
            static uint32_t size = 0;
            uint32_t ack = NSAPI_ERROR_OK;
            _usb->receive_nb(&buffer[size], MBED_CONF_LWIP_TCP_MSS-1-size, &ack);   // receive_nb() faster than receive() for small transferts
            if(ack > NSAPI_ERROR_OK)
            {
                size += ack;
                if((size == (MBED_CONF_LWIP_TCP_MSS-1)) || ((buffer[size-1] == '\n') && _TermChar))
                {
                    buffer[size] = '\0';
                    size = 0;
                    if(_processing) preprocessing((char *)buffer, USB_DELIVERY);
                }
            }
        } else _usb->connect();
    }
    if(eth_connect() && (message.status == BLUE_CLIENT))
    {
        char buffer[MBED_CONF_LWIP_TCP_MSS] = {0};  // not static because all TCP data was send in one time
        nsapi_error_t ack = 0, size = 0;
        do{
            ack = _clientTCP->recv(&buffer[size], MBED_CONF_LWIP_TCP_MSS-size);
            if(ack > NSAPI_ERROR_OK) size += ack;
        }while((ack == MBED_CONF_LWIP_TCP_MSS) && (size < MBED_CONF_LWIP_TCP_MSS));   // MBED_CONF_LWIP_TCP_MSS == 536 by default
        if(ack < NSAPI_ERROR_WOULD_BLOCK) eth_error("clientTCP_recv", ack);
        if((ack == NSAPI_ERROR_OK) || (ack == NSAPI_ERROR_NO_CONNECTION))
        {
            eth_error("clientTCP_disconnect", _clientTCP->close());
            eth_state();
            serverTCP_accept();
        }
        if(size && _processing) preprocessing(buffer, strstr(buffer, " HTTP/")?HTTP_DELIVERY:TCP_DELIVERY);
    }
    else if(!_usb) for(int i = 0; ((i < 10) && (message.status != BLUE_CLIENT)); i++) ThisThread::sleep_for(10ms);
    return message.status;
}

void Transmission::preprocessing(char *buffer, const enum_trans_delivery delivery)
{
    for(int i = 0; (buffer[i] != 0) && (i < MBED_CONF_LWIP_TCP_MSS); i++)
    {
        if(_caseIgnore && (buffer[i] >= 'a') && (buffer[i] <= 'z')) buffer[i] += 'A'-'a';
        if((delivery != HTTP_DELIVERY) && (buffer[i] == ';'))       buffer[i] = '\n';
    }
    if(delivery == HTTP_DELIVERY) send(_processing(buffer), HTTP_DELIVERY);
    else if(buffer[0] != 0)
    {
        string cmd, ssend;
        istringstream srecv(buffer);
        while(getline(srecv, cmd, '\n'))
        {
            cmd = _processing(cmd += '\n');
            ssend += ((ssend.size() && cmd.size())?" ":"")+cmd;
        }
        if(ssend.size())
        {
            if(_TermChar) ssend += "\n";
            send(ssend, delivery);
        }
    }
}

nsapi_error_t Transmission::send(const string& ssend, const enum_trans_delivery& delivery)
{
    if(ssend.empty()) return 0;
    uint32_t ack = 0;
    if(_usb && ((delivery == USB_DELIVERY) || (delivery == ANY_DELIVERY)))
    {
        if(_usb->ready()) ack = _usb->send((uint8_t*)ssend.c_str(), ssend.size()); // send() is blocking (No ISR) but send_nb() only for data size <= CDC_MAX_PACKET_SIZE
        else _usb->connect();
    }
    if(_serial && ((delivery == SERIAL_DELIVERY) || (delivery == ANY_DELIVERY)))
        ack = _serial->write(ssend.c_str(), ssend.length());
    if(_eth && ((delivery == TCP_DELIVERY) || (delivery == HTTP_DELIVERY) || (delivery == ANY_DELIVERY)))
    {
        if(message.status == BLUE_CLIENT)
        {
            eth_error("clientTCP_send", ack = _clientTCP->send(ssend.c_str(), ssend.size()));
            if(delivery == HTTP_DELIVERY)
            {
                eth_error("clientTCP_disconnect", _clientTCP->close());
                eth_state();
                serverTCP_accept();
            }
        }
    }
    return ack;
}

string Transmission::get(const string& ssend, const string& server, const int& port)
{
    char buffer[256] = {0};
    if(!server.empty())
    {
        TCPSocket clientTCP;
        clientTCP.set_timeout(message.TIMEOUT);
        if(eth_error("clientTCP_open", clientTCP.open(_eth)) == NSAPI_ERROR_OK)
        {
            if(eth_error("clientTCP_connect", clientTCP.connect(SocketAddress(server.c_str(), port))) == NSAPI_ERROR_OK)
            {
                eth_error("clientTCP_send", clientTCP.send(ssend.c_str(), ssend.size()));
                eth_error("clientTCP_recv", clientTCP.recv(buffer, 256));
            }
        }
        eth_error("clientTCP_close", clientTCP.close());
    }
    return buffer;
}

bool Transmission::exist(string mail, const char* server)
{
    if((!_eth) || (!message.DHCP) || (_eth->get_connection_status() != NSAPI_STATUS_GLOBAL_UP) || mail.empty()) return false;
    string code, smtpParams[] = { "", "HELO Mbed\r\n", "MAIL FROM: <Mbed>\r\n","RCPT TO: <" + mail + ">\r\n", "QUIT\r\n" };
    TCPSocket clientSMTP;
    clientSMTP.set_timeout(message.TIMEOUT);
    if(eth_error("clientSMTP_open", clientSMTP.open(_eth)) == NSAPI_ERROR_OK)
    {
        for(const string& ssend : smtpParams)
        {
            char buffer[64] = {0};
            if(code.empty()) { if(eth_error("clientSMTP_connect", clientSMTP.connect(SocketAddress(server, 25))) < NSAPI_ERROR_OK)  break; }
            else if(eth_error("clientSMTP_send", clientSMTP.send(ssend.c_str(), ssend.size())) < NSAPI_ERROR_OK)                    break;
            if(eth_error("clientSMTP_recv", clientSMTP.recv(buffer, 64)) < NSAPI_ERROR_OK)                                          break;
            buffer[3] = 0;
            code += buffer;
            if(ssend == "QUIT\r\n") break;
        }
        eth_error("clientSMTP_close", clientSMTP.close());
    }
    return code == "220250250250221";
}

bool Transmission::smtp(string mail, string from, string subject, string data, const char* server)
{
    if((!_eth) || (!message.DHCP) || (_eth->get_connection_status() != NSAPI_STATUS_GLOBAL_UP) || mail.empty()) return false;
    ostringstream mid;
    mid << std::hex << std::uppercase << time(NULL);
    for(char &c : mail) mid << ((uint32_t)c);
    string code, smtpParams[] = { "", "HELO MBED-" + from + "\r\n", "MAIL FROM:<Mbed." + from + "@UNIVERSITE-PARIS-SACLAY.FR>\r\n", "RCPT TO:<" + mail + ">\r\n", "DATA\r\n", 
                                "From:\"Mbed " + from + "\" <Mbed." + from + "@UNIVERSITE-PARIS-SACLAY.FR>\r\nTo:<" + mail + ">\r\nSubject:" + subject + "\r\nMessage-Id:<" + mid.str() + ">\r\n" + data + "\r\n.\r\n", "QUIT\r\n" };
    TCPSocket clientSMTP;
    clientSMTP.set_timeout(message.TIMEOUT);
    if(eth_error("clientSMTP_open", clientSMTP.open(_eth)) == NSAPI_ERROR_OK)
    {
        for(const string& ssend : smtpParams)
        {
            char buffer[64] = {0};
            if(code.empty()) { if(eth_error("clientSMTP_connect", clientSMTP.connect(SocketAddress(server, 25))) < NSAPI_ERROR_OK)  break; }
            else if(eth_error("clientSMTP_send", clientSMTP.send(ssend.c_str(), ssend.size())) < NSAPI_ERROR_OK)                    break;
            if(eth_error("clientSMTP_recv", clientSMTP.recv(buffer, 64)) < NSAPI_ERROR_OK)                                          break;
            buffer[3] = 0;
            code += buffer;
            if(ssend == "QUIT\r\n") break;
        }
        eth_error("clientSMTP_close", clientSMTP.close());
        ThisThread::sleep_for(1s);
    }
    if((code == "220250250250354250221") || (code == "220250250250354")) return true;
    #if MBED_MAJOR_VERSION > 5
    _queue.call_in(60s, this, &Transmission::smtp, mail, from, subject, data, server);
    #else
    _queue.call_in(60000, this, &Transmission::smtp, mail, from, subject, data, server);
    #endif
    return false;
}

time_t Transmission::ntp(const char* server)
{
    if(!_eth) return 0;
    if((!message.DHCP) || (_eth->get_connection_status() != NSAPI_STATUS_GLOBAL_UP)) return time(NULL);
    time_t timeStamp = 0;
    UDPSocket clientNTP;
    clientNTP.set_timeout(message.TIMEOUT);
    if(eth_error("clientNTP_open", clientNTP.open(_eth)) == NSAPI_ERROR_OK)
    {
        uint32_t buffer[12] = { 0b11011, 0 };  // VN = 3 & Mode = 3
        SocketAddress aSERVER(server, 123);
        string sSERVER(server);
        if(sSERVER != TRANSMISSION_NTP_SERVER) eth_error("eth_gethostbyname", _eth->gethostbyname(sSERVER.c_str(), &aSERVER));
        if(eth_error("clientNTP_send", clientNTP.sendto(aSERVER, (void*)buffer, sizeof(buffer))) > NSAPI_ERROR_OK)
        {
            if(eth_error("clientNTP_recv", clientNTP.recvfrom(NULL, (void*)buffer, sizeof(buffer))) > NSAPI_ERROR_OK)
            {
                timeStamp = ((buffer[10] & 0xFF) << 24) | ((buffer[10] & 0xFF00) << 8) | ((buffer[10] & 0xFF0000UL) >> 8) | ((buffer[10] & 0xFF000000UL) >> 24);
                timeStamp -= 2208985200U;   // 01/01/1970 Europe
                struct tm * tmTimeStamp = localtime(&timeStamp);
                if (((tmTimeStamp->tm_mon > 2) && (tmTimeStamp->tm_mon < 10)) || ((tmTimeStamp->tm_mon == 2) && ((tmTimeStamp->tm_mday - tmTimeStamp->tm_wday) > 24)) || ((tmTimeStamp->tm_mon == 9) && ((tmTimeStamp->tm_mday - tmTimeStamp->tm_wday) < 25)))
                    timeStamp += 3600;  // DST starts last Sunday of March; 2am (1am UTC), DST ends last Sunday of october; 3am (2am UTC)    
            }
        }
        eth_error("clientNTP_close", clientNTP.close());
    }
    return timeStamp;
}

intptr_t Transmission::eth_status(const string& source, const intptr_t& code)
{
    #ifndef NDEBUG
    string message("\n" + source + "[" + to_string(code) + "] ");
    switch(code)
    {
        case NSAPI_STATUS_LOCAL_UP:             message += "NSAPI_STATUS_LOCAL_UP          < local IP address set >";       break;
        case NSAPI_STATUS_GLOBAL_UP:            message += "NSAPI_STATUS_GLOBAL_UP         < global IP address set >";      break;
        case NSAPI_STATUS_DISCONNECTED:         message += "NSAPI_STATUS_DISCONNECTED      < no connection to network >";   break;
        case NSAPI_STATUS_CONNECTING:           message += "NSAPI_STATUS_CONNECTING        < connecting to network >";      break;
        case NSAPI_STATUS_ERROR_UNSUPPORTED:    message += "NSAPI_STATUS_ERROR_UNSUPPORTED < unsupported functionality >";  break;
    }
    _serial->write(message.c_str(), message.size());
    #endif
    return code;
}

nsapi_error_t Transmission::eth_error(const string& source, const nsapi_error_t& code)
{
    #ifndef NDEBUG
    string message("\n" + source + "[" + to_string(code) + "] ");
    switch(code)
    {
        case NSAPI_ERROR_OK:                    message += "NSAPI_ERROR_OK                 < no error >";                                           break;
        case NSAPI_ERROR_WOULD_BLOCK:           message += "NSAPI_ERROR_WOULD_BLOCK        < no data is not available but call is non-blocking >";  break;
        case NSAPI_ERROR_UNSUPPORTED:           message += "NSAPI_ERROR_UNSUPPORTED        < unsupported functionality >";                          break;
        case NSAPI_ERROR_PARAMETER:             message += "NSAPI_ERROR_PARAMETER          < invalid configuration >";                              break;
        case NSAPI_ERROR_NO_CONNECTION:         message += "NSAPI_ERROR_NO_CONNECTION      < not connected to a network >";                         break;
        case NSAPI_ERROR_NO_SOCKET:             message += "NSAPI_ERROR_NO_SOCKET          < socket not available for use >";                       break;
        case NSAPI_ERROR_NO_ADDRESS:            message += "NSAPI_ERROR_NO_ADDRESS         < IP address is not known >";                            break;
        case NSAPI_ERROR_NO_MEMORY:             message += "NSAPI_ERROR_NO_MEMORY          < memory resource not available >";                      break;
        case NSAPI_ERROR_NO_SSID:               message += "NSAPI_ERROR_NO_SSID            < ssid not found >";                                     break;
        case NSAPI_ERROR_DNS_FAILURE:           message += "NSAPI_ERROR_DNS_FAILURE        < DNS failed to complete successfully >";                break;
        case NSAPI_ERROR_DHCP_FAILURE:          message += "NSAPI_ERROR_DHCP_FAILURE       < DHCP failed to complete successfully >";               break;
        case NSAPI_ERROR_AUTH_FAILURE:          message += "NSAPI_ERROR_AUTH_FAILURE       < connection to access point failed >";                  break;
        case NSAPI_ERROR_DEVICE_ERROR:          message += "NSAPI_ERROR_DEVICE_ERROR       < failure interfacing with the network processor >";     break;
        case NSAPI_ERROR_IN_PROGRESS:           message += "NSAPI_ERROR_IN_PROGRESS        < operation (eg connect) in progress >";                 break;
        case NSAPI_ERROR_ALREADY:               message += "NSAPI_ERROR_ALREADY            < operation (eg connect) already in progress >";         break;
        case NSAPI_ERROR_IS_CONNECTED:          message += "NSAPI_ERROR_IS_CONNECTED       < socket is already connected >";                        break;
        case NSAPI_ERROR_CONNECTION_LOST:       message += "NSAPI_ERROR_CONNECTION_LOST    < connection lost >";                                    break;
        case NSAPI_ERROR_CONNECTION_TIMEOUT:    message += "NSAPI_ERROR_CONNECTION_TIMEOUT < connection timed out >";                               break;
        case NSAPI_ERROR_ADDRESS_IN_USE:        message += "NSAPI_ERROR_ADDRESS_IN_USE     < Address already in use >";                             break;
        case NSAPI_ERROR_TIMEOUT:               message += "NSAPI_ERROR_TIMEOUT            < operation timed out >";                                break;
        case NSAPI_ERROR_BUSY:                  message += "NSAPI_ERROR_BUSY               < device is busy and cannot accept new operation >";     break;
        default:                                message += "NSAPI_ERROR                    < unknow code >";                                        break;
    }
    if(code < NSAPI_ERROR_OK) _serial->write(message.c_str(), message.size());
    #endif
    return code;
}
