#include "lib_Transmission.h"

Transmission::Transmission(UnbufferedSerial *serial, EthernetInterface *eth, EventQueue *queue, void(*com_init)(void), void(*com_processing)(string, const enumCOM&))
{
    _serial = serial;
    _eth = eth;
    _queue = queue;
    fn_com_init = com_init;
    fn_com_processing = com_processing;
}

bool Transmission::eth_connect(void)
{
    if(message.TCP)
    {
        switch(_eth->get_connection_status())
        {
            case NSAPI_STATUS_DISCONNECTED:
                if(statusCOLOR == RED_DISCONNECTED)
                {
                    eth_error("Ethernet_blocking", _eth->set_blocking(false));
                    eth_error("Ethernet_dhcp", _eth->set_dhcp(TCP_SET_DHCP));
                    if(!TCP_SET_DHCP) eth_error("Ethernet_static", _eth->set_network(SocketAddress(TCP_IP), SocketAddress(TCP_NETMASK), SocketAddress(TCP_GATEWAY)));
                    //{ SocketAddress ip(TCP_IP); SocketAddress mask(TCP_NETMASK); SocketAddress gateway(TCP_GATEWAY); eth_error("Ethernet_static", eth_error("Ethernet_static", _eth->set_network(ip, mask, gateway))); }
                    eth_error("Ethernet_connect", _eth->connect());
                }
            break;
            case NSAPI_STATUS_CONNECTING:
                if(statusCOLOR == RED_DISCONNECTED)
                {
                    eth_status("Ethernet_connect", NSAPI_STATUS_CONNECTING);
                    statusCOLOR = YELLOW_CONNECTING;
                    _eth->attach(callback(this, &Transmission::eth_event));
                }
            break;
            case NSAPI_STATUS_GLOBAL_UP: return serverTCP_connect(); break;
            default: break;
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
        case NSAPI_STATUS_CONNECTING:   statusCOLOR = YELLOW_CONNECTING;  break;
        case NSAPI_STATUS_DISCONNECTED: statusCOLOR = RED_DISCONNECTED;   break;
        case NSAPI_STATUS_GLOBAL_UP:    statusCOLOR = GREEN_GLOBAL_UP;    break;
        default:                                                                break;
    }
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
    _serial->write(message.str().c_str(), message.str().size());//debug("%s", message.str().c_str());
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
    if(code < NSAPI_ERROR_OK) _serial->write(message.str().c_str(), message.str().size());//debug("%s", message.str().c_str());
    return code;
}

bool Transmission::serverTCP_connect(void)
{
    static bool serverTCP_connect = false;
    if(!serverTCP_connect)
        if (eth_error("serverTCP_open", serverTCP.open(_eth)) == NSAPI_ERROR_OK)
            if (eth_error("serverTCP_bind", serverTCP.bind(TCP_PORT)) == NSAPI_ERROR_OK)
                if (eth_error("serverTCP_listen", serverTCP.listen()) == NSAPI_ERROR_OK)
                {
                    serverTCP.set_blocking(false);
                    serverTCP.sigio(callback(this, &Transmission::serverTCP_event));
                    serverTCP_connect = true;
                    fn_com_init();
                }
    return serverTCP_connect;
}

void Transmission::serverTCP_event(void)
{
    _queue->call(this, &Transmission::serverTCP_accept);
}

void Transmission::serverTCP_accept(void)
{
    if(statusCOLOR == GREEN_GLOBAL_UP)
    {
        nsapi_error_t ack = NSAPI_ERROR_WOULD_BLOCK;
        statusCOLOR = MAGENTA_ACCEPT;
        clientTCP = serverTCP.accept(&ack);
        switch(ack)
        {
            case NSAPI_ERROR_OK:
                clientTCP->set_timeout(TCP_CLIENT_TIMEOUT);
                statusCOLOR = BLUE_CLIENT;
            break;
            case NSAPI_ERROR_WOULD_BLOCK:
                statusCOLOR = GREEN_GLOBAL_UP;
            break;
            case NSAPI_ERROR_NO_CONNECTION:
                statusCOLOR = GREEN_GLOBAL_UP;
                _queue->call(this, &Transmission::serverTCP_accept);
            break;
            default:
                statusCOLOR = GREEN_GLOBAL_UP;
                eth_error("serverTCP_accept", ack);
            break;
        }
    }
}

nsapi_error_t Transmission::send(const string& buff, const enumCOM& type)
{
    nsapi_error_t ack = NSAPI_ERROR_WOULD_BLOCK;
    string ssend(buff+"\n");
    switch(type)
    {
        case TCP:
            if(message.BREAK)
            {
                message.BREAK = false;
                eth_error("clientTCP_disconnect", clientTCP->close());
                switch(_eth->get_connection_status())
                {
                    case NSAPI_STATUS_DISCONNECTED: statusCOLOR = RED_DISCONNECTED;   break;
                    case NSAPI_STATUS_CONNECTING:   statusCOLOR = YELLOW_CONNECTING;  break;
                    case NSAPI_STATUS_GLOBAL_UP:    statusCOLOR = GREEN_GLOBAL_UP;    break;
                    default:                                                                break;
                }
                _queue->call(this, &Transmission::serverTCP_accept);
            }
            else if(!buff.empty())
            {
                ack = clientTCP->send(ssend.c_str(), ssend.size());
                if(message.HTTP)
                {
                    message.HTTP = false;
                    eth_error("clientTCP_disconnect", clientTCP->close());
                    switch(_eth->get_connection_status())
                    {
                        case NSAPI_STATUS_DISCONNECTED: statusCOLOR = RED_DISCONNECTED;   break;
                        case NSAPI_STATUS_CONNECTING:   statusCOLOR = YELLOW_CONNECTING;  break;
                        case NSAPI_STATUS_GLOBAL_UP:    statusCOLOR = GREEN_GLOBAL_UP;    break;
                        default:                                                                break;
                    }
                    _queue->call(this, &Transmission::serverTCP_accept);
                }
            }
        break;
        case SERIAL:
            if(!buff.empty()) ack = _serial->write(ssend.c_str(), ssend.length());
        break;
    }
    return ack;
}

enumCOLOR Transmission::recv(void)
{
    if(eth_connect())
    {
        char buffer[1024] = {0};
        nsapi_error_t ack = NSAPI_ERROR_WOULD_BLOCK;
        if(statusCOLOR == BLUE_CLIENT)
            if((ack = clientTCP->recv(buffer, 1024)) < NSAPI_ERROR_WOULD_BLOCK)
                eth_error("clientTCP_recv", ack);
        switch(ack)
        {
            case NSAPI_ERROR_NO_CONNECTION:
            case NSAPI_ERROR_OK:
            message.BREAK = true;   break;
            default:            break;
        }
        for(int i = 0; i < ack; i++) if(buffer[i] == '\n') buffer[i] = ';';
        fn_com_processing(message.buffer[TCP] = buffer, TCP);
        message.buffer[TCP].clear();
    }
    if(_serial->readable())
    {
        char caractere;
        _serial->read(&caractere, 1);
        switch(caractere)
        {
            case '\n':
                fn_com_processing(message.buffer[SERIAL], SERIAL);
                message.buffer[SERIAL].clear();
            break;
            default:
                if((caractere > 31) && (caractere < 127)) message.buffer[SERIAL] += caractere;
            break;
        }
    }
    return statusCOLOR;
}

bool Transmission::sendSMTP(const char* MAIL, const char* FROM, const char* SUBJECT, const char* DATA)
{
    TCPSocket clientSMTP;
    clientSMTP.set_timeout(5000);
    string code;
    if(eth_error("clientSMTP_open", clientSMTP.open(_eth)) == NSAPI_ERROR_OK)
    {
        const string sMAIL(MAIL), sFROM(FROM), sSUBJECT(SUBJECT), sDATA(DATA), 
        smtpSend[] = { "", "HELO Mbed " + sFROM + "\r\n", "MAIL FROM: <Mbed." + sFROM + "@U-PSUD.FR>\r\n", "RCPT TO: <" + sMAIL + ">\r\n", "DATA\r\n",
        "From: \"Mbed " + sFROM + "\" <Mbed." + sFROM + "@U-PSUD.FR>\r\nTo: \"DESTINATAIRE\" <" + sMAIL + ">\r\nSubject:" + sSUBJECT + "\r\n" + sDATA + "\r\n.\r\n", "QUIT\r\n" };
        for(const string ssend : smtpSend)
        {
            char buffer[256] = {0};
            if(code.empty()) { if(eth_error("clientSMTP_connect", clientSMTP.connect(SocketAddress(SMTP_SERVER, 25))) < NSAPI_ERROR_OK)  break; }
            else if(eth_error("clientSMTP_send", clientSMTP.send(ssend.c_str(), ssend.size())) < NSAPI_ERROR_OK)                                    break;
            if(eth_error("clientSMTP_recv", clientSMTP.recv(buffer, 256)) < NSAPI_ERROR_OK)                                                         break;
            buffer[3] = 0;
            code += buffer;
        }
        eth_error("clientSMTP_close", clientSMTP.close());
    }
    if(code != "220250250250354250221")
    {
        _queue->call_in(60s, this, &Transmission::sendSMTP, MAIL, FROM, SUBJECT, DATA);
        return false;
    }
    return true;
}

bool Transmission::checkSMTP(const char* MAIL)
{
    TCPSocket clientSMTP;
    clientSMTP.set_timeout(5000);
    string code(MAIL);
    if(eth_error("clientSMTP_open", clientSMTP.open(_eth)) == NSAPI_ERROR_OK)
    {
        const string smtpSend[] = { "", "HELO Mbed\r\n", "MAIL FROM: <Mbed>\r\n","RCPT TO: <" + code + ">\r\n", "QUIT\r\n" };
        code.clear();
        for(const string ssend : smtpSend)
        {
            char buffer[256] = {0};
            if(code.empty()) { if(eth_error("clientSMTP_connect", clientSMTP.connect(SocketAddress(SMTP_SERVER, 25))) < NSAPI_ERROR_OK) break; }
            else if(eth_error("clientSMTP_send", clientSMTP.send(ssend.c_str(), ssend.size())) < NSAPI_ERROR_OK)                        break;
            if(eth_error("clientSMTP_recv", clientSMTP.recv(buffer, 256)) < NSAPI_ERROR_OK)                                             break;
            buffer[3] = 0;
            code += buffer;
        }
        eth_error("clientSMTP_close", clientSMTP.close());
    }
    return code == "220250250250221";
}