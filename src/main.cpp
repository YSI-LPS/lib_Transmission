#include <map>
#include <sstream>
#include "lib_Transmission.h"   // si les NAN ne fonctionne pas aller dans mbed-os/tools/profiles/ puis ARMC6 et dans common ajouter -fhonor-nans
#include "kvstore_global_api.h"

#define SERIAL_BAUDRATE     230400
#define UC_HELIUM_SENSOR    1
#define UC_HELIUM_METER     2
#define PROJECT_COMPILED    UC_HELIUM_SENSOR

#if PROJECT_COMPILED ==     UC_HELIUM_SENSOR
#define MBED_PROJECT        "HeliumSensor"

enum enumHTML       { CONTENT, HEAD, HEAD_META, OPEN_BODY, OPEN_BODY_IFRAME, CLOSE_BODY, FAVICON, ERR404, SMTP };
enum enumPARAM      { MAC, NAME, NETWORK, SPEC };

//  --------------------    INITIALIZE
void        setup(void);
//  --------------------    KVSTORE
string      kv(string, string = "");
void        kv_apply(string, string = "");
//  --------------------    TRANSMISSION
void        ethset(string = "");
void        ethup(void);
string      processing(string);
//  --------------------    STRING
string      stime(const bool& = true, const bool& = false, const time_t = 0);
//  --------------------    HTML
string      html(const enumHTML&);
//  --------------------    HELIUM
void        helium_rising_event(void);
void        helium_falling_event(void);
void        helium_rising_timeout(void);
void        helium_falling_timeout(void);
void        helium_set(void);

struct {
            const string                            idn;
            UnbufferedSerial                        *ss;
            USBCDC                                  *usb;
            EthernetInterface                       *eth;
            Transmission                            *tr;
            Thread                                  *queueThread;
            Thread                                  *setThread;
            EventQueue                              *queue;
            EventQueue                              *set;
            string                                  alerts;
            string                                  pwd;
            string                                  adm;
            string                                  mail[10];
            string                                  param[4];
            int                                     color[8];
            BusOut                                  *leds;
            const Transmission::enum_trans_status   init;
            const Transmission::enum_trans_status   client;
            Transmission::enum_trans_status         state;
            string                                  ip;
            string                                  mask;
            string                                  gateway;
            uint16_t                                port;
            bool                                    dhcp;
} uc = {
            MBED_PROJECT + ((string)" Rev. ") + MBED_STRINGIFY(TARGET_NAME) + " " + __DATE__ + " " + __TIME__ + " MbedOS " + to_string(MBED_VERSION),
            new UnbufferedSerial(USBTX, USBRX, SERIAL_BAUDRATE),
            new USBCDC(false),
            new EthernetInterface,
            new Transmission(uc.ss, uc.usb, uc.eth, &processing, &ethup),
            new Thread,
            new Thread,
            new EventQueue,
            new EventQueue,
            "",
            { 0x2A, 0x2A, 0x55, 0x43 },
            { 0x2A, 0x45, 0x4C, 0x49, 0x4E, 0x53, 0x59, 0x53, 0x49, 0x2A },
            { "YANNIC.SIMON@UNIVERSITE-PARIS-SACLAY.FR", "", "", "", "", "", "", "", "", "" },
            { "", "", "", "" },
            { 7, 6, 3, 2, 5, 1, 4, 0 },
            new BusOut(LED1, LED2, LED3),
            Transmission::WHITE_STATUS,
            Transmission::BLUE_CLIENT,
            Transmission::RED_DISCONNECTED,
            "192.168.1.1",
            "255.255.255.0",
            "192.168.1.0",
            80,
            false
};
struct {
            SocketAddress   *server;
            BusOut          *vcc;
            BusIn           *tag;
            InterruptIn     *meter;
            Timeout         *timeout_rising;
            Timeout         *timeout_falling;
            Timer           *timer_count;
            Timer           *timer_set;
            int             set;
            int             id;
            string          room;
            uint32_t        liters;
            float           flow_minute;
            string          update;
} sensor = {
            new SocketAddress,
            new BusOut(PD_3, PD_4, PD_5, PD_6, PD_7),
            new BusIn(A0, A1, A2, A3, A4),
            new InterruptIn(A5), // IN-Z62
            new Timeout,
            new Timeout,
            new Timer,
            new Timer,
            0,
            0,
            "",
            0,
            0.0F,
            ""
};

int main(void)
{
    setup();
    while(true) uc.leds->write(uc.color[uc.state = uc.tr->recv()]);
}

void setup(void)
{
    uc.leds->write(uc.color[uc.init]);
    ThisThread::sleep_for(1s);
    uc.leds->write(uc.color[uc.state]);
    sensor.vcc->write(0x1F);
    sensor.meter->mode(PullUp);
    sensor.tag->mode(PullDown);
    sensor.id = sensor.tag->read();
    sensor.room = sensor.id?"R0"+to_string(sensor.id):"NONE";
    uc.ip = "192.168.1."+to_string(sensor.id);
    kv_apply("IDN");
    kv_apply("ETH");
    kv_apply("PASSWORD");
    kv_apply("LITERS");
    kv_apply("ROOM");
    uc.queueThread->start(callback(uc.queue, &EventQueue::dispatch_forever));
    uc.setThread->start(callback(uc.set, &EventQueue::dispatch_forever));
    sensor.meter->fall(&helium_falling_event);
    sensor.meter->rise(&helium_rising_event);
}
void ethup(void)
{
    if(uc.dhcp)
    {
        stime(true, true);
        ethset("ETH=DHCP");
        sensor.server->set_ip_address("192.168.142.94"); // uc.eth->gethostbyname("helium-uc-0", sensor.server); doesn't work
    }
    else sensor.server->set_ip_address("192.168.1.0");
    sensor.server->set_port(80);
    uc.set->call(&helium_set);
}

string kv(string kvKey, string kvParam)
{
    if(kvKey.empty()) return "";
    size_t keySize;
    kv_info_t keyInfo;
    char kvValue[256] = {0};
    if((kvKey == "IDN") && !kvParam.empty()) kv_reset("/kv/");
    if(kv_get_info("IDN", &keyInfo) != MBED_SUCCESS) if(kv_reset("/kv/") == MBED_SUCCESS) kv_set("IDN", "", 0, 0);
    if(kv_get_info(kvKey.c_str(), &keyInfo) == MBED_SUCCESS) if(kv_get(kvKey.c_str(), kvValue, 256, &keySize) == MBED_SUCCESS) if(!kvParam.empty()) kv_remove(kvKey.c_str());
    if(!kvParam.empty()) kv_set(kvKey.c_str(), kvParam.c_str(), kvParam.size(), 0);
    return kvValue;
}

void kv_apply(string kvKey, string kvParam)
{
    string kvValue = kv(kvKey);
    enum enumKvSTATUS { CHANGE_KVAL_PVAL, SAVE_NOKVAL_PVAL, LOAD_KVAL_NOPVAL, INIT_NOKVAL_NOPVAL } kvStatus = enumKvSTATUS((1*kvValue.empty()) + (2*kvParam.empty()));
    if((kvKey == "IDN") && (kvValue != uc.idn)) kv(kvKey, uc.idn);
    else if(kvKey == "ETH") processing((kvValue.substr(0, 3) == "ETH")?kvValue:"ETH=DHCP");
    else if(kvKey == "PASSWORD")
    {
        switch(kvStatus)
        {
            case CHANGE_KVAL_PVAL: case SAVE_NOKVAL_PVAL: if(kvValue != kvParam) kv(kvKey, kvValue = kvParam); break;
            case INIT_NOKVAL_NOPVAL: kv(kvKey, kvValue = uc.pwd); break;
            case LOAD_KVAL_NOPVAL: break;
        }
        uc.pwd = kvValue;
    }
    else if(kvKey == "ROOM")
    {
        switch(kvStatus)
        {
            case CHANGE_KVAL_PVAL: case SAVE_NOKVAL_PVAL: if(kvValue != kvParam) kv(kvKey, kvValue = kvParam); break;
            case INIT_NOKVAL_NOPVAL: kv(kvKey, kvValue = sensor.room); break;
            case LOAD_KVAL_NOPVAL: break;
        }
        sensor.room = kvValue;
    }
    else if(kvKey == "LITERS")
    {
        switch(kvStatus)
        {
            case CHANGE_KVAL_PVAL: case SAVE_NOKVAL_PVAL: if(kvValue != kvParam) kv(kvKey, kvValue = kvParam); break;
            case INIT_NOKVAL_NOPVAL: kv(kvKey, kvValue = to_string(sensor.liters)); break;
            case LOAD_KVAL_NOPVAL: break;
        }
        istringstream istr(kvValue);
        istr >> sensor.liters;
    }
}

string stime(const bool& second, const bool& update, const time_t stamp)
{
    char timeLogs[22] = {0};
    time_t ucTimeStamp = stamp?stamp:time(NULL);
    static time_t ucTimeRef = ucTimeStamp;
    if((ucTimeStamp < 1600000000) || ((ucTimeStamp - ucTimeRef) > 86400)) uc.queue->call(&stime, false, true, 0);
    if(update)
    {
        time_t ntpTimeStamp = uc.tr->ntp();
        if(ntpTimeStamp > 0) set_time(ucTimeRef = ucTimeStamp = ntpTimeStamp);
    }
    struct tm * tmTimeStamp = localtime(&ucTimeStamp);
    strftime(timeLogs, 21, second?"%F %T":"%F %R", tmTimeStamp);
    return timeLogs;
}

void ethset(string cmd)
{
    size_t pos;
    if((uc.dhcp = (cmd.find("ETH=IP") == string::npos))) cmd = uc.tr->ip(uc.tr->ip());
    if((pos = cmd.find(":")) != string::npos)
    {
        istringstream istr(cmd.substr(pos+1));
        istr >> uc.port;
        cmd.replace(pos, 1, " ");
    }
    istringstream sget(cmd);
    for(int dot = 0, data = 0; getline(sget, cmd, ' '); dot = 0)
    {
        for(const char c:cmd) if(c == '.') dot++;
        if(dot == 3)
        {
            data++;
            if(cmd == "0.0.0.0") break;
            else
            {
                istringstream istr(cmd);
                switch(data)
                {
                    case 1: istr >> uc.ip;      break;
                    case 2: istr >> uc.mask;    break;
                    case 3: istr >> uc.gateway; break;
                    default:                    break;
                }
            }
        }
    }
}

string html(const enumHTML& PART)
{
    ostringstream ssend;
    ssend << fixed;
    ssend.precision(2);
    switch(PART)
    {
        case CONTENT:               ssend << "Content-Type: text/html; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\n\r\n"; break;
        case HEAD:                  ssend << "<!DOCTYPE html>\r\n<html>\r\n\t<head>\r\n\t\t<title>" << MBED_PROJECT << "</title>\r\n\t</head>"; break;
        case HEAD_META:             ssend << "<!DOCTYPE html>\r\n<html>\r\n\t<head>\r\n\t\t<title>" << MBED_PROJECT << "</title>\r\n\t\t<meta http-equiv=refresh content=10>\r\n\t</head>"; break;
        case OPEN_BODY:             ssend << "\r\n\t<body style=background-color:dimgray>\r\n\t\t<center>\r\n\t\t\t<h1 style=color:white>" << MBED_PROJECT << "</h1>"; break;
        case OPEN_BODY_IFRAME:      ssend << "\r\n\t<body style=background-color:transparency>\r\n\t\t<center>"; break;
        case CLOSE_BODY:            ssend << "\r\n\t\t</center>\r\n\t</body>\r\n</html>"; break;
        case FAVICON: case ERR404:  ssend << "Content-Type: image/svg+xml\r\nAccess-Control-Allow-Origin: *\r\n\r\n<svg width=\"100%\" height=\"100%\" viewBox=\"0 0 100 100\" style=\"background-color:black\" xmlns=\"http://www.w3.org/2000/svg\">\r\n\t<title>" << MBED_PROJECT << "</title>\r\n\t<text x=\"0\" y=\"90\" textLength=\"80\" font-size=\"120\" fill=\"white\" font-weight=\"bold\" font-style=\"italic\" lengthAdjust=\"spacingAndGlyphs\" style=\"font-family:\'Bauhaus 93\';-inkscape-font-specification:\'Bauhaus 93, Normal\'\">" << (PART==FAVICON?"ElI":"404") << "</text>\r\n</svg>"; break;
        case SMTP: break;
    }
    return ssend.str();
}

void helium_falling_event(void)
{
    sensor.timeout_falling->attach(&helium_falling_timeout, 200ms);
    sensor.timeout_rising->attach(&helium_rising_timeout, 60s);
}

void helium_rising_event(void)
{
    sensor.timeout_rising->attach(&helium_rising_timeout, 200ms);
}

void helium_falling_timeout(void)
{
    sensor.timer_count->start();
}

void helium_rising_timeout(void)
{
    uint32_t microsecond_by_liter = sensor.timer_count->elapsed_time().count();
    sensor.timer_count->stop();
    sensor.timer_count->reset();
    sensor.flow_minute = 0.0F;
    if(microsecond_by_liter)
    {
        uc.queue->call(&kv_apply, "LITERS", to_string(sensor.liters += 10));
        sensor.timeout_rising->attach(&helium_rising_timeout, microsecond_by_liter*20us); // approximate next flow update x2
        sensor.flow_minute = 84468500.0F/microsecond_by_liter; // 60000000us*1.4078L/min=84468500usL/min The empirical flow rate is 1.4078L/min for the magnet time
    }
    uc.set->call(&helium_set);
}

void helium_set(void)
{
    sensor.timer_set->stop();
    bool delayed = (sensor.timer_set->elapsed_time() < 9s);
    sensor.timer_set->reset();
    sensor.timer_set->start();
    if(sensor.set)
    {
        uc.set->cancel(sensor.set);
        sensor.set = 0;
    }
    if(!delayed)
    {
        ostringstream swrite;
        swrite << "HELIUM=" << sensor.id << "&" << sensor.room << "&" << sensor.liters << "&" << sensor.flow_minute << "&" << (sensor.update = stime(true)) << "\n";
        delayed = !uc.tr->set(swrite.str(), *sensor.server);
    }
    if(delayed) sensor.set = uc.set->call_in(10s, &helium_set);
}

string processing(string cmd)
{
    static bool password = false, admin = false;
    static string transfert;
    ostringstream ssend;
    ssend << fixed;
    ssend.precision(2);
    if(cmd.empty());
    else if(cmd == "*IDN?\n")
        return uc.idn;
    else if(cmd == "MAC?\n")
        ssend << "MAC\t[" << (uc.param[MAC].empty()?(uc.eth->get_mac_address()?(uc.param[MAC]=uc.eth->get_mac_address()):"00:00:00:00:00:00"):uc.param[MAC]) << "]";
    else if(cmd == "CLIENT?\n")
        ssend << "CLIENT\t[" << uc.tr->client() << "]";
    else if(cmd == "ETH?\n")
        ssend << "ETH\t[" << (uc.dhcp?"DHCP":"STATIC") << ((uc.eth->get_connection_status() == NSAPI_STATUS_GLOBAL_UP)?":UP]":":DOWN]");
    else if(cmd == "IP?\n")
        ssend << "IP\t[" << uc.tr->ip() << "]";
    else if(cmd == "IP=ALL?\n")
        ssend << "IP=ALL\t[" << uc.tr->ip(uc.tr->ip()) << "]";
    else if(cmd == "IP=MEM?\n")
        ssend << "IP=MEM\t[" << uc.ip << ":" << uc.port << " " << uc.mask << " " << uc.gateway << "]";
    else if(cmd.find("ETH=OFF") != string::npos)
        uc.param[MAC] = uc.tr->ip(false);
    else if(cmd.find("ETH=REBOOT") != string::npos)
    {
        uc.queue->call(&processing, "ETH=OFF");
        uc.queue->call(&kv_apply, "ETH", "");
    }
    else if(cmd.find("ETH=DHCP") != string::npos)
    {
        ethset(cmd);
        uc.param[MAC] = uc.tr->ip(true);
        kv("ETH", "ETH=DHCP");
    }
    else if(cmd.find("ETH=IP") != string::npos)
    {
        ethset(cmd);
        uc.param[MAC] = uc.tr->ip(true, uc.ip.c_str(), uc.port, uc.mask.c_str(), uc.gateway.c_str());
        kv("ETH", "ETH=IP " + uc.ip + ":" + to_string(uc.port) + " " + uc.mask + " " + uc.gateway);
    }
    else if(cmd == "HELIUM?\n")
        ssend << "HELIUM\t[" << sensor.id << " " << sensor.room << " " << sensor.liters << " " << sensor.flow_minute << " " << sensor.update << "]";
    else if(cmd.find("GET /HELIUM? HTTP") != string::npos)
        ssend << uc.tr->http.RETURN_OK << html(CONTENT) << "HELIUM\t[" << sensor.id << " " << sensor.room << " " << sensor.liters << " " << sensor.flow_minute << " " << sensor.update << "]";
    else if(cmd.find("LOGIN=") != string::npos)
    {
        cmd = cmd.substr(cmd.find("LOGIN=") + 6);
        password = ((cmd == uc.pwd) || (admin = (cmd == uc.adm)));
        ssend << uc.tr->http.RETURN_SEE_OTHER << "Location: /settings\r\n\r\n";
    }
    else if(cmd.find("PASSWORD=") != string::npos)
    {
        string  oldPwd(cmd.substr(cmd.find("PASSWORD=") + 9, cmd.find('&') - cmd.find("PASSWORD=") - 9)),
                newPwd(cmd.substr(cmd.find("NEW=") + 4, cmd.find(" HTTP") - cmd.find("NEW=") - 4));
        if(password && (cmd.find("REFERER: ") != string::npos) && (admin || ((oldPwd == uc.pwd) && (newPwd != oldPwd))))
        {
            uc.queue->call(&kv_apply, "PASSWORD", newPwd);
            password = false;
            ssend << uc.tr->http.RETURN_SEE_OTHER << "Location: /settings\r\n\r\n";
        }
        else ssend << uc.tr->http.RETURN_NO_CONTENT << html(CONTENT);
    }
    else if(cmd.find("HEAD /") != string::npos)
        ssend << uc.tr->http.RETURN_OK << html(CONTENT);
    else if(cmd.find("GET /FAVICON.ICO HTTP") != string::npos)
        ssend << uc.tr->http.RETURN_OK << html(FAVICON);
    else if(cmd.find("GET / HTTP") != string::npos)
    {
        ssend << uc.tr->http.RETURN_OK << html(CONTENT) << html(HEAD_META) << html(OPEN_BODY);
        ssend << "\r\n\t\t\t<p style=color:white>" << stime(true) << "</p>\r\n\t\t\t<form method=post>";
        if(sensor.update.size())
        {
            ssend << "\r\n\t\t\t\t<a href=/He_sensor.csv download=He_sensor.csv><button type=button style=text-align:center;width:5em>Download</button></a><br>";
            ssend << "\r\n\t\t\t\t<table style=border-collapse:collapse>\r\n\t\t\t\t\t<tr>\r\n\t\t\t\t\t\t<th style=text-align:center;padding:5px>Room</th>\r\n\t\t\t\t\t\t<th style=text-align:center;padding:5px>Liters</th>\r\n\t\t\t\t\t\t<th style=text-align:center;padding:5px>Flow/min</th>\r\n\t\t\t\t\t\t<th style=text-align:center;padding:5px>Update</th>\r\n\t\t\t\t\t</tr>";
            ssend << "\r\n\t\t\t\t\t<tr>\r\n\t\t\t\t\t\t<td style=text-align:center;padding:5px>" << sensor.room << "</td>\r\n\t\t\t\t\t\t<td style=text-align:center;padding:5px>" << sensor.liters << "</td>\r\n\t\t\t\t\t\t<td style=text-align:center;padding:5px" << (sensor.flow_minute>0?";color:red":"") << ">" << sensor.flow_minute << "</td>\r\n\t\t\t\t\t\t<td style=text-align:center;padding:5px" << (sensor.set?";color:red":"") << ">" << sensor.update << "</td>\r\n\t\t\t\t\t</tr>";
            ssend << "\r\n\t\t\t\t</table>";
        }
        ssend << "\r\n\t\t\t\t<button type=submit style=text-align:center;width:5em formaction=/settings>Settings</button>\r\n\t\t\t</form>";
        ssend << html(CLOSE_BODY);
    }
    else if(cmd.find("GET /HE_SENSOR.CSV HTTP") != string::npos)
        ssend << uc.tr->http.RETURN_OK << "Content-type: application/csv\r\n\r\nID;IP;ROOM;LITERS;FLOW/MIN;UPDATE\n" << sensor.id << ";" << uc.ip << ";" << sensor.room << ";" << sensor.liters << ";" << sensor.flow_minute << ";" << sensor.update << "\n";
    else if(cmd.find("POST /SETTINGS HTTP") != string::npos)
    {
        password = admin = false;
        ssend << uc.tr->http.RETURN_SEE_OTHER << "Location: /settings\r\n\r\n";
    }
    else if(cmd.find("GET /SETTINGS HTTP") != string::npos)
    {
        ssend << uc.tr->http.RETURN_OK << html(CONTENT) << html(HEAD) << html(OPEN_BODY);
        if(password && (cmd.find("REFERER: ") != string::npos))
        {
            ssend << "\r\n\t\t\t<details>\r\n\t\t\t\t<summary style=color:dimgray></summary>\r\n\t\t\t\t<span style=color:white>" << uc.idn << "</span>\r\n\t\t\t\t<br><a href=http://helium-uc-0><button type=button style=text-align:center;width:5em>HeMeter</button></a>\r\n\t\t\t</details>";
            ssend << "\r\n\t\t\t<fieldset>\r\n\t\t\t\t<legend style=margin-left:auto;margin-right:auto;color:white>SENSOR</legend>";
            ssend << "\r\n\t\t\t\t\t<table>\r\n\t\t\t\t\t\t<tr>";
            ssend << "\r\n\t\t\t\t\t\t\t<th style=text-align:center;width:5em><label>Room</label></th>";
            ssend << "\r\n\t\t\t\t\t\t\t<th style=text-align:center;width:5em><label>Liters</label></th>";
            ssend << "\r\n\t\t\t\t\t\t\t<th style=text-align:center;width:5em></th>";
            ssend << "\r\n\t\t\t\t\t\t</tr>\r\n\t\t\t\t\t</table>";
            ssend << "\r\n\t\t\t\t\t<form method=get>\r\n\t\t\t\t\t\t<table>\r\n\t\t\t\t\t\t\t<tr>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<td style=text-align:center;width:5em><input type=text name=Room style=text-align:center value=\"" << sensor.room << "\" required minlength=1 maxlength=6 size=6></td>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<td style=text-align:center;width:5em><input type=number name=Liters style=text-align:center value=\"" << sensor.liters << "\" required min=0 max=99999990 step=10 size=5></td>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<td style=text-align:center;width:5em><button type=submit formaction=/settings/sensor/set style=text-align:center;width:5em>Set</button></td>";
            ssend << "\r\n\t\t\t\t\t\t\t</tr>\r\n\t\t\t\t\t\t</table>\r\n\t\t\t\t\t</form>";
            ssend << "\r\n\t\t\t</fieldset>";
            ssend << "\r\n\t\t\t<details>\r\n\t\t\t\t<summary style=color:white>EXTRA SETTINGS</summary>";
            ssend << "\r\n\t\t\t\t<fieldset>\r\n\t\t\t\t\t<legend style=margin-left:auto;margin-right:auto;color:white>ETHERNET</legend>";
            ssend << "\r\n\t\t\t\t\t<form method=get>\r\n\t\t\t\t\t\t<table>\r\n\t\t\t\t\t\t\t<tr>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<th style=text-align:center;width:3em><label for=idDhcp>Dhcp</label></th>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<th style=text-align:center><label for=idIp>Ip</label></th>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<th style=text-align:center><label for=idMask>Mask</label></th>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<th style=text-align:center><label for=idGateway>Gateway</label></th>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<th style=width:3em></th>";
            ssend << "\r\n\t\t\t\t\t\t\t</tr>\r\n\t\t\t\t\t\t\t<tr>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<td style=text-align:center><input type=checkbox name=Connection value=Dhcp id=idDhcp " << (uc.dhcp?"checked":"") << "></td>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<td style=text-align:center><input type=text name=Ip id=idIp style=text-align:center value=\"" << uc.ip << ":" << uc.port << "\" required minlength=10 maxlength=18 size=15 pattern=\"^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)\\.?\\b){4}:80$\"></td>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<td style=text-align:center><input type=text name=Mask id=idMask style=text-align:center value=\"" << uc.mask << "\" required minlength=7 maxlength=15 size=15 pattern=\"^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)\\.?\\b){4}$\"></td>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<td style=text-align:center><input type=text name=Gateway id=idGateway style=text-align:center value=\"" << uc.gateway << "\" required minlength=7 maxlength=15 size=15 pattern=\"^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)\\.?\\b){4}$\"></td>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<td></td>";
            ssend << "\r\n\t\t\t\t\t\t\t</tr>\r\n\t\t\t\t\t\t</table>\r\n\t\t\t\t\t\t<button type=submit formaction=/settings/ethernet/set style=text-align:center;width:5em>Set</button>\r\n\t\t\t\t\t</form>";
            ssend << "\r\n\t\t\t\t\t<a href=/settings/ethernet/reboot><button type=button style=text-align:center;width:5em>Reboot</button></a>\r\n\t\t\t\t</fieldset>";
            ssend << "\r\n\t\t\t\t<fieldset>\r\n\t\t\t\t\t<legend style=margin-left:auto;margin-right:auto;color:white>PASSWORD</legend>\r\n\t\t\t\t\t<form method=post>";
            ssend << "\r\n\t\t\t\t\t\t<input style=text-align:center type=password name=password placeholder=OLD maxlength=16 " << (admin?"hidden":"required") << " list=extension><br>";
            ssend << "\r\n\t\t\t\t\t\t<input style=text-align:center type=password name=new placeholder=NEW maxlength=16 required list=extension><br>";
            ssend << "\r\n\t\t\t\t\t\t<button type=submit formaction=/settings/change style=text-align:center;width:5em>Set</button>\r\n\t\t\t\t\t</form>\r\n\t\t\t\t</fieldset>\r\n\t\t\t</details>";
            transfert.clear();
        }
        else ssend << "\r\n\t\t\t<form method=post>\r\n\t\t\t\t<input style=text-align:center type=password name=login autofocus required>\r\n\t\t\t</form>";
        ssend << "\r\n\t\t\t<br><a href=/><button type=button style=text-align:center;width:5em>Return</button></a>" << html(CLOSE_BODY);
    }
    else if(cmd.find("GET /SETTINGS/SENSOR/SET") != string::npos)
    {
        string START[] = { "ROOM=", "LITERS=" }, STOP[] = { "&LITERS", " HTTP" };
        for(int PARAM = 0; PARAM < 2; PARAM++)
        {
            if(cmd.find(START[PARAM]) != string::npos)
            {
                istringstream istr(cmd.substr(cmd.find(START[PARAM]) + START[PARAM].size(), cmd.find(STOP[PARAM]) - cmd.find(START[PARAM]) - START[PARAM].size()));
                switch(PARAM)
                {
                    case 0:
                        istr >> sensor.room;
                        uc.queue->call(&kv_apply, "ROOM", sensor.room);
                    break;
                    case 1:
                        uc.queue->call(&kv_apply, "LITERS", istr.str());
                        istr >> sensor.liters;   
                    break;
                    default: break;
                }
            }
        }
        uc.set->call(&helium_set);
        ssend << uc.tr->http.RETURN_RESET_CONTENT;
    }
    else if(cmd.find("GET /SETTINGS/ETHERNET/REBOOT HTTP") != string::npos)
    {
        if(password && (cmd.find("REFERER: ") != string::npos)) uc.queue->call(&processing, "ETH=REBOOT");
        ssend << uc.tr->http.RETURN_RESET_CONTENT;
    }
    else if(cmd.find("GET /SETTINGS/ETHERNET/SET?") != string::npos)
    {
        if(password && (cmd.find("REFERER: ") != string::npos))
        {
            if((cmd.find("CONNECTION=DHCP") != string::npos) && !uc.dhcp) uc.queue->call(&processing, "ETH=DHCP");
            else
            {
                string START[] = { "IP=", "%3A", "MASK=", "GATEWAY=" }, STOP[] = { "%3A", "&MASK", "&GATEWAY", " HTTP" };
                for(int PARAM = 0; PARAM < 4; PARAM++)
                {
                    if(cmd.find(START[PARAM]) != string::npos)
                    {
                        istringstream istr(cmd.substr(cmd.find(START[PARAM]) + START[PARAM].size(), cmd.find(STOP[PARAM]) - cmd.find(START[PARAM]) - START[PARAM].size()));
                        switch(PARAM)
                        {
                            case 0: istr >> uc.ip;      break;
                            case 1: istr >> uc.port;    break;
                            case 2: istr >> uc.mask;    break;
                            case 3: istr >> uc.gateway; break;
                            default:                    break;
                        }
                    }
                }
                uc.queue->call(&processing, "ETH=IP " + uc.ip + ":" + to_string(uc.port) + " " + uc.mask + " " + uc.gateway);
            }
        }
        ssend << uc.tr->http.RETURN_RESET_CONTENT;
    }
    else if(cmd.find("GET /") != string::npos)
        ssend << uc.tr->http.RETURN_NOT_FOUND << html(ERR404);
    else if(cmd.find("?") != string::npos)
        ssend << "?";
    return ssend.str();
}
#elif PROJECT_COMPILED ==   UC_HELIUM_METER
#define MBED_PROJECT        "HeliumMeter"

enum enumHTML       { CONTENT, HEAD, HEAD_META, OPEN_BODY, OPEN_BODY_IFRAME, CLOSE_BODY, FAVICON, ERR404, SMTP };
enum enumPARAM      { MAC, NAME, NETWORK, SPEC };

//  --------------------    INITIALIZE
void        setup(void);
//  --------------------    KVSTORE
string      kv(string, string = "");
void        kv_apply(string, string = "");
//  --------------------    TRANSMISSION
void        ethset(string = "");
void        ethup(void);
string      processing(string);
//  --------------------    STRING
string      stime(const bool& = true, const bool& = false, const time_t = 0);
//  --------------------    HTML
string      html(const enumHTML&);
//  --------------------    EMAIL
void        notify(const string, const string, const string = "");

struct {
            const string                            idn;
            UnbufferedSerial                        *ss;
            USBCDC                                  *usb;
            EthernetInterface                       *eth;
            Transmission                            *tr;
            Thread                                  *qThread;
            EventQueue                              *queue;
            string                                  alerts;
            string                                  pwd;
            string                                  adm;
            string                                  mail[10];
            string                                  param[4];
            int                                     color[8];
            BusOut                                  *leds;
            const Transmission::enum_trans_status   init;
            const Transmission::enum_trans_status   client;
            Transmission::enum_trans_status         state;
            string                                  ip;
            string                                  mask;
            string                                  gateway;
            uint16_t                                port;
            bool                                    dhcp;
} uc = {
            MBED_PROJECT + ((string)" Rev. ") + MBED_STRINGIFY(TARGET_NAME) + " " + __DATE__ + " " + __TIME__ + " MbedOS " + to_string(MBED_VERSION),
            new UnbufferedSerial(USBTX, USBRX, SERIAL_BAUDRATE),
            new USBCDC(false),
            new EthernetInterface,
            new Transmission(uc.ss, uc.usb, uc.eth, &processing, &ethup),
            new Thread,
            new EventQueue,
            "",
            { 0x2A, 0x2A, 0x55, 0x43 },
            { 0x2A, 0x45, 0x4C, 0x49, 0x4E, 0x53, 0x59, 0x53, 0x49, 0x2A },
            { "YANNIC.SIMON@UNIVERSITE-PARIS-SACLAY.FR", "", "", "", "", "", "", "", "", "" },
            { "", "HELIUM-UC-0", ".lps", "METER" },
            { 7, 6, 3, 2, 5, 1, 4, 0 },
            new BusOut(LED1, LED2, LED3),
            Transmission::WHITE_STATUS,
            Transmission::BLUE_CLIENT,
            Transmission::RED_DISCONNECTED,
            "192.168.1.0",
            "255.255.255.0",
            "192.168.1.0",
            80,
            false
};
struct He {
            int         id;
            string      ip;
            string      room;
            uint32_t    liters;
            float       flow_minute;
            string      update;
} helium[32];

int main(void)
{
    setup();
    while(true) uc.leds->write(uc.color[uc.state = uc.tr->recv()]);
}

void setup(void)
{
    uc.leds->write(uc.color[uc.init]);
    ThisThread::sleep_for(1s);
    uc.leds->write(uc.color[uc.state]);
    kv_apply("IDN");
    kv_apply("ETH");
    kv_apply("PASSWORD");
    kv_apply("MAILS");
    uc.qThread->start(callback(uc.queue, &EventQueue::dispatch_forever));
    for(He sensor : helium)
    {
        sensor.ip = sensor.room = sensor.update = "";
        sensor.id = sensor.liters = sensor.flow_minute = 0.0F;
    }
}

void ethup(void)
{
    if(uc.dhcp)
    {
        stime(true, true);
        ethset("ETH=DHCP");
        for(string mail : uc.mail) if(mail.size()) uc.queue->call_in(10min, &notify, "CONNECTION", mail, "");
    }
}

void notify(const string EVENT, const string MAIL, const string NOTIF)
{
    string FROM(MBED_PROJECT), SUBJECT("[Alert]" + FROM + " : " + EVENT);
    if(!uc.tr->smtp(MAIL, FROM, SUBJECT, NOTIF.empty()?html(SMTP):NOTIF)) uc.queue->call_in(1min, &notify, EVENT, MAIL, NOTIF);
    ThisThread::sleep_for(1s);
}

string kv(string kvKey, string kvParam)
{
    if(kvKey.empty()) return "";
    size_t keySize;
    kv_info_t keyInfo;
    char kvValue[256] = {0};
    if((kvKey == "IDN") && !kvParam.empty()) kv_reset("/kv/");
    if(kv_get_info("IDN", &keyInfo) != MBED_SUCCESS) if(kv_reset("/kv/") == MBED_SUCCESS) kv_set("IDN", "", 0, 0);
    if(kv_get_info(kvKey.c_str(), &keyInfo) == MBED_SUCCESS) if(kv_get(kvKey.c_str(), kvValue, 256, &keySize) == MBED_SUCCESS) if(!kvParam.empty()) kv_remove(kvKey.c_str());
    if(!kvParam.empty()) kv_set(kvKey.c_str(), kvParam.c_str(), kvParam.size(), 0);
    return kvValue;
}

void kv_apply(string kvKey, string kvParam)
{
    string kvValue = kv(kvKey);
    enum enumKvSTATUS { CHANGE_KVAL_PVAL, SAVE_NOKVAL_PVAL, LOAD_KVAL_NOPVAL, INIT_NOKVAL_NOPVAL } kvStatus = enumKvSTATUS((1*kvValue.empty()) + (2*kvParam.empty()));
    if((kvKey == "IDN") && (kvValue != uc.idn)) kv(kvKey, uc.idn);
    else if(kvKey == "ETH") processing((kvValue.substr(0, 3) == "ETH")?kvValue:"ETH=DHCP");
    else if(kvKey == "PASSWORD")
    {
        switch(kvStatus)
        {
            case CHANGE_KVAL_PVAL: case SAVE_NOKVAL_PVAL: if(kvValue != kvParam) kv(kvKey, kvValue = kvParam); break;
            case INIT_NOKVAL_NOPVAL: kv(kvKey, kvValue = uc.pwd); break;
            case LOAD_KVAL_NOPVAL: break;
        }
        uc.pwd = kvValue;
    }
    else if(kvKey == "MAILS")
    {
        switch(kvStatus)
        {
            case CHANGE_KVAL_PVAL: case SAVE_NOKVAL_PVAL: if(kvValue != kvParam) kv(kvKey, kvValue = kvParam); break;
            case INIT_NOKVAL_NOPVAL:
                for(const string &mail : uc.mail) if(!mail.empty()) kvValue += (kvValue.empty()?"":" ") + mail;
                kv(kvKey, kvValue);
            break;
            case LOAD_KVAL_NOPVAL: break;
        }
        istringstream stream(kvValue);
        for(string &mail : uc.mail)
        {
            string line;
            getline(stream, line, ' ');
            mail = line;
        }
    }
}

string stime(const bool& second, const bool& update, const time_t stamp)
{
    char timeLogs[22] = {0};
    time_t ucTimeStamp = stamp?stamp:time(NULL);
    static time_t ucTimeRef = ucTimeStamp;
    if((ucTimeStamp < 1600000000) || ((ucTimeStamp - ucTimeRef) > 86400)) uc.queue->call(&stime, false, true, 0);
    if(update)
    {
        time_t ntpTimeStamp = uc.tr->ntp();
        if(ntpTimeStamp > 0) set_time(ucTimeRef = ucTimeStamp = ntpTimeStamp);
    }
    struct tm * tmTimeStamp = localtime(&ucTimeStamp);
    strftime(timeLogs, 21, second?"%F %T":"%F %R", tmTimeStamp);
    return timeLogs;
}

void ethset(string cmd)
{
    size_t pos;
    if((uc.dhcp = (cmd.find("ETH=IP") == string::npos))) cmd = uc.tr->ip(uc.tr->ip());
    if((pos = cmd.find(":")) != string::npos)
    {
        istringstream istr(cmd.substr(pos+1));
        istr >> uc.port;
        cmd.replace(pos, 1, " ");
    }
    istringstream sget(cmd);
    for(int dot = 0, data = 0; getline(sget, cmd, ' '); dot = 0)
    {
        for(const char c:cmd) if(c == '.') dot++;
        if(dot == 3)
        {
            data++;
            if(cmd == "0.0.0.0") break;
            else
            {
                istringstream istr(cmd);
                switch(data)
                {
                    case 1: istr >> uc.ip;      break;
                    case 2: istr >> uc.mask;    break;
                    case 3: istr >> uc.gateway; break;
                    default:                    break;
                }
            }
        }
    }
}

string html(const enumHTML& PART)
{
    ostringstream ssend;
    ssend << fixed;
    ssend.precision(2);
    switch(PART)
    {
        case CONTENT:               ssend << "Content-Type: text/html; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\n\r\n"; break;
        case HEAD:                  ssend << "<!DOCTYPE html>\r\n<html>\r\n\t<head>\r\n\t\t<title>" << MBED_PROJECT << "</title>\r\n\t</head>"; break;
        case HEAD_META:             ssend << "<!DOCTYPE html>\r\n<html>\r\n\t<head>\r\n\t\t<title>" << MBED_PROJECT << "</title>\r\n\t\t<meta http-equiv=refresh content=10>\r\n\t</head>"; break;
        case OPEN_BODY:             ssend << "\r\n\t<body style=background-color:dimgray>\r\n\t\t<center>\r\n\t\t\t<h1 style=color:white>" << MBED_PROJECT << "</h1>"; break;
        case OPEN_BODY_IFRAME:      ssend << "\r\n\t<body style=background-color:transparency>\r\n\t\t<center>"; break;
        case CLOSE_BODY:            ssend << "\r\n\t\t</center>\r\n\t</body>\r\n</html>"; break;
        case FAVICON: case ERR404:  ssend << "Content-Type: image/svg+xml\r\nAccess-Control-Allow-Origin: *\r\n\r\n<svg width=\"100%\" height=\"100%\" viewBox=\"0 0 100 100\" style=\"background-color:black\" xmlns=\"http://www.w3.org/2000/svg\">\r\n\t<title>" << MBED_PROJECT << "</title>\r\n\t<text x=\"0\" y=\"90\" textLength=\"80\" font-size=\"120\" fill=\"white\" font-weight=\"bold\" font-style=\"italic\" lengthAdjust=\"spacingAndGlyphs\" style=\"font-family:\'Bauhaus 93\';-inkscape-font-specification:\'Bauhaus 93, Normal\'\">" << (PART==FAVICON?"ElI":"404") << "</text>\r\n</svg>"; break;
        case SMTP:
            ssend << "MIME-Version:1.0\r\nDate:" << stime(true) << "\r\nContent-Type:multipart/mixed; boundary=\"boundary0\"\r\n\r\n--boundary0\r\nContent-Type:multipart/alternative; boundary=\"boundary1\"";
            ssend << "\r\n\r\n--boundary1\r\nContent-Type:text/plain; charset=utf-8\r\nContent-Transfer-Encoding:8bit\r\n\r\n" << MBED_PROJECT << "\r\n" << stime(true) << "\r\n";
            ssend << "\r\n\tRoom\tLiters\tFlow/min\tUpdate";
            for(He sensor : helium) if(sensor.id) ssend << "\r\n\t" << sensor.room << "\t" << sensor.liters << "\t" << sensor.flow_minute << "\t" << sensor.update;
            ssend << "\r\n\r\nReal-time monitoring display (from the local network) : http://";
            if(uc.param[NAME].empty()) ssend << uc.tr->ip().substr(0, uc.tr->ip().find(':'));
            else ssend << uc.param[NAME] << uc.param[NETWORK] << ".u-psud.fr";
            ssend << "\r\n--boundary1\r\nContent-Type:text/html; charset=utf-8\r\nContent-Transfer-Encoding:8bit\r\n\r\n<html>\r\n\t<body>\r\n\t\t<center>\r\n\t\t\t<h1>" << MBED_PROJECT << "</h1>\r\n\t\t\t<p>" << stime(true) << "</p>\r\n\t\t\t<table style=border-collapse:collapse>";
            ssend << "\r\n\t\t\t\t<tr>\r\n\t\t\t\t\t<th style=text-align:center;padding:5px>Room</th>\r\n\t\t\t\t\t<th style=text-align:center;padding:5px>Liters</th>\r\n\t\t\t\t\t<th style=text-align:center;padding:5px>Flow/min</th>\r\n\t\t\t\t\t<th style=text-align:center;padding:5px>Update</th>\r\n\t\t\t\t</tr>";
            for(He sensor : helium) if(sensor.id) ssend << "\r\n\t\t\t\t<tr>\r\n\t\t\t\t\t<td style=text-align:center;padding:5px>" << sensor.room << "</td>\r\n\t\t\t\t\t<td style=text-align:center;padding:5px>" << sensor.liters << "</td>\r\n\t\t\t\t\t<td style=text-align:center;padding:5px>" << sensor.flow_minute << "</td>\r\n\t\t\t\t\t<td style=text-align:center;padding:5px>" << sensor.update << "</td>\r\n\t\t\t\t</tr>";
            ssend << "\r\n\t\t\t</table>\r\n\t\t</center>\r\n\t\t<footer><p>Real-time monitoring display (from the local network) : http://";
            if(uc.param[NAME].empty()) ssend << uc.tr->ip().substr(0, uc.tr->ip().find(':'));
            else ssend << uc.param[NAME] << uc.param[NETWORK] << ".u-psud.fr";
            ssend << "</p></footer>\r\n\t</body>\r\n</html>\r\n--boundary1\r\n--boundary0";
            ssend << "\r\nContent-Type: text/csv\r\nContent-disposition: attachment; filename=He_meter.csv;\r\nContent-Transfer-Encoding: 8bit\r\n\r\n";
            ssend << "ID;IP;ROOM;LITERS;FLOW/MIN;UPDATE\n";
            for(He sensor : helium) if(sensor.id) ssend << sensor.id << ";" << sensor.ip << ";" << sensor.room << ";" << sensor.liters << ";" << sensor.flow_minute << ";" << sensor.update << "\n";
            ssend << "\r\n--boundary0";
        break;
    }
    return ssend.str();
}

string processing(string cmd)
{
    static bool password = false, admin = false;
    static string transfert;
    ostringstream ssend;
    ssend << fixed;
    ssend.precision(2);
    if(cmd.empty());
    else if(cmd == "*IDN?\n")
        return uc.idn;
    else if(cmd == "MAC?\n")
        ssend << "MAC\t[" << (uc.param[MAC].empty()?(uc.eth->get_mac_address()?(uc.param[MAC]=uc.eth->get_mac_address()):"00:00:00:00:00:00"):uc.param[MAC]) << "]";
    else if(cmd == "CLIENT?\n")
        ssend << "CLIENT\t[" << uc.tr->client() << "]";
    else if(cmd == "ETH?\n")
        ssend << "ETH\t[" << (uc.dhcp?"DHCP":"STATIC") << ((uc.eth->get_connection_status() == NSAPI_STATUS_GLOBAL_UP)?":UP]":":DOWN]");
    else if(cmd == "IP?\n")
        ssend << "IP\t[" << uc.tr->ip() << "]";
    else if(cmd == "IP=ALL?\n")
        ssend << "IP=ALL\t[" << uc.tr->ip(uc.tr->ip()) << "]";
    else if(cmd == "IP=MEM?\n")
        ssend << "IP=MEM\t[" << uc.ip << ":" << uc.port << " " << uc.mask << " " << uc.gateway << "]";
    else if(cmd.find("ETH=OFF") != string::npos)
        uc.param[MAC] = uc.tr->ip(false);
    else if(cmd.find("ETH=REBOOT") != string::npos)
    {
        uc.queue->call(&processing, "ETH=OFF");
        uc.queue->call(&kv_apply, "ETH", "");
    }
    else if(cmd.find("ETH=DHCP") != string::npos)
    {
        ethset(cmd);
        uc.param[MAC] = uc.tr->ip(true);
        kv("ETH", "ETH=DHCP");
    }
    else if(cmd.find("ETH=IP") != string::npos)
    {
        ethset(cmd);
        uc.param[MAC] = uc.tr->ip(true, uc.ip.c_str(), uc.port, uc.mask.c_str(), uc.gateway.c_str());
        kv("ETH", "ETH=IP " + uc.ip + ":" + to_string(uc.port) + " " + uc.mask + " " + uc.gateway);
    }
    else if(cmd == "PING?\n")
    {
        ssend << "PING\t[";
        for(He sensor : helium) if(sensor.id) ssend << (ssend.str()=="PING\t["?"":", ") << sensor.ip << " " <<  (uc.tr->ping(sensor.ip, 80)?"OK":"ERR");
        ssend << "]";
    }
    else if(cmd == "HELIUM?\n")
    {
        ssend << "HELIUM\t[";
        for(He sensor : helium) if(sensor.id) ssend << (ssend.str()=="HELIUM\t["?"":", ") << sensor.id << " " << sensor.room << " " << sensor.liters << " " << sensor.flow_minute << " " << sensor.update;
        ssend << "]";
    }
    else if(cmd.find("HELIUM=") != string::npos)
    {
        cmd.replace(cmd.find("\n"), 1, "&");
        istringstream istr(cmd.substr(cmd.find("=")+1));
        string ip(uc.tr->client());
        ip = ip.substr(0, ip.find(":"));
        int position = -1, id = 0;
        while(getline(istr, cmd, '&'))
        {
            istringstream srecv(cmd);
            float float_p = 0.0F;
            srecv >> float_p;
            int int_p = float_p;
            switch(++position)
            {
                case 0: if((id = int_p)){   helium[id].id = id;
                                            helium[id].ip = ip;                 }break;
                case 1: if(id)              helium[id].room = cmd;              break;
                case 2: if(id)              helium[id].liters = int_p;          break;
                case 3: if(id)              helium[id].flow_minute = float_p;   break;
                case 4: if(id)              helium[id].update = cmd;            break;
                default:                                                        break;
            }
        }
    }
    else if(cmd.find("LOGIN=") != string::npos)
    {
        cmd = cmd.substr(cmd.find("LOGIN=") + 6);
        password = ((cmd == uc.pwd) || (admin = (cmd == uc.adm)));
        ssend << uc.tr->http.RETURN_SEE_OTHER << "Location: /settings\r\n\r\n";
    }
    else if(cmd.find("PASSWORD=") != string::npos)
    {
        string  oldPwd(cmd.substr(cmd.find("PASSWORD=") + 9, cmd.find('&') - cmd.find("PASSWORD=") - 9)),
                newPwd(cmd.substr(cmd.find("NEW=") + 4, cmd.find(" HTTP") - cmd.find("NEW=") - 4));
        if(password && (cmd.find("REFERER: ") != string::npos) && (admin || ((oldPwd == uc.pwd) && (newPwd != oldPwd))))
        {
            uc.queue->call(&kv_apply, "PASSWORD", newPwd);
            password = false;
            ssend << uc.tr->http.RETURN_SEE_OTHER << "Location: /settings\r\n\r\n";
        }
        else ssend << uc.tr->http.RETURN_NO_CONTENT << html(CONTENT);
    }
    else if(cmd.find("HEAD /") != string::npos)
        ssend << uc.tr->http.RETURN_OK << html(CONTENT);
    else if(cmd.find("GET /FAVICON.ICO HTTP") != string::npos)
        ssend << uc.tr->http.RETURN_OK << html(FAVICON);
    else if(cmd.find("GET / HTTP") != string::npos)
    {
        bool HeSens = false;
        for(He sensor : helium) HeSens |= sensor.id;
        ssend << uc.tr->http.RETURN_OK << html(CONTENT) << html(HEAD_META) << html(OPEN_BODY);
        ssend << "\r\n\t\t\t<p style=color:white>" << stime(true) << "</p>\r\n\t\t\t<form method=post>";
        if(HeSens)
        {
            ssend << "\r\n\t\t\t\t<a href=/He_meter.csv download=He_meter.csv><button type=button style=text-align:center;width:5em>Download</button></a><br>";
            ssend << "\r\n\t\t\t\t<table style=border-collapse:collapse>\r\n\t\t\t\t\t<tr>\r\n\t\t\t\t\t\t<th style=text-align:center;padding:5px>Room</th>\r\n\t\t\t\t\t\t<th style=text-align:center;padding:5px>Liters</th>\r\n\t\t\t\t\t\t<th style=text-align:center;padding:5px>Flow/min</th>\r\n\t\t\t\t\t\t<th style=text-align:center;padding:5px>Update</th>\r\n\t\t\t\t\t</tr>";
            for(He sensor : helium) if(sensor.id) ssend << "\r\n\t\t\t\t\t<tr>\r\n\t\t\t\t\t\t<td style=text-align:center;padding:5px>" << sensor.room << "</td>\r\n\t\t\t\t\t\t<td style=text-align:center;padding:5px>" << sensor.liters << "</td>\r\n\t\t\t\t\t\t<td style=text-align:center;padding:5px" << (sensor.flow_minute>0?";color:red":"") << ">" << sensor.flow_minute << "</td>\r\n\t\t\t\t\t\t<td style=text-align:center;padding:5px>" << sensor.update << "</td>\r\n\t\t\t\t\t</tr>";
            ssend << "\r\n\t\t\t\t</table>";
        }
        ssend << "\r\n\t\t\t\t<button type=submit style=text-align:center;width:5em formaction=/settings>Settings</button>\r\n\t\t\t</form>";
        ssend << html(CLOSE_BODY);
    }
    else if(cmd.find("GET /HE_METER.CSV HTTP") != string::npos)
    {
        ssend << uc.tr->http.RETURN_OK << "Content-type: application/csv\r\n\r\nID;IP;ROOM;LITERS;FLOW/MIN;UPDATE\n";
        for(He sensor : helium) if(sensor.id) ssend << sensor.id << ";" << sensor.ip << ";" << sensor.room << ";" << sensor.liters << ";" << sensor.flow_minute << ";" << sensor.update << "\n";
    }
    else if(cmd.find("POST /SETTINGS HTTP") != string::npos)
    {
        password = admin = false;
        ssend << uc.tr->http.RETURN_SEE_OTHER << "Location: /settings\r\n\r\n";
    }
    else if(cmd.find("GET /SETTINGS HTTP") != string::npos)
    {
        ssend << uc.tr->http.RETURN_OK << html(CONTENT) << html(HEAD) << html(OPEN_BODY);
        if(password && (cmd.find("REFERER: ") != string::npos))
        {
            ssend << "\r\n\t\t\t<details>\r\n\t\t\t\t<summary style=color:dimgray></summary>\r\n\t\t\t\t<span style=color:white>" << uc.idn << "</span>\r\n\t\t\t</details>";
            bool HeSens = false;
            for(He sensor : helium) HeSens |= sensor.id;
            if(HeSens)
            {
                ssend << "\r\n\t\t\t\t<fieldset>\r\n\t\t\t\t\t<legend style=margin-left:auto;margin-right:auto;color:white>SENSOR</legend>";
                ssend << "\r\n\t\t\t\t\t\t<table>\r\n\t\t\t\t\t\t\t<tr>";
                ssend << "\r\n\t\t\t\t\t\t\t\t<th style=text-align:center;width:5em><label>Sensor</label></th>";
                ssend << "\r\n\t\t\t\t\t\t\t\t<th style=text-align:center;width:5em><label>Room</label></th>";
                ssend << "\r\n\t\t\t\t\t\t\t\t<th style=text-align:center;width:5em><label>Liters</label></th>";
                ssend << "\r\n\t\t\t\t\t\t\t\t<th style=text-align:center;width:5em></th>";
                ssend << "\r\n\t\t\t\t\t\t\t</tr>\r\n\t\t\t\t\t\t</table>";
                for(He sensor : helium)
                {
                    if(sensor.id)
                    {
                        bool online = uc.tr->ping(sensor.ip, 80);
                        ssend << "\r\n\t\t\t\t\t\t<form method=get>\r\n\t\t\t\t\t\t\t<table>\r\n\t\t\t\t\t\t\t\t<tr>";
                        ssend << "\r\n\t\t\t\t\t\t\t\t\t<td style=text-align:center;width:5em><a href=http://helium-uc-" << sensor.id << ">" << sensor.id << "</a></td>";
                        ssend << "\r\n\t\t\t\t\t\t\t\t\t<td style=text-align:center;width:5em><input type=text name=Room style=text-align:center value=\"" << sensor.room << "\" required minlength=1 maxlength=6 size=6 " << (online?"":"disabled") << "></td>";
                        ssend << "\r\n\t\t\t\t\t\t\t\t\t<td style=text-align:center;width:5em><input type=number name=Liters style=text-align:center value=\"" << sensor.liters << "\" required min=0 max=99999990 step=10 size=5 " << (online?"":"disabled") << "></td>";
                        ssend << "\r\n\t\t\t\t\t\t\t\t\t<td style=text-align:center;width:5em><button type=submit formaction=/settings/sensor/set" << sensor.id << " style=text-align:center;width:5em " << (online?"":"disabled") << ">Set</button></td>";
                        ssend << "\r\n\t\t\t\t\t\t\t\t</tr>\r\n\t\t\t\t\t\t\t</table>\r\n\t\t\t\t\t\t</form>";
                    }
                }
                ssend << "\r\n\t\t\t\t</fieldset>";
            }
            ssend << "\r\n\t\t\t<details" << (transfert.empty()?"":" open") << ">\r\n\t\t\t\t<summary style=color:white>EXTRA SETTINGS</summary>";
            ssend << "\r\n\t\t\t\t<fieldset>\r\n\t\t\t\t\t<legend style=margin-left:auto;margin-right:auto;color:white>ETHERNET</legend>";
            ssend << "\r\n\t\t\t\t\t<form method=get>\r\n\t\t\t\t\t\t<table>\r\n\t\t\t\t\t\t\t<tr>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<th style=text-align:center;width:3em><label for=idDhcp>Dhcp</label></th>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<th style=text-align:center><label for=idIp>Ip</label></th>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<th style=text-align:center><label for=idMask>Mask</label></th>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<th style=text-align:center><label for=idGateway>Gateway</label></th>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<th style=width:3em></th>";
            ssend << "\r\n\t\t\t\t\t\t\t</tr>\r\n\t\t\t\t\t\t\t<tr>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<td style=text-align:center><input type=checkbox name=Connection value=Dhcp id=idDhcp " << (uc.dhcp?"checked":"") << "></td>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<td style=text-align:center><input type=text name=Ip id=idIp style=text-align:center value=\"" << uc.ip << ":" << uc.port << "\" required minlength=10 maxlength=18 size=15 pattern=\"^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)\\.?\\b){4}:80$\"></td>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<td style=text-align:center><input type=text name=Mask id=idMask style=text-align:center value=\"" << uc.mask << "\" required minlength=7 maxlength=15 size=15 pattern=\"^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)\\.?\\b){4}$\"></td>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<td style=text-align:center><input type=text name=Gateway id=idGateway style=text-align:center value=\"" << uc.gateway << "\" required minlength=7 maxlength=15 size=15 pattern=\"^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)\\.?\\b){4}$\"></td>";
            ssend << "\r\n\t\t\t\t\t\t\t\t<td></td>";
            ssend << "\r\n\t\t\t\t\t\t\t</tr>\r\n\t\t\t\t\t\t</table>\r\n\t\t\t\t\t\t<button type=submit formaction=/settings/ethernet/set style=text-align:center;width:5em>Set</button>\r\n\t\t\t\t\t</form>";
            ssend << "\r\n\t\t\t\t\t<a href=/settings/ethernet/reboot><button type=button style=text-align:center;width:5em>Reboot</button></a>\r\n\t\t\t\t</fieldset>";
            ssend << "\r\n\t\t\t\t<fieldset>\r\n\t\t\t\t\t<legend style=margin-left:auto;margin-right:auto;color:white>E-MAILS</legend>";
            ssend << "\r\n\t\t\t\t\t<p>" << transfert << (transfert.empty()?"":"</p>\r\n\t\t\t\t\t<p>") << "LIST[ ";
            transfert.clear();
            size_t size = ssend.str().size();
            for(const string &mail : uc.mail) if(!mail.empty() && (mail != uc.mail[0])) ssend << mail << ' ';
            ssend << ((ssend.str().size() == size)?"EMPTY ":"") << "]</p>";
            ssend << "\r\n\t\t\t\t\t<form enctype=text/plain method=post>\r\n\t\t\t\t\t\t<input style=text-align:center type=email name=emails placeholder=E-MAIL maxlength=60 required list=extension><br>";
            ssend << "\r\n\t\t\t\t\t\t<button type=submit formaction=/settings/emails/list/add style=text-align:center;width:5em>Add</button>\r\n\t\t\t\t\t\t<button type=submit formaction=/settings/emails/list/del style=text-align:center;width:5em>Del</button>\r\n\t\t\t\t\t</form>";
            ssend << "\r\n\t\t\t\t\t<form enctype=text/plain method=post>\r\n\t\t\t\t\t\t<textarea maxlength=420 rows=1 cols=19 name=emails placeholder=E-MAILS style=text-align:center " << (admin?"required":"hidden") << "></textarea><br>";
            ssend << "\r\n\t\t\t\t\t\t<button type=submit formaction=/settings/emails/list/add style=text-align:center;width:5em " << (admin?"":"hidden") << ">Add</button>\r\n\t\t\t\t\t\t<button type=submit formaction=/settings/emails/list/del style=text-align:center;width:5em " << (admin?"":"hidden") << ">Del</button>\r\n\t\t\t\t\t</form>";
            ssend << "\r\n\t\t\t\t\t<form enctype=text/plain method=post>\r\n\t\t\t\t\t\t<textarea maxlength=420 rows=1 cols=19 name=notifarea required style=text-align:center>Text to notify...</textarea><br>";
            ssend << "\r\n\t\t\t\t\t\t<button type=submit formaction=/settings/emails/notify style=text-align:center;width:5em>Notify</button>\r\n\t\t\t\t\t</form>";
            ssend << "\r\n\t\t\t\t\t<a href=/settings/emails/request><button type=button style=text-align:center;width:5em>Request</button></a>\r\n\t\t\t\t</fieldset>";
            ssend << "\r\n\t\t\t\t<fieldset>\r\n\t\t\t\t\t<legend style=margin-left:auto;margin-right:auto;color:white>PASSWORD</legend>\r\n\t\t\t\t\t<form method=post>";
            ssend << "\r\n\t\t\t\t\t\t<input style=text-align:center type=password name=password placeholder=OLD maxlength=16 " << (admin?"hidden":"required") << " list=extension><br>";
            ssend << "\r\n\t\t\t\t\t\t<input style=text-align:center type=password name=new placeholder=NEW maxlength=16 required list=extension><br>";
            ssend << "\r\n\t\t\t\t\t\t<button type=submit formaction=/settings/change style=text-align:center;width:5em>Set</button>\r\n\t\t\t\t\t</form>\r\n\t\t\t\t</fieldset>\r\n\t\t\t</details>";
        }
        else ssend << "\r\n\t\t\t<form method=post>\r\n\t\t\t\t<input style=text-align:center type=password name=login autofocus required>\r\n\t\t\t</form>";
        ssend << "\r\n\t\t\t<br><a href=/><button type=button style=text-align:center;width:5em>Return</button></a>" << html(CLOSE_BODY);
    }
    else if(cmd.find("GET /SETTINGS/SENSOR/SET") != string::npos)
    {
        cmd = cmd.substr(0, cmd.find(" HTTP") + 5) + "\n";
        istringstream idstr(cmd.substr(cmd.find("GET /SETTINGS/SENSOR/SET") + 24, cmd.find("?") - cmd.find("GET /SETTINGS/SENSOR/SET") - 24));
        int id;
        idstr >> id;
        if(id)
        {
            string setstr = "GET /SETTINGS/SENSOR/SET?" + cmd.substr(cmd.find("?") + 1, cmd.find(" HTTP") - cmd.find("?") - 1) + " HTTP\n";
            if(!uc.tr->set(setstr, helium[id].ip, 80)) uc.queue->call_in(10s, &processing, cmd);
        }
        ssend << uc.tr->http.RETURN_RESET_CONTENT;
    }
    else if(cmd.find("GET /SETTINGS/ETHERNET/REBOOT HTTP") != string::npos)
    {
        if(password && (cmd.find("REFERER: ") != string::npos)) uc.queue->call(&processing, "ETH=REBOOT");
        ssend << uc.tr->http.RETURN_RESET_CONTENT;
    }
    else if(cmd.find("GET /SETTINGS/ETHERNET/SET?") != string::npos)
    {
        if(password && (cmd.find("REFERER: ") != string::npos))
        {
            if((cmd.find("CONNECTION=DHCP") != string::npos) && !uc.dhcp) uc.queue->call(&processing, "ETH=DHCP");
            else
            {
                string START[] = { "IP=", "%3A", "MASK=", "GATEWAY=" }, STOP[] = { "%3A", "&MASK", "&GATEWAY", " HTTP" };
                for(int PARAM = 0; PARAM < 4; PARAM++)
                {
                    if(cmd.find(START[PARAM]) != string::npos)
                    {
                        istringstream istr(cmd.substr(cmd.find(START[PARAM]) + START[PARAM].size(), cmd.find(STOP[PARAM]) - cmd.find(START[PARAM]) - START[PARAM].size()));
                        switch(PARAM)
                        {
                            case 0: istr >> uc.ip;      break;
                            case 1: istr >> uc.port;    break;
                            case 2: istr >> uc.mask;    break;
                            case 3: istr >> uc.gateway; break;
                            default:                    break;
                        }
                    }
                }
                uc.queue->call(&processing, "ETH=IP " + uc.ip + ":" + to_string(uc.port) + " " + uc.mask + " " + uc.gateway);
            }
        }
        ssend << uc.tr->http.RETURN_RESET_CONTENT;
    }
    else if(cmd.find("POST /SETTINGS/EMAILS/NOTIFY HTTP") != string::npos)
    {
        if(password && (cmd.find("REFERER: ") != string::npos))
        {
            cmd = cmd.substr(cmd.find("NOTIFAREA=") + 10);
            for(char &c : cmd) if ((c >= 'A') && (c <= 'Z')) c -= 'A' - 'a';
            for(string mail : uc.mail) if(mail.size()) uc.queue->call(&notify, "NOTIFICATION", mail, cmd);
            ssend << uc.tr->http.RETURN_RESET_CONTENT;
        }
        else ssend << uc.tr->http.RETURN_NO_CONTENT << html(CONTENT);
    }
    else if(cmd.find("GET /SETTINGS/EMAILS/REQUEST HTTP") != string::npos)
    {
        if(password && (cmd.find("REFERER: ") != string::npos)) for(string mail : uc.mail) if(mail.size()) uc.queue->call(&notify, "REQUEST", mail, "");
        ssend << uc.tr->http.RETURN_RESET_CONTENT;
    }
    else if(cmd.find("POST /SETTINGS/EMAILS/LIST/") != string::npos)
    {
        if(password && (cmd.find("REFERER: ") != string::npos))
        {
            ssend << uc.tr->http.RETURN_SEE_OTHER << "Location: /settings\r\n\r\n";
            string run, err, post(cmd.substr(cmd.find("LIST/") + 5, 3));
            string::size_type pos = cmd.find("EMAILS=");
            cmd = cmd.substr(pos + 7);
            for(char carac : { '\r', '\n', '\t', '\\', ',', ';', '/' }) while((pos = cmd.find(carac)) != string::npos) cmd.replace(pos, 1, " ");
            istringstream gline(cmd);
            while(getline(gline, cmd, ' '))
            {
                if(cmd.find("@") != string::npos)
                {
                    string set;
                    if(post == "ADD") if(uc.tr->exist(cmd)) for(string &MAIL : uc.mail) if(((MAIL == cmd) || MAIL.empty()) && set.empty()) set = MAIL = cmd;
                    if(post == "DEL") for(string &MAIL : uc.mail) if((MAIL == cmd) && (MAIL != uc.mail[0]) && set.empty()) MAIL = "", set = cmd;
                    if(set.empty()) err += cmd + " ";
                    else            run += set + " ";
                }
            }
            if(!run.empty())
            {
                run = "<p>" + post + "[ " + run + "]</p>";
                string MAILS;
                for(const string &MAIL : uc.mail) if(!MAIL.empty()) MAILS += MAILS.empty()?MAIL:(" "+MAIL);
                uc.queue->call(&kv_apply, "MAILS", MAILS);
            }
            if(!err.empty()) err = "<p>ERROR[ " + err + "]</p>";
            transfert = run + err;
        }
        else ssend << uc.tr->http.RETURN_RESET_CONTENT;
    }
    else if(cmd.find("GET /*IDN? HTTP") != string::npos)
        ssend << uc.tr->http.RETURN_OK << html(CONTENT) << uc.idn;
    else if(cmd.find("GET /") != string::npos)
        ssend << uc.tr->http.RETURN_NOT_FOUND << html(ERR404);
    else if(cmd.find("?") != string::npos)
        ssend << "?";
    return ssend.str();
}
#endif