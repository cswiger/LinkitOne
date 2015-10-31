// This sketch takes the number of Satellites seen and publishes it at io.adafruit.com
// it's mostly just merging two example scripts together and altering the webclient one to post data

#include <LWiFi.h>
#include <LWiFiClient.h>
#include <LGPS.h>

// Modify with your access point specifics, ssid, password and type
#define WIFI_AP "WiFiAccessPointSSID"
#define WIFI_PASSWORD "WiFiAccessPointPasswd"
#define WIFI_AUTH LWIFI_WPA  // choose from LWIFI_OPEN, LWIFI_WPA, or LWIFI_WEP.
#define SITE_URL "io.adafruit.com"

// User you io.adafruit.com key in this next line - using headers is recommended as more secure than this
// https://learn.adafruit.com/adafruit-io/browser
#define DATA "/api/feeds/3460/data/send.json?x-aio-key=1234567890123456789012345678901234567890&value="

LWiFiClient c;

gpsSentenceInfoStruct info;
char buff[256];

// global for number of satellites
int numsats;
// we get sat info every 5 seconds, but publish to io.adafruit.com every minute or 12 counts
int count = 0;
boolean disconnectedMsg = false;

static unsigned char getComma(unsigned char num,const char *str)
{
  unsigned char i,j = 0;
  int len=strlen(str);
  for(i = 0;i < len;i ++)
  {
     if(str[i] == ',')
      j++;
     if(j == num)
      return i + 1; 
  }
  return 0; 
}

static double getDoubleNumber(const char *s)
{
  char buf[10];
  unsigned char i;
  double rev;
  
  i=getComma(1, s);
  i = i - 1;
  strncpy(buf, s, i);
  buf[i] = 0;
  rev=atof(buf);
  return rev; 
}

static double getIntNumber(const char *s)
{
  char buf[10];
  unsigned char i;
  double rev;
  
  i=getComma(1, s);
  i = i - 1;
  strncpy(buf, s, i);
  buf[i] = 0;
  rev=atoi(buf);
  return rev; 
}

void parseGPGGA(const char* GPGGAstr)
{
  /* Refer to http://www.gpsinformation.org/dale/nmea.htm#GGA
   * Sample data: $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
   * Where:
   *  GGA          Global Positioning System Fix Data
   *  123519       Fix taken at 12:35:19 UTC
   *  4807.038,N   Latitude 48 deg 07.038' N
   *  01131.000,E  Longitude 11 deg 31.000' E
   *  1            Fix quality: 0 = invalid
   *                            1 = GPS fix (SPS)
   *                            2 = DGPS fix
   *                            3 = PPS fix
   *                            4 = Real Time Kinematic
   *                            5 = Float RTK
   *                            6 = estimated (dead reckoning) (2.3 feature)
   *                            7 = Manual input mode
   *                            8 = Simulation mode
   *  08           Number of satellites being tracked
   *  0.9          Horizontal dilution of position
   *  545.4,M      Altitude, Meters, above mean sea level
   *  46.9,M       Height of geoid (mean sea level) above WGS84
   *                   ellipsoid
   *  (empty field) time in seconds since last DGPS update
   *  (empty field) DGPS station ID number
   *  *47          the checksum data, always begins with *
   */
  double latitude;
  double longitude;
  int tmp, hour, minute, second, num ;
  if(GPGGAstr[0] == '$')
  {
    tmp = getComma(1, GPGGAstr);
    hour     = (GPGGAstr[tmp + 0] - '0') * 10 + (GPGGAstr[tmp + 1] - '0');
    minute   = (GPGGAstr[tmp + 2] - '0') * 10 + (GPGGAstr[tmp + 3] - '0');
    second    = (GPGGAstr[tmp + 4] - '0') * 10 + (GPGGAstr[tmp + 5] - '0');
    
    sprintf(buff, "UTC timer %2d-%2d-%2d", hour, minute, second);
    Serial.println(buff);
    
    tmp = getComma(2, GPGGAstr);
    latitude = getDoubleNumber(&GPGGAstr[tmp]);
    tmp = getComma(4, GPGGAstr);
    longitude = getDoubleNumber(&GPGGAstr[tmp]);
    sprintf(buff, "latitude = %10.4f, longitude = %10.4f", latitude, longitude);
    Serial.println(buff); 
    
    tmp = getComma(7, GPGGAstr);
    num = getIntNumber(&GPGGAstr[tmp]);    
    sprintf(buff, "satellites number = %d", num);
    numsats = num;
    Serial.println(buff); 
  }
  else
  {
    Serial.println("Not get data"); 
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("LWiFi begin...");
  LWiFi.begin();
  
  // Turn GPS on first as it takes a while to lock
  LGPS.powerOn();
  Serial.println("LGPS Power on, and waiting ..."); 
  
  // keep retrying until connected to AP
  Serial.println("Connecting to AP");
  while (0 == LWiFi.connect(WIFI_AP, LWiFiLoginInfo(WIFI_AUTH, WIFI_PASSWORD)))
  {
    delay(1000);
  }
  
  printWifiStatus();
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("LGPS loop"); 
  LGPS.getData(&info);
  Serial.println((char*)info.GPGGA); 
  parseGPGGA((const char*)info.GPGGA);
  delay(5000);
  count += 1;
  if ( count == 12 ) {
    count = 0;
    
    Serial.println("Connecting to WebSite");
    while (0 == c.connect(SITE_URL, 80))
    {
      Serial.println("Re-Connecting to WebSite");
      delay(1000);
    }

    disconnectedMsg = false;
    // send HTTP request, ends with 2 CR/LF
    Serial.println("send HTTP GET request");
    c.print("POST ");
    c.print(DATA);
    c.print(String(numsats));
    c.println(" HTTP/1.1");
    c.println("Host: " SITE_URL);
    c.println("Connection: close");
    c.println();

    // waiting for server response
    Serial.println("waiting HTTP response:");
    while (!c.available())
    { 
      delay(100);
    }
    // Make sure we are connected, and dump the response content to Serial
    while (c)
    {
      int v = c.read();
      if (v != -1)
      {
        Serial.print((char)v);
      }
      else
      {
        Serial.println("no more content, disconnect");
        c.stop();
      }
    }

    if (!disconnectedMsg)
    {
      Serial.println("disconnected by server");
      disconnectedMsg = true;
    }    
  }  
}

void printWifiStatus()
{
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(LWiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = LWiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  Serial.print("subnet mask: ");
  Serial.println(LWiFi.subnetMask());

  Serial.print("gateway IP: ");
  Serial.println(LWiFi.gatewayIP());

  // print the received signal strength:
  long rssi = LWiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

