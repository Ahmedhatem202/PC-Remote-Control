#include "NodeMCU.h"

#define EEPROM_Initiallized_ADDRESS            0
#define Already_Downloaded_FLAG_ADDRESS        1
#define NewFirmware_Is_Uploaded_Flag_Address   2            
#define TargetECU_StartAddress_VM              3

#define EEPROM_Initiallized                    1 

#define Firmware_Is_Not_downloaded_Yet         0
#define Firmware_Is_ALready_downloaded         1

/* Flag to indecate if a new firmware is uploaded or not */
int Flag = 0;
#define NewFirmware_Is_Uploaded_Flag           1  

/* Flag to indecate if Wifi is connected or not */
char Not_Connected = 0;


void setup() 
{
  /* Start serial communication */
  Serial.begin(115200);    
  Serial.flush();
  EEPROM.begin(50); 

  // Check if the EEPROM is initialized or not
  if (readData(EEPROM_Initiallized_ADDRESS) != EEPROM_Initiallized)
  {
    // Set the EEPROM to be initiallized
    writeData(EEPROM_Initiallized_ADDRESS, EEPROM_Initiallized);

    // the new software is not downloaded yet 
    writeData(Already_Downloaded_FLAG_ADDRESS, 0); 
    writeData(NewFirmware_Is_Uploaded_Flag_Address, 0); 

    for(int i = 0; i<5; i++)
    {
      writeData(TargetECU_StartAddress_VM + i, 0);
    }
  }
}

void loop() 
{
  char x = '0';
  if(Serial.available())
  {
    x = Serial.read(); 
    if (x == 'A') 
    {
      /* First disconnecet the WIFI */
      WiFi.disconnect();
      /***********************************/
      // Serial.println("You select A");
      /***********************************/
      
      /* Scan for the available WIFI networks in the surround */
      if(WifiScanner())
      {
        /* WIFI is connected successfuly */
        After_Connection();
        Serial.print(Internnet_Connected_Successfuly);
        // Serial.println("Donne after connection");
        // Reconnect if MQTT client is not connected
        if (!client.connected()) 
        {
          Reconnect_To_MQTT();
        }
        client.publish("InternetConnected", "true",true);
        // Process incoming MQTT messages and maintain connection
        if(!Not_Connected)
        {
          client.loop();
        }
      }
      else
      {
        /* WIFI couldn't connect */
        Serial.print(Internnet_Connection_Failed);
      }

    }
    else if (x == 'B')
    {
      /*******************************/
      // Serial.println("you select B");
      /*******************************/

      // Check MQTT connection and handle MQTT messages
      if (WiFi.status() == WL_CONNECTED) 
      {
        // Reconnect if MQTT client is not connected
        if (!client.connected()) 
        {
          Reconnect_To_MQTT();
        }
        // Process incoming MQTT messages and maintain connection
        client.loop();

        // Serial.println("connected");

        /* Check for new updates */
        Check_For_NewUpdates();

      }
      else
      {
        // Serial.println("Wifi is not connected");
      }
    }
  }
  /* If Wifi is connected */
  // Process incoming MQTT messages and maintain connection
  if (WiFi.status() == WL_CONNECTED) 
  {
    client.loop();
  }
}
void Check_For_NewUpdates()
{
  if( readData(NewFirmware_Is_Uploaded_Flag_Address) == 1 )
  {
    /* the current SW version is different from the uploaded the version of the last uploaded Firmware  */
    Serial.print(New_Update_Is_Found);

    /**********************************/
    // Serial.println("T-to download ");
    /**********************************/

    /* wait for the response from the MCU */
    while(!Serial.available());
    char x = Serial.read();
    if( x == Confirm_Download_NewFirmware )
    {
      // if( readData(Already_Downloaded_FLAG_ADDRESS) == 0 )
      // {
      //   // Serial.println("Not already downloaded");
      //   /* Download the Firmware as it wasn't downloaded before */
      //   Download_NewFirmware();
      // }
      Download_NewFirmware();
      //Serial.println("Already downloaded");
      readFile("/NewFirmware.hex");
      client.publish(DoneInstalling_Topic, "true", true);
    }
    else
    {
      /* If the user didn't want to download the Firmware */
      return;
    }
  }
  else
  {
    // Serial.println("New Update Is Not Found");
    Serial.print(New_Update_Is_Not_Found);
    return;
  }
}

void After_Connection()
{
  /************ Initialization MQTT broker **************/
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(MQTT_callback);

  /************* Initialization Firebase ****************/
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  // Comment or pass false value when WiFi reconnection will control by your code or third party library e.g. WiFiManager
  Firebase.reconnectNetwork(true);

  // Since v4.4.x, BearSSL engine was used, the SSL buffer need to be set.
  // Large data transmission may require larger RX buffer, otherwise connection issue or data read time out can be occurred.
  // Current_SW_Version.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
  // Latest_SW_Version.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
  Firebase_Storage_dataObject.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
  // VehicleState.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);

  /* Assign download buffer size in byte */
  // Data to be downloaded will read as multiple chunks with this size, to compromise between speed and memory used for buffering.
  // The memory from external SRAM/PSRAM will not use in the TCP client internal rx buffer.
  config.fcs.download_buffer_size = 2048;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Connected = 1;

}

bool WifiScanner()
{
  while(1)
  {
    // Connect to WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin();

    // Start scanning for networks
    int numNetworks = WiFi.scanNetworks();
    if (numNetworks == 0) 
    {
      /*************************************************/
      // Serial.println("No available networks is found");
      /*************************************************/
      Serial.print(No_Internet_Networks_Found);
      return 0;
    } 
    Serial.flush();
    for (int i = 0; i < 8; i++) 
    {
      if(i != 0)
      {
        Serial.print(i + 1);
      }  
      Serial.print("-");
      Networks[i] = WiFi.SSID(i);
      Serial.println(Networks[i]);
    }
    Serial.print("#"); //terminate the passed array

    // Select a network
    while (!Serial.available());

    int Selected_Network = Serial.parseInt();
    Serial.flush();
    if(Selected_Network > 0 && Selected_Network <= 8)
    {
      Serial.print(Selected_Network_Accepted);
      /*********************************/
      // Serial.println("Password:");
      /*********************************/
      while (!Serial.available());

      Serial.flush();
      // Read the password from the serial port
      char password[64];
      int count = 0;
      char c = '\0';
      while (count < 64 ) 
      {
        if (Serial.available()) 
        {
          c = Serial.read();
          Serial.flush();
          if (c == '#') 
          {
            password[count] = '\0'; // Null-terminate the string
            break;
          }
          password[count] = c;
          count++;
        }
      }
      if( Setup_wifi(Networks[Selected_Network - 1], password) )
      {
        /* Wifi is CONNECTED successfuly */
        return 1;
      }
      else
      {
        /* Wifi couldn't connect */
        return 0; 
      }
    }
    else
    {
      /* Do nothing */
    }
  }
}

/************* Start by connecting to a WiFi network ********/
bool Setup_wifi(String Network, char *pass) 
{
  delay(10);

  /* First disconnect the wifi */
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(Network, pass);

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 30) 
  {
    delay(300);
    attempt++;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    /* If wifi status is not WL_CONNECTED then it couldn't connect */
    return 0;
  }
  else
  {
    /* If wifi status is WL_CONNECTED then it connected successfuly */
    return 1;
  }
  randomSeed(micros());
}

/******* Callback function for MQTT uppon recieving a new message *******/
void MQTT_callback(char* topic, byte* payload, unsigned int length) 
{  
  /*******************************************************************************************************************/
  // Serial.print("Message arrived from topic [ ");  Serial.print(topic);  Serial.print(" ]");  Serial.print(" is: ");
  /*******************************************************************************************************************/
  char msg[100];
  for (int i = 0; i < length; i++) 
  {    msg[i] = (char)payload[i];  }
  /**********************/
  // Serial.println(msg);
  /**********************/

  if( strcmp(topic, Target_ECU_Name_Topic) == 0 )
  {
    strncpy(TargetECU,msg,5);

    for(int i = 0; i<5; i++)
    {
      writeData(TargetECU_StartAddress_VM + i, TargetECU[i]);
    }
  }
  else if( strcmp(topic, New_Firmware_Is_Uploaded) == 0 )
  {
    if(strncmp(msg,Confirmation_message,strlen(Confirmation_message)) == 0)
    {
      /* A new software has been uploaded on the firebase by the website */
      writeData(NewFirmware_Is_Uploaded_Flag_Address, NewFirmware_Is_Uploaded_Flag);
    }
  }
  else if( strcmp(topic, Feature_Download_Confirmation) == 0 )
  {
    if(strncmp(msg,LED_Feature,strlen(LED_Feature)) == 0)
    {
      // Serial.println("Led feature is being downloaded");
      Download_NewFeature(LED_Feature_File_Path);
      // Firebase.RTDB.setBool(&fbdo, LED_Feature_RTDB_Path, true);
      // Serial.println("Led feature has downloaded");
    }
    if(strncmp(msg,Motor_Feature,strlen(Motor_Feature)) == 0)
    {
      // Serial.println("Motor feature is being downloaded");
      Download_NewFeature(Motor_Feature_File_Path);
      // Firebase.RTDB.setBool(&fbdo, Motor_Feature_RTDB_Path, true);
      // Serial.println("Motor feature has downloaded");
    }
  }
  else if( strcmp(topic, Update_Confirmation_Topic) == 0 )
  {
    if(strncmp(msg,Confirmation_message,strlen(Confirmation_message)) == 0)
    {
      /* DownLoad new firmware */
      // Serial.println("New firmware is detected");
      // if(Compare_SW_Version())
      if( readData(NewFirmware_Is_Uploaded_Flag_Address) == 0 )
      {
        /* matched */
        // Serial.println("The current SW version matches the recent uploaded version");
        // Serial.println("No firmware is detected");
        client.publish(Download_Start_Topic, "false",true);
        return;
      }
      else
      {
        /* Just Download the new software without installing it */
        client.publish(Download_Start_Topic, "true");
        Download_NewFirmware();
        writeData(Already_Downloaded_FLAG_ADDRESS, Firmware_Is_ALready_downloaded);
        return;
      }
    } 
  }
}


/******* Reconnect to mqtt broker if not connected ********/
void Reconnect_To_MQTT() 
{
  char attempt_Count = 0;
  // Loop until we're reconnected
  while (!client.connected()) 
  {
    // Serial.println("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str()) )
    {
      client.subscribe(New_Firmware_Is_Uploaded);
      client.subscribe(Update_Confirmation_Topic);
      client.subscribe(Download_Start_Topic);
      client.subscribe(Download_Confirmation_Topic);
      client.subscribe(Download_Percentage_Topic);
      client.subscribe(Feature_Download_Confirmation);
      client.subscribe(Target_ECU_Name_Topic);
      client.subscribe(StartInstalling_Topic);
      client.subscribe(DoneInstalling_Topic);
      client.subscribe(FailedInstalling_Topic);
      client.subscribe("InternetConnected");
      client.subscribe(Install_Percentage_Topic);

      Not_Connected = 0;
      // Serial.println("Connected to the topics");
    } 
    else 
    {
      // Serial.print("failed, rc=");
      // Serial.print(client.state());
      // Serial.println(" try again in 5 seconds");
      
      attempt_Count++;
      if(attempt_Count == 10)
      {
        // Serial.print(Failed_To_Subscribe_To_Topics);
        Not_Connected = 1;
        return;
      }
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/************** The Firebase Storage download callback function **************/
void fcsDownloadCallback(FCS_DownloadStatusInfo info)
{
    if (info.status == firebase_fcs_download_status_init)
    {
      // Serial.printf("Downloading file %s (%d) to %s\n", info.remoteFileName.c_str(), info.fileSize, info.localFileName.c_str());
    }
    else if (info.status == firebase_fcs_download_status_download)
    {
      int x = info.progress;
      char S[10];
      itoa(x,S,10);
      client.publish(Download_Percentage_Topic,S);
      //Serial.printf("Downloaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
    }
    else if (info.status == firebase_fcs_download_status_complete)
    {
      client.publish(Download_Confirmation_Topic,"true");
      //Serial.println("Download completed\n");
    }
    else if (info.status == firebase_fcs_download_status_error)
    {
      //Serial.printf("Download failed, %s\n", info.errorMsg.c_str());
    }
}

/************ Function used to read the downloaded file ************/
void readFile(const char *path)
{
  /***********************************************/
  // Serial.printf("Reading file: %s\n",path) ;
  /***********************************************/

  while(1)
  {
    if(Serial.available())
    {
      if(Serial.read() == Send_Target_Name)
      {
        break;
      }
    }
  }
  char x = 0;
  for(int i = 0; i<5; i++)
  {
    x = readData(TargetECU_StartAddress_VM + i);
    Serial.print(x);
  }
  Serial.print('#');
  
  File file = LittleFS.open(path, "r");
  if(!file)
  {
    /*************************************************/
    // Serial.println("failed to open file for reading");
    /*************************************************/
    client.publish(FailedInstalling_Topic, "true");
    Serial.print(Failed_To_Open_The_File);
    file.close();
    return;
  }
  else
  {
    // Serial.println("send v");
    Serial.print(File_Is_Opened);
    /*************************************************/
    // Serial.println("Successfully file is opened");
    /*************************************************/
  }
  const size_t fileSize = file.size(); // Get the size of the file

  if (fileSize > MAX_FILE_SIZE)
  {
      /************************************************/
      // Serial.println("File size is ");
      // Serial.println(fileSize);
      // Serial.println(" which exceeds maximum limit");
      /************************************************/
      Serial.print(File_Exceeds_The_Max_File_Allowed);
      file.close();
      return;
  }
  else
  { 
    /***************************/
    // Serial.println("It fits");
    /***************************/
    while(1)
    {
      if(Serial.available())
      {
        if(Serial.read() == Send_File_Size)
        {
          break;
        }
      }
    }
    Serial.print( fileSize / Number_Char_Per_Line );
    Serial.print('#'); 
  }

  while(1)
  {
    if(Serial.available())
    {
      if(Serial.read() == Start)
      {
        break;
      }
    }
  }
  client.publish(StartInstalling_Topic, "true");
  char c = 0;
  int i = 0;
  char Buffer[2] = {0};
  int HexData = 0;
  
  int FileSize_Int = fileSize / Number_Char_Per_Line;
  int Percentage = 0;
  int z = 0;
  int prev_z = 0;
  Serial.print(NodeMcu_will_Send_Line);
  // while(Serial.read() != FotaMaster_Is_Ready_to_Recieve_Line);
  while(file.available()) 
  {
    c = file.read();
    /* if the char is carriage return the continue */
    if (c == ':') 
    {
      Serial.print(':');
    } 
    else if(c == '\r')
    {
      continue;
    } 
    /* if the char is newLine then wait an ack from the FotaMaster to send next line */
    else if(c == '\n')
    {
      /**************************/
      //Serial.print('#');
      /**************************/
      
      while (true) 
      {
        if (Serial.available()) 
        {
          char Received_Message = Serial.read();
          if (Received_Message == Ack_For_NextLine) 
          {
            /*****************************/
            // Serial.println();
            /*****************************/
            if (file.position() == fileSize) 
            {
              client.publish(DoneInstalling_Topic, "true", true);
              /*******************/
              //Serial.println("Done installing");
              /*******************/
              Serial.print(End_Of_Transmitting);
            }
            else
            {
              prev_z = z;
              z = (Percentage*100) / FileSize_Int;
              if(z != prev_z)
              {
                char S[10];
                itoa(z,S,10);
                client.publish(Install_Percentage_Topic, S );
                /*******************/
                //Serial.println(S);
                /*******************/
              }
              Serial.print(NodeMcu_will_Send_Line);
              Percentage = Percentage + 1;
            }
            break;
          }
          else if ( Received_Message == NACK ) 
          {
            /* End of transmitting for any reason */
            writeData(Already_Downloaded_FLAG_ADDRESS, 0);
            // Serial.println("NACKKK");
            client.publish(FailedInstalling_Topic, "true");
            /* Close the file */
            file.close();
            LittleFS.remove("/NewFirmware.hex");
            return;
          }
        }
      }
      continue;
    }
    else
    {
      /* Send the Char to Fota Master */
      HexData = 0;
      Buffer[i] = c;
      i++;
      if(i == 2)
      {
        HexData = strtol(Buffer, NULL, 16);
        Serial.write(HexData);
        i = 0;
      }
    }
  }
  /* Close the file */
  file.close();

  /* Finally after reading the File successfully DELETE the file  */
  LittleFS.remove("/NewFirmware.hex");

  for(int i = 0; i<5; i++)
  {
    writeData(TargetECU_StartAddress_VM + i, 0);
  }

  /* Clear the Already_Downloaded as the file is already transmitte to the Fota Master */
  writeData(Already_Downloaded_FLAG_ADDRESS, 0);
  writeData(NewFirmware_Is_Uploaded_Flag_Address, 0);
  client.publish(New_Firmware_Is_Uploaded, "false", true);
}

/********** Function used to download new firmware ************/
void Download_NewFirmware() 
{
  if (Firebase.ready()) 
  {
    // Check if the ESP8266 is connected to the internet
    if (WiFi.status() != WL_CONNECTED) 
    {
      // Serial.println("Firmware couldn't be downloaded - Not connected to the internet");
      Serial.println(Connection_Lost_While_Downloading);
      return;
    }
    // Serial.println("\nFirmware is detected \nDownload new firmware...\n");
    
    File file = LittleFS.open("/NewFirmware.hex", "r");
    if(file)
    {
      LittleFS.remove("/NewFirmware.hex");
    }
    // The file systems for flash and SD/SDMMC can be changed in FirebaseFS.h.
    if (!Firebase.Storage.download(&Firebase_Storage_dataObject, STORAGE_BUCKET_ID, New_Firmware_File_Path, "/NewFirmware.hex", mem_storage_type_flash, fcsDownloadCallback)) 
    {
      // Serial.println("Firmware couldn't be downloaded");
      // Serial.println(Firebase_Storage_dataObject.errorReason());
      //return;
    } 
    else 
    {
     // Serial.println("The file is downloaded");
    }
  }
}

/********** Function used to download new feature file ************/
void Download_NewFeature(const char* path)
{
  if (Firebase.ready())
  {
    // Serial.println("\nNew feature is detected \nDownload new feature...\n");

    // The file systems for flash and SD/SDMMC can be changed in FirebaseFS.h.
    /*path = elantra/led.hex or motor.hex*/
    if (!Firebase.Storage.download(&Firebase_Storage_dataObject, STORAGE_BUCKET_ID /* Firebase Storage bucket id */, path /* path of remote file stored in the bucket */, "/NewFeature.hex" /* path to local file */, mem_storage_type_flash /* memory storage type, mem_storage_type_flash and mem_storage_type_sd */, fcsDownloadCallback /* callback function */))
      {
        // Serial.println(Firebase_Storage_dataObject.errorReason());
      } 
        
    else
    {
      readFile("/NewFeature.hex");
    }
  }
}

bool Compare_SW_Version()
{
  char current_version[10];
  char latest_version[10];

  if(Firebase.ready())
  {
    if (! Firebase.RTDB.getString(&Current_SW_Version,Current_SW_Version_Path) )
    {
      Serial.println("failed to read from Elantra/MobileApp/CurrentVersion");
      return false;
    }
    if (! Firebase.RTDB.getString(&Latest_SW_Version,Latest_SW_Version_Path) )
    {
      Serial.println("failed to read from New_Firmware/Latest_SW_Version");
      return false;
    }

    if( Current_SW_Version.dataType() == "string" )
    {
      strncpy(current_version, Current_SW_Version.stringData().c_str(), sizeof(current_version));
      Serial.print("Current version is: ");
      Serial.println(current_version);
    }
    if( Latest_SW_Version.dataType() == "string" )
    {
      strncpy(latest_version, Latest_SW_Version.stringData().c_str(), sizeof(latest_version));
      Serial.print("latest version is: ");
      Serial.println(latest_version);
    }

    if( strcmp(latest_version, current_version) == 0 )
    {
      return true;
    }
    else
    {
      return false;
    }
  }
    return 0;
}

/*****************************************EEPROM Functions********************************************/             
void writeData(int address, byte value) 
{
  EEPROM.write(address, value);
  EEPROM.commit(); // Ensure the data is written to flash memory
}

byte readData(int address) 
{
  return EEPROM.read(address);
}
