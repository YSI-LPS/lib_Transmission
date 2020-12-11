#include "lib_Transmission.h"

#if MBED_MAJOR_VERSION > 5
Transmission::Transmission(UnbufferedSerial *serial, EthernetInterface *eth, void(*init)(void), void(*processing)(string, Transmission::enum_trans_to))
{
    _queueThread.start(callback(&_queue, &EventQueue::dispatch_forever));
    _serial = serial;
    _eth = eth;
    _init = init;
    _processing = processing;
}
#else
Transmission::Transmission(Serial *serial, EthernetInterface *eth, void(*init)(void), void(*processing)(string, Transmission::enum_trans_to))
{
    _queueThread.start(callback(&_queue, &EventQueue::dispatch_forever));
    _serial = serial;
    _eth = eth;
    _init = init;
    _processing = processing;
}
#endif

string Transmission::set(bool SET, const char* IP, uint16_t PORT)
{
    if(_eth == NULL) return "00:00:00:00:00:00";
    if(message.SET && SET)
    {
        if(message.PORT != PORT)
        {
            message.CONNECT = false;
            _serverTCP.sigio(NULL);
            eth_error("serverTCP_close", _serverTCP.close());
        }
        eth_error("Ethernet_disconnect", _eth->disconnect());
    }
    message.SET = SET;
    message.IP = IP;
    message.PORT = PORT;
    message.DHCP = message.IP.empty();
    eth_connect();
    return _eth->get_mac_address();
}

string Transmission::get(void)
{
    if(_eth == NULL) return "0.0.0.0";
    SocketAddress ip;
    _eth->get_ip_address(&ip);
    string address(ip.get_ip_address()?ip.get_ip_address():"0.0.0.0");
    address += ":" + to_string(message.PORT);
    return address;
}

bool Transmission::eth_connect(void)
{
    if(_eth == NULL) return false;
    if(message.SET)
    {
        switch(_eth->get_connection_status())
        {
            case NSAPI_STATUS_DISCONNECTED:
                if(message.status == RED_DISCONNECTED)
                {
                    eth_error("Ethernet_blocking", _eth->set_blocking(false));
                    eth_error("Ethernet_dhcp", _eth->set_dhcp(message.DHCP));
                    if(!message.DHCP) eth_error("Ethernet_static", _eth->set_network(SocketAddress(message.IP.c_str()), SocketAddress("255.255.255.0"), SocketAddress("192.168.1.1")));
                    eth_error("Ethernet_connect", _eth->connect());
                }
            break;
            case NSAPI_STATUS_CONNECTING:
                if(message.status == RED_DISCONNECTED)
                {
                    eth_status("Ethernet_connect", NSAPI_STATUS_CONNECTING);
                    message.status = YELLOW_CONNECTING;
                    _eth->attach(callback(this, &Transmission::eth_event));
                }
            break;
            case NSAPI_STATUS_GLOBAL_UP:    return message.CONNECT; break;
            default:                                                break;
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
        case NSAPI_STATUS_DISCONNECTED: message.status = RED_DISCONNECTED;  break;
        case NSAPI_STATUS_CONNECTING:if(message.status == BLUE_CLIENT) eth_error("clientTCP_disconnect", _clientTCP->close());
                                        message.status = YELLOW_CONNECTING; break;
        case NSAPI_STATUS_GLOBAL_UP:    message.status = GREEN_GLOBAL_UP;
                                        if(message.CONNECT)
                                             serverTCP_event();
                                        else serverTCP_connect();           break;
        default:                                                            break;
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
                    _serverTCP.sigio(callback(this, &Transmission::serverTCP_event));
                    message.CONNECT = true;
                    if(_init != NULL) _queue.call(_init);
                }
    return message.CONNECT;
}

void Transmission::serverTCP_event(void)
{
    _queue.call(this, &Transmission::serverTCP_accept);
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
                _clientTCP->set_timeout(REQUEST_TIMEOUT);
                message.status = BLUE_CLIENT;
            break;
            case NSAPI_ERROR_NO_CONNECTION:
                eth_state();
                serverTCP_event();
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
        default:                                                                            break;
    }
}

Transmission::enum_trans_status Transmission::recv(void)
{
    if(eth_connect())
    {
        char buffer[1072] = {0};
        nsapi_error_t ack = NSAPI_ERROR_WOULD_BLOCK, size = 0;
        if(message.status == BLUE_CLIENT) while((ack = _clientTCP->recv(&buffer[size], 1072-size)) > NSAPI_ERROR_OK) size += ack;
        if(ack < NSAPI_ERROR_WOULD_BLOCK) eth_error("clientTCP_recv", ack);
        if(!size) message.BREAK = ((ack == NSAPI_ERROR_OK) || (ack == NSAPI_ERROR_NO_CONNECTION));
        for(int i = 0; i < size; i++) if(buffer[i] == '\n') buffer[i] = ';';
        if(_processing != NULL) _processing(buffer, TCP);
    }
    if(_serial != NULL)
    {
        if(_serial->readable())
        {
            char caractere;
            #if MBED_MAJOR_VERSION > 5
            _serial->read(&caractere, 1);
            #else
            caractere = _serial->getc();
            #endif
            if((caractere == '\n') || (caractere == '\r'))
            {
                if(_processing != NULL) _processing(message.serial, SERIAL);
                message.serial.clear();
            }
            else if((caractere > 31) && (caractere < 127)) message.serial += caractere;
        }
    }
    return message.status;
}

nsapi_error_t Transmission::send(const string& buff, const enum_trans_to& type)
{
    nsapi_error_t ack = NSAPI_ERROR_WOULD_BLOCK;
    string ssend(buff+"\n");
    if((type != TCP) && (type != HTTP) && !buff.empty())
    {
        if(_serial == NULL) return NSAPI_ERROR_NO_CONNECTION;
        #if MBED_MAJOR_VERSION > 5
        ack = _serial->write(ssend.c_str(), ssend.length());
        #else
        ack = _serial->printf("%s", ssend.c_str());
        #endif
    }
    if(type != SERIAL)
    {
        if(_eth == NULL) return NSAPI_ERROR_NO_CONNECTION;
        if(!message.BREAK && !buff.empty() && (message.status == BLUE_CLIENT))
            eth_error("clientTCP_send", ack = _clientTCP->send(ssend.c_str(), ssend.size()));
        if(message.BREAK || (type == HTTP))
        {
            message.BREAK = false;
            eth_error("clientTCP_disconnect", _clientTCP->close());
            eth_state();
            serverTCP_event();
        }
    }
    return ack;
}

bool Transmission::smtp(const char* MAIL, const char* FROM, const char* SUBJECT, const char* DATA)
{
    if(_eth == NULL) return false;
    if((!message.DHCP) || (_eth->get_connection_status() != NSAPI_STATUS_GLOBAL_UP)) return false;
    TCPSocket clientSMTP;
    clientSMTP.set_timeout(REQUEST_TIMEOUT*20);
    const string sMAIL(MAIL), sFROM(FROM), sSUBJECT(SUBJECT), sDATA(DATA);
    const string smtpParams[][7] = {{ "", "HELO Mbed " + sFROM + "\r\n", "MAIL FROM: <Mbed." + sFROM + "@UNIVERSITE-PARIS-SACLAY.FR>\r\n", "RCPT TO: <" + sMAIL + ">\r\n", "DATA\r\n", "From: \"Mbed " + sFROM + "\" <Mbed." + sFROM + "@U-PSUD.FR>\r\nTo: \"DESTINATAIRE\" <" + sMAIL + ">\r\nSubject:" + sSUBJECT + "\r\n" + sDATA + "\r\n.\r\n", "QUIT\r\n" },
                                    { "", "HELO Mbed\r\n", "MAIL FROM: <Mbed>\r\n","RCPT TO: <" + sMAIL + ">\r\n", "QUIT\r\n" }};
    string code;
    if(eth_error("clientSMTP_open", clientSMTP.open(_eth)) == NSAPI_ERROR_OK)
    {
        for(const string ssend : smtpParams[(sFROM.empty())?1:0])
        {
            char buffer[256] = {0};
            if(code.empty()) { if(eth_error("clientSMTP_connect", clientSMTP.connect(SocketAddress(SMTP_SERVER, 25))) < NSAPI_ERROR_OK) break; }
            else if(eth_error("clientSMTP_send", clientSMTP.send(ssend.c_str(), ssend.size())) < NSAPI_ERROR_OK)                        break;
            if(eth_error("clientSMTP_recv", clientSMTP.recv(buffer, 256)) < NSAPI_ERROR_OK)                                             break;
            buffer[3] = 0;
            code += buffer;
            if(ssend == "QUIT\r\n") break;
        }
        eth_error("clientSMTP_close", clientSMTP.close());
    }
    if(sFROM.empty()) return code == "220250250250221";
    #if MBED_MAJOR_VERSION > 5
    else if(code != "220250250250354250221") _queue.call_in(60s, this, &Transmission::smtp, MAIL, FROM, SUBJECT, DATA);
    #else
    else if(code != "220250250250354250221") _queue.call_in(60000, this, &Transmission::smtp, MAIL, FROM, SUBJECT, DATA);
    #endif
    return code == "220250250250354250221";
}

time_t Transmission::ntp(const char* ADDRESS)
{
    if(_eth == NULL) return 0;
    if((!message.DHCP) || (_eth->get_connection_status() != NSAPI_STATUS_GLOBAL_UP)) return 0;
    time_t timeStamp = 0;
    UDPSocket clientNTP;
    clientNTP.set_timeout(REQUEST_TIMEOUT*20);
    if(eth_error("clientNTP_open", clientNTP.open(_eth)) == NSAPI_ERROR_OK)
    {
        string sADDRESS(ADDRESS);
        uint32_t buffer[12] = { 0b11011, 0 };  // VN = 3 & Mode = 3
        SocketAddress address(NTP_SERVER, 123);
        if(!sADDRESS.empty()) eth_error("eth_gethostbyname", _eth->gethostbyname(sADDRESS.c_str(), &address));
        if(eth_error("clientNTP_send", clientNTP.sendto(address, (void*)buffer, sizeof(buffer))) > NSAPI_ERROR_OK)
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
    stringstream message;
    message << "\n" << source << "[" << code;
    switch(code)
    {
        case NSAPI_STATUS_LOCAL_UP:             message << "] NSAPI_STATUS_LOCAL_UP          < local IP address set >";     break;
        case NSAPI_STATUS_GLOBAL_UP:            message << "] NSAPI_STATUS_GLOBAL_UP         < global IP address set >";    break;
        case NSAPI_STATUS_DISCONNECTED:         message << "] NSAPI_STATUS_DISCONNECTED      < no connection to network >"; break;
        case NSAPI_STATUS_CONNECTING:           message << "] NSAPI_STATUS_CONNECTING        < connecting to network >";    break;
        case NSAPI_STATUS_ERROR_UNSUPPORTED:    message << "] NSAPI_STATUS_ERROR_UNSUPPORTED < unsupported functionality >";break;
    }
    #ifndef NDEBUG
    _serial->write(message.str().c_str(), message.str().size());
    #endif
    return code;
}

nsapi_error_t Transmission::eth_error(const string& source, const nsapi_error_t& code)
{
    stringstream message;
    message << "\n" << source << "[" << code;
    switch(code)
    {
        case NSAPI_ERROR_OK:                    message << "] NSAPI_ERROR_OK                 < no error >";                                         break;
        case NSAPI_ERROR_WOULD_BLOCK:           message << "] NSAPI_ERROR_WOULD_BLOCK        < no data is not available but call is non-blocking >";break;
        case NSAPI_ERROR_UNSUPPORTED:           message << "] NSAPI_ERROR_UNSUPPORTED        < unsupported functionality >";                        break;
        case NSAPI_ERROR_PARAMETER:             message << "] NSAPI_ERROR_PARAMETER          < invalid configuration >";                            break;
        case NSAPI_ERROR_NO_CONNECTION:         message << "] NSAPI_ERROR_NO_CONNECTION      < not connected to a network >";                       break;
        case NSAPI_ERROR_NO_SOCKET:             message << "] NSAPI_ERROR_NO_SOCKET          < socket not available for use >";                     break;
        case NSAPI_ERROR_NO_ADDRESS:            message << "] NSAPI_ERROR_NO_ADDRESS         < IP address is not known >";                          break;
        case NSAPI_ERROR_NO_MEMORY:             message << "] NSAPI_ERROR_NO_MEMORY          < memory resource not available >";                    break;
        case NSAPI_ERROR_NO_SSID:               message << "] NSAPI_ERROR_NO_SSID            < ssid not found >";                                   break;
        case NSAPI_ERROR_DNS_FAILURE:           message << "] NSAPI_ERROR_DNS_FAILURE        < DNS failed to complete successfully >";              break;
        case NSAPI_ERROR_DHCP_FAILURE:          message << "] NSAPI_ERROR_DHCP_FAILURE       < DHCP failed to complete successfully >";             break;
        case NSAPI_ERROR_AUTH_FAILURE:          message << "] NSAPI_ERROR_AUTH_FAILURE       < connection to access point failed >";                break;
        case NSAPI_ERROR_DEVICE_ERROR:          message << "] NSAPI_ERROR_DEVICE_ERROR       < failure interfacing with the network processor >";   break;
        case NSAPI_ERROR_IN_PROGRESS:           message << "] NSAPI_ERROR_IN_PROGRESS        < operation (eg connect) in progress >";               break;
        case NSAPI_ERROR_ALREADY:               message << "] NSAPI_ERROR_ALREADY            < operation (eg connect) already in progress >";       break;
        case NSAPI_ERROR_IS_CONNECTED:          message << "] NSAPI_ERROR_IS_CONNECTED       < socket is already connected >";                      break;
        case NSAPI_ERROR_CONNECTION_LOST:       message << "] NSAPI_ERROR_CONNECTION_LOST    < connection lost >";                                  break;
        case NSAPI_ERROR_CONNECTION_TIMEOUT:    message << "] NSAPI_ERROR_CONNECTION_TIMEOUT < connection timed out >";                             break;
        case NSAPI_ERROR_ADDRESS_IN_USE:        message << "] NSAPI_ERROR_ADDRESS_IN_USE     < Address already in use >";                           break;
        case NSAPI_ERROR_TIMEOUT:               message << "] NSAPI_ERROR_TIMEOUT            < operation timed out >";                              break;
        case NSAPI_ERROR_BUSY:                  message << "] NSAPI_ERROR_BUSY               < device is busy and cannot accept new operation >";   break;
        default:                                message << "] NSAPI_ERROR                    < unknow code >";                                      break;
    }
    #ifndef NDEBUG
    if(code < NSAPI_ERROR_OK) _serial->write(message.str().c_str(), message.str().size());
    #endif
    return code;
}