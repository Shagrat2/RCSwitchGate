#include <RCSwitch.h>
#include <avr/wdt.h>
#include <SPI.h>
#include <UIPEthernet.h>
//#include <Ethernet.h>

// Параметры

#define HOST_NAME "RCSW_"      // Название устройства
#define StaticIP
#define BUFSIZ 48
#define BUFSend 120

#define SendTry 3

//#define ALIVETIMER             // Флаг присутствия
//#define ALIVETIMEOUT 300000    // Тайминг присутствия - 5min
#define REPEATTIMEOUT 1500     // Таймаут повторного запроса

#define RCControl              // Флаг управления RC

//#define UIP_CONNECT_TIMEOUT = 5; // Таймаут соединения

uint8_t mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEC}; // MAC

// Сеть
IPAddress deviceip(10,9,0,240);
IPAddress sendip(10,9,0,2);

//IPAddress deviceip(192,168,1,230);
//IPAddress sendip(192,168,1,101);

//----------------
char buf[BUFSend];

unsigned long lasttime; // Last commin time
char lastbuf[BUFSend];      // Last buffer

#ifdef ALIVETIMER
  unsigned long AliveTime;
#endif  

RCSwitch mySwitch = RCSwitch();
EthernetClient client;

#ifdef RCControl
  EthernetServer server = EthernetServer(80);

const char HTMLH[] PROGMEM = 
  "HTTP/1.1 200 OK\r\n"
  "Content-Type: text/html\r\n"
  "\r\n"
  "<html><head><title>RC Switch gate</title></head><body>";
  
const char HTMLE[] PROGMEM =   
  "</body></html>\r\n";  
#endif 

unsigned long GetTickDiff(unsigned long AOldTickCount, unsigned long ANewTickCount){
  // This is just in case the TickCount rolled back to zero
  if (ANewTickCount >= AOldTickCount){
    return ANewTickCount - AOldTickCount;
  } else {
    return 0xFFFFFFFF - AOldTickCount + ANewTickCount;
  }
}

bool sendHTTPRequest() {  
  bool ret = true;
  
  if (client.connect(sendip, 80)) {
    // Serial.println("connected");
    
    // Make a HTTP request:
    client.println(buf);
    client.println("User-Agent: arduino-ethernet");     
    client.println("Connection: close");     
    client.println();
    
    Serial.println(buf);   
  } else {
    Serial.println("Timeout connected");
    ret = false;
  }
  
  client.stop(); 
  return ret;
}

void setup() {
  wdt_disable();
  delay(5000);
  
  Serial.begin(9600);
  #ifndef StaticIP
    Serial.println("Read DHCP config");  
    if (!Ethernet.begin(mac)){
      Serial.println("Failed to configure Ethernet using DHCP");
  #endif
    Ethernet.begin(mac, deviceip);
  #ifndef StaticIP  
    }   
  #endif  

  wdt_enable(WDTO_8S);
  
  Serial.print("localIP: ");
  Serial.println(Ethernet.localIP());
  Serial.print("subnetMask: ");
  Serial.println(Ethernet.subnetMask());
  Serial.print("gatewayIP: ");
  Serial.println(Ethernet.gatewayIP());
  Serial.print("dnsServerIP: ");
  Serial.println(Ethernet.dnsServerIP());
  
  mySwitch.enableReceive(1);
  mySwitch.enableTransmit(4);  
  
  #ifdef RCControl
    server.begin();  
  #endif  
  
  #ifdef ALIVETIMER
    AliveTime = millis();
  #endif  
  lasttime = millis();
}

void loop() {
  wdt_reset();

  // RCSwitch
  if (mySwitch.available()) {    
    sprintf(buf, "GET /objects/?script=rcswitch&rcswitch=%ld-%ibit-P%i HTTP/1.0", mySwitch.getReceivedValue(), mySwitch.getReceivedBitlength(), mySwitch.getReceivedProtocol() );
    
    // Test double quest   
    if ( (strcmp(buf, lastbuf) != 0) || (GetTickDiff(lasttime, millis()) >= REPEATTIMEOUT)){
      if ( sendHTTPRequest() ){
        strncpy(lastbuf, buf, BUFSend);
        lasttime = millis();
      } 
    }
    
    mySwitch.resetAvailable();    
  }  
  
  // ALIVETIMER
  #ifdef ALIVETIMER
    if (GetTickDiff(AliveTime, millis()) >= ALIVETIMEOUT){
      sprintf(buf, "GET /objects/?script=rcswitchalive HTTP/1.0" );
      sendHTTPRequest();
      AliveTime = millis();
    }
  #endif

  // HTTP
  #ifdef RCControl
    if (EthernetClient sclient = server.available()) {
      char header[BUFSIZ];
      byte index = 0;
    
      boolean currentLineIsBlank = true;
      while (sclient.available()) {
        char c = sclient.read();
        if (index < BUFSIZ){        
          header[index] = c;
          index++;
          header[index] = 0;
        }
        
        // Ansfer
        if (c == '\n' && currentLineIsBlank){ 
          break; 
        }
        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }        
      }

      char delimiters[] = "/?,= ";

      // Head
      char * istr = strtok(header, delimiters);
      char * status = "";
      
      // Type
      istr = strtok(NULL, delimiters);
      if (strcmp(istr, "RCSwitch") == 0){
        // id        
        int unsigned long id   = strtol( strtok(NULL, delimiters), NULL, 10 );

        // bits        
        int unsigned long bits = strtol( strtok(NULL, delimiters), NULL, 10 );
       
        //Serial.print("Send command: ");
        Serial.print( id );
        Serial.print( "," );
        Serial.println( bits );
        
        // Send try
        for (int n=0; n < SendTry; n++){
          mySwitch.send( id, bits );
        }

        status = "Command send OK";
      } else {        
        Serial.print("Error type: ");
        Serial.println( istr );
        
        status = "Command send ERROR";
      }
    
      //-- response --  
      sclient.print(HTMLH);
      sclient.print( status );
      sclient.print(HTMLE);
 
      sclient.stop();
    }   
  #endif    
} 
