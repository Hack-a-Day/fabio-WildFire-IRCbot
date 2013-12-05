/* Hackaday Fubarino contest Entry Example
 Designed to run on Wicked Device WildFire
 Based on Code from https://github.com/robacarp/Arduino-IRC-Bot
 and Adafruit's geoLocate example
 Horribly Hacked by Adam Fabio
 Author's note: Please, for the love of god do not use this code as an example
 for anything. It is a thrown together mishmash.
 Bonus points if you can count how many commandments of embedded programming
 I blatantly violate.
 
 */

#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"
#include <WildFire.h>

/*FILL IN THE FOLLOWING WITH YOUR INFO*/
#define IRC_HOST "your-server" //Enter your IRC host here - tested with irc.afternet.org
#define WLAN_SSID       "YOUR-SSID"   // cannot be longer than 32 characters!
#define WLAN_PASS       "YOUR-PASSWORD"
String channel="#yourChannel"; //Enter your IRC channel here

void printlnBoth(String text);
void printBoth(String text);
WildFire wf;
char c = '\n';
String nick="SearchRobot";
String client_join = "";//"NICK ? \nUSER ? 8 * : ?\n";
String channel_join = "";//"JOIN ? ";

#define LED_PIN 5
unsigned long lastmillis = 0;
unsigned long lastTimeMillis = 0;
Adafruit_CC3000 cc3000 = Adafruit_CC3000(SPI_CLOCK_DIV2); // you can change this clock speed
bool must_disconnect = false;
bool stay_dead = false;
bool okToJoinChannel = false;
bool okToPing = false;
bool leetMode = false;
unsigned int searchCount = 0;

// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

Adafruit_CC3000_Client client;

const unsigned long
dhcpTimeout     = 60L * 1000L, // Max time to wait for address from DHCP
connectTimeout  = 60L * 1000L, // Max time to wait for server connection
responseTimeout = 15L * 1000L; // Max time to wait for data from server

uint32_t ip;
void setup() 
{

  uint32_t t;
  ip = 0L;
  digitalWrite(LED_PIN,LOW);
  wf.begin();

  Serial.begin(115200);
  Serial.println(F("Booting"));

  Serial.print("Free RAM: "); 
  Serial.println(getFreeRam(), DEC);

  Serial.print(F("Starting CC3000..."));
  if(!cc3000.begin()) 
  {
    Serial.println(F("Fail."));
    return;
  }

  Serial.print(F("OK.\r\nConnecting to network..."));
  if(!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) 
  {
    Serial.println(F("Failed!"));
    return;
  }
  Serial.println(F("connected!"));

  Serial.print(F("Requesting address from DHCP server..."));
  for(t=millis(); !cc3000.checkDHCP() && ((millis() - t) < dhcpTimeout); delay(1000));
  if(cc3000.checkDHCP()) {
    Serial.println(F("OK"));
  } 
  else 
  {
    Serial.println(F("Failed"));
    return;
  }

  while(!displayConnectionDetails()) delay(1000);

  // Look up IRC server's IP address
  Serial.print(F("\r\nGetting server IP address..."));
  t = millis();
  while((0L == ip) && ((millis() - t) < connectTimeout)) 
  {
    Serial.println(F("Trying..."));
    cc3000.getHostByName(IRC_HOST, &ip);
    delay(1000);
  }
  if(0L == ip) {
    Serial.println(F("failed"));
    return;
  }
  cc3000.printIPdotsRev(ip);
  Serial.println();

}

void loop()
{
  static unsigned int loopCounter = 0;
  if((loopCounter %100)==0)
  {
    ;//    Serial.println("Looping");
  }

  //check_dhcp();

  check_server_connection();

  read_from_server();

  read_from_terminal();

  check_server_disconnection();

  if((millis() - lastTimeMillis) > 45000)
  {
    if(okToPing)
    {
      printBoth("PING *\n");
    }
    lastTimeMillis = millis();
  }
  loopCounter++;

}





void check_server_connection()
{
  int dot_count = 0;
  if (! client.connected() ) 
  {
    Serial.println("Connecting.");
    while (! client.connected() )
    {
      client = cc3000.connectTCP(ip, 6667);  
      if (millis() - lastmillis > 50)
      {
        lastmillis = millis();
        Serial.print('.');
        dot_count ++;

        if (dot_count > 50)
        {
          Serial.println();
          dot_count = 0;
        }
      }
    }

    Serial.println("Connected to chat server (W00T).");
    client_join = "NICK " + nick + " \nUSER " + nick + " 8 * : itsaduinobot";
    //client_join = "USER " + nick + " 8 * : itsaduinobot";

    printlnBoth(client_join);
    delay(500);

  }
  if(okToJoinChannel)
  {    
    channel_join = "JOIN " + channel;
    printlnBoth(channel_join);
    okToJoinChannel=false;
    okToPing=true;
  }
}  


void check_server_disconnection()
{
  if (must_disconnect && client.connected())
  {
    printlnBoth("QUIT");
    must_disconnect = false;
    //client.stop();
  }

  if (! client.connected() )
  {
    //client.stop();
    Serial.println("DISCONNECTED.");
  }
  if(stay_dead)
  {
    while(1)
    {
      ;
    }
  }
}

/*I really hope Arduino's string memory handling is written well.... */
void read_from_server(){
  String a_sentence;
  String response;
  String after_space;

  while (client.available()){

    c = client.read();

    a_sentence = "";
    //pull out characters from the client serial and assemble a string
    while (client.available() && c != '\n'){
      a_sentence = a_sentence + c;
      c = client.read();
    }

    response = command_response(a_sentence);
    //todo, stuff to detect if the message was sent from/to a pm or a channel
    //  and direct the response back to that channel

    Serial.println(a_sentence);

    if (response != "")
    {
      printlnBoth(response);
    }

  }

}

String command_response(String a_sentence)
{
  String response = "";
  int space_index = a_sentence.indexOf(' ') + 1;
  int nick_index = a_sentence.indexOf(nick, space_index);

  if (a_sentence.startsWith("PING"))
  {
    //hehe...P(I|O)NG
    response = a_sentence;
    response.setCharAt(1,'O');
    Serial.println("Sending a Pong");
  }
  else if ( a_sentence.startsWith("PRIVMSG", space_index) ) 
  {   //I suppose this is the most common
    response = message_response(a_sentence);
  } 
  else if ( a_sentence.startsWith("NOTICE", space_index) ) 
  {
    Serial.println("NOTICE line");
  } 
  else if ( a_sentence.startsWith("JOIN", space_index) ) 
  {
    Serial.println("JOIN line");
  } 
  else if ( a_sentence.startsWith("PART", space_index) ) 
  {
    Serial.println("PART line");
  } 
  else if ( a_sentence.startsWith("MODE", space_index) ) 
  {
    Serial.println("MODE line");
  } 
  else if ( a_sentence.startsWith("0", space_index) ) 
  { //2xx = Network Info
    Serial.println("0 line");
  } 
  else if ( a_sentence.startsWith("2", space_index) ) 
  { //2xx = User Stats
    Serial.println("2 line");
    okToJoinChannel=true;

  } 
  else if ( a_sentence.startsWith("3", space_index) ) 
  { //3xx = MOTD / nicklist
    //    okToJoinChannel=true;
    Serial.println("3 lineq");

  } 
  else if ( a_sentence.startsWith("421", space_index) ) 
  { //421 = unknown command
    Serial.println("Got a 21 ERROR");
  } 
  else if ( a_sentence.startsWith("433", space_index) ) 
  { 
    Serial.println("Got a 433 error - Nick in use !");
    //451 = nickname in use
    //throw on a _ to the end of the nick, and force reconnect
    nick += '_';
    response = "NICK "+ nick + '\n';


  } 
  else if ( nick_index > -1 ) 
  {
    response = a_sentence.substring(nick_index);
  }

  return response;
}

String message_response(String a_sentence)
{
  String response = "";
  int space_index = a_sentence.indexOf(' ') + 1;
  int colon_index = a_sentence.indexOf(':', space_index) + 1;
  int nick_index = a_sentence.indexOf(nick, colon_index) + 1;

  if ( nick_index > colon_index)
  {
    if ( a_sentence.indexOf("ECHO") > -1 ) 
    {
      Serial.println("Got ECHO command");
      Serial.println(nick_index);
      Serial.println(colon_index);
      response = a_sentence;
    } 
    else if ( a_sentence.indexOf("SEARCH") > -1) 
    {
      Serial.println("Got SEARCH command");
      searchCount++;
      Serial.println(nick_index);
      Serial.println(colon_index);   
      //Only search hackaday every third time. (or if in 1337 mode)
      if((leetMode==true) || ((searchCount%3)==0))
      {
        response = "PRIVMSG " + channel + " :Search Complete! http://hackaday.com/?s=";
        response += a_sentence.substring((a_sentence.indexOf("SEARCH")+7));
        response += "\n";
      }
      else //normal mode - search google
      {
        response = "PRIVMSG " + channel + " :Search Complete! https://www.google.com/#q=";
        response += a_sentence.substring((a_sentence.indexOf("SEARCH")+7));
        response += "\n";	  
      }
    }
    else if ( a_sentence.indexOf("EXEC") > -1) 
    {
      Serial.println("Got EXEC command");      
      response = execute_command( a_sentence );
    }
    else if ( a_sentence.indexOf("1337") > -1) 
    {
      Serial.println("Got 1337 Mode");  
      if(leetMode==true) //already were in leet mode
      {
        response = "PRIVMSG " + channel + " :I am already 1337 H4x0r";
      }
      else //switch to leet mode
      {
        leetMode=true;
        response = "PRIVMSG " + channel + " :1337 H4x0r Mode ENGAGED.";
      }
    }
    else if ( a_sentence.indexOf("LAMER") > -1) 
    {
      Serial.println("Got 1337 Mode");  
      if(leetMode==false) //already were in lamer mode
      {
        response = "PRIVMSG " + channel + " :Noob";
      }
      else //switch to lamer mode
      {
        leetMode=false;
        response = "PRIVMSG " + channel + " :1337 H4x0r Mode DISENGAGD. Returning to Lamer status.";
      }
    }
    else if ( a_sentence.indexOf("HELP") > -1) 
    {
      Serial.println("Got 1337 Mode");  
      if(leetMode==false) //lamer mode help
      {
        response = "PRIVMSG " + channel + " :Commands: SEARCH, ECHO, SHOW";
      }
      else //leet mode help
      {

        response = "PRIVMSG " + channel + " :1337 H4x0r Mode Commands: SEARCH, ECHO, SHOW,LED ON, LED OFF.";
      }
    }		
    else if ( a_sentence.indexOf("LED ON") > -1) 
    {
      Serial.println("Got LED ON COMMAND");  
      digitalWrite(LED_PIN,HIGH);	  
      response = "PRIVMSG " + channel + " :Turning On an LED, because I'm an Arduino after all";
    }
    else if ( a_sentence.indexOf("LED OFF") > -1) 
    {
      Serial.println("Got LED OFF COMMAND");   
      digitalWrite(LED_PIN,LOW);	  
      response = "PRIVMSG " + channel + " :Turning OFF an LED. Someday I'll grow up to be a 6502";
    }
    else if(a_sentence.indexOf("SHOW") > -1)
    {
      Serial.println("Got SHOW command");
      Serial.println(nick_index);
      Serial.println(colon_index);
      if(leetMode==true)
      {
        response="PRIVMSG "+ channel + " :visit http://www.hackaday.com for fresh hacks every day!\n";
      }
      else
      {
        response="PRIVMSG "+ channel + " :I am a search robot! Adam really needs to clean up my code...";
      }
    } 
    else if ( a_sentence.indexOf("DIE") > -1) 
    {
      Serial.println("Got DIE command");      
      stay_dead = 1;
      must_disconnect = 1;
      response = "PRIVMSG "+ channel+" :I'll be back!";
    } 
    else 
    {
      Serial.println("Saw My Nick with no command");
      response = "PRIVMSG "+channel+" : Command not recognized! Try HELP";
    }
  }
  else 
  {
    Serial.print("not for me :(\nindex:");
    Serial.println(nick_index);
    Serial.println(colon_index);
  }

  return response;
}

String execute_command(String a_sentence){
  Serial.println("execute function or something");
  return "";
}

void read_from_terminal()
{
  char c = NULL;
  while(Serial.available())
  {
    c = Serial.read();
    //send tilde for linebreak
    if (c == '~')
    {
      client.println();
      Serial.println();
    } 
    else
    {
      //client.fastrprint(&c);
      client.print(c);
      Serial.print(c);
    }
  }
}

void printBoth(String text)
{ 
  Serial.print(text); 
  client.print(text); 
}
void printlnBoth(String text)
{ 
  Serial.println(text); 
  client.println(text); 
}


bool displayConnectionDetails(void) 
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;

  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv)) 
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  } 
  else {
    Serial.print(F("\nIP Addr: ")); 
    cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); 
    cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); 
    cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); 
    cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); 
    cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}



