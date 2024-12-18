#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h" //Provide the token generation process info.
#include "addons/RTDBHelper.h" //Provide the RTDB payload printing info and other helper functions.
#include "string.h"
#include <EEPROM.h>

#define Selected_Network_Accepted                 'B' 
#define No_Internet_Networks_Found                'C' 
#define Internnet_Connected_Successfuly           'D' 
#define Internnet_Connection_Failed               'E' 
#define Failed_To_Subscribe_To_Topics             'W'

#define New_Update_Is_Found                       'F' 
#define New_Update_Is_Not_Found                   'G' 

#define Confirm_Download_NewFirmware              'T' 

#define Ack_For_NextLine                          'A' 
#define NACK                                      'u' 

#define Failed_To_Open_The_File                   'L'
#define File_Exceeds_The_Max_File_Allowed         'M'
#define Connection_Lost_While_Downloading         'N'

#define NodeMcu_will_Send_Line                    'o'
#define FotaMaster_Is_Ready_to_Recieve_Line       'p'
#define End_Of_Transmitting                       'k'

#define Send_Target_Name                          'x'
#define Send_File_Size                            'h'

#define Start                                     'v' 
#define Ready                                     'R' 
#define File_Is_Opened                            'y'

#define Number_Char_Per_Line                      43

char TargetECU[5] = {'\0'};
int Already_Downloaded = 0;
String Networks[8];
char password[10] = {'\0'}; 
int Connected = 0;

#define Line_ACK 'V'
#define CHUNK_SIZE 128

/* The maximum allowed file size that if the new firmware exeeds it will not be downloaded */
#define MAX_FILE_SIZE 50000

/* MQTT server */
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883; 

/*************************************************************/
/*                 Firebase Configuration                    */
/*************************************************************/
/* Insert Firebase project API Key */
#define API_KEY "AIzaSyArnmIpYULfLdJzER8VaHOnLZnjOdaGy5c"

/* Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "fota-436d5-default-rtdb.europe-west1.firebasedatabase.app"

/* Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL "fotateam@gmail.com"
#define USER_PASSWORD "123456"

/* Define the Firebase storage bucket ID e.g bucket-name.appspot.com */
#define STORAGE_BUCKET_ID "fota-436d5.appspot.com"

FirebaseAuth auth;
FirebaseConfig config;

/*************************************************************/
/*                    RTDB Configuration                     */
/*************************************************************/
FirebaseData Firebase_Storage_dataObject; /* FBDO used with firebase storage  */
FirebaseData Current_SW_Version;          /* FBDO used with firebase RTDB to get current SW version */
FirebaseData Latest_SW_Version;           /* FBDO used with firebase RTDB to get latest SW version */
FirebaseData VehicleState;                /* FBDO used with firebase RTDB to get vehicle state */
FirebaseData Target_ECU;                  /* FBDO used with firebase RTDB to get target ECU name */

FirebaseData fbdo;                

#define Vehicle_Name  "Elantra" 

const char* LED_Feature   = "LED.hex";
const char* Motor_Feature = "Motor.hex";

#define LED_Feature_File_Path     "Elantra/Features/LED.hex"
#define Motor_Feature_File_Path   "Elantra/Features/Motor.hex"
#define New_Firmware_File_Path    "Elantra/New_Firmware/New_Firmware.hex"

#define Current_SW_Version_Path   "CAR_LIST/Elantra/MobileApp/CurrentVersion"                                 /* Variable to hold the path of Current SW Version in the vehicle now */
#define Latest_SW_Version_Path    "CAR_LIST/Elantra/MobileApp/NewFirmware/New_Firmware_version"               /* Variable to hold the path of SW Version of the latest uploaded firmware */
#define VehicleState_Path         "CAR_LIST/Elantra/Vehicle_State"                                            /* Variable to hold the path of state of the vehicle wheather it is up to date or not */
#define Target_ECU_Path           "CAR_LIST/Elantra/MobileApp/NewFirmware/Filename"                           /* Variable to hold the path of the file name for the target ECU */

#define LED_Feature_RTDB_Path     "CAR_LIST/Elantra/MobileApp/Features/CarFeatures/LED"                /* Variable to hold the path of the LED feature on RTDB */
#define Motor_Feature_RTDB_Path   "CAR_LIST/Elantra/MobileApp/Features/CarFeatures/Motor"              /* Variable to hold the path of the Motor feature on RTDB */

/*************************************************************/
/*                       MQTT Topics                         */
/*************************************************************/
#define Update_Confirmation_Topic     "FOTA/ESP/UpdateConfirmation"                     /* A topic that will be sent on it from the mobile app the confirm to start downloading new firmware */
#define Download_Start_Topic          "FOTA/ESP/DownloadStart"                          /* A topic that will be sent on it from the Node Mcu that downloading new firmware has started */
#define Download_Confirmation_Topic   "FOTA/ESP/DownloadConfirmation"                   /* A topic that will be sent on it from the NodeMcu that downloading new firmware has ended */
#define New_Firmware_Is_Uploaded      "FOTA/OTA/NewUpdate"
#define Target_ECU_Name_Topic         "FOTA/ESP/TargetECU/Name"
#define StartInstalling_Topic         "FOTA/ESP/StartInstalling"
#define DoneInstalling_Topic          "FOTA/ESP/DoneInstalling"
#define FailedInstalling_Topic        "FOTA/ESP/FailedInstalling"

#define Download_Percentage_Topic     "FOTA/ESP/DownloadPercentage"                     /* A topic that will be sent on it from the NodeMcu the progress of downloading the new firmware */
#define Install_Percentage_Topic      "FOTA/ESP/InstallPercentage"
#define New_Features_Topic            "FOTA/OTA/NewFeatures"
#define Feature_Download_Confirmation "FOTA/Features/ESP/ConfirmationFile"

/*************************************************************/
/*                     MQTT Configuration                    */
/*************************************************************/
WiFiClient espClient;
PubSubClient client(espClient);

/* Buffer for received message from MQTT */
#define MSG_BUFFER_SIZE	(50) 
/* Constant string that carries the confirmation message for starting tha upload */
const char* Confirmation_message = "true";

/********************************************************************/
/*                  Functions prototype                             */
/********************************************************************/
bool Setup_wifi(String Network , char *pass);
void MQTT_callback(char* topic, byte* payload, unsigned int length);
void Reconnect_To_MQTT();
void fcsDownloadCallback(FCS_DownloadStatusInfo info);
void readFile(const char *path);
void Download_NewFirmware();
void Download_NewFeature(const char* FilePath);
bool Compare_SW_Version();
void After_Connection();
bool WifiScanner();
void Check_For_NewUpdates();
void writeData(int address, byte value);
byte readData(int address); 
/********************************************************************/