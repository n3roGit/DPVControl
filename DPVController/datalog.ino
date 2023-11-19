/**
* Regularly saves data about the state of the vehicle to disc.
*/


struct Datapoint{
  time:unsigned long,
  motorTemp:float
};

/*
* CONSTANTS
*/ 
const String HEADER = "time,motor temp";
const String DATALOG_DIR = "/datalog";
const unsigned long DATALOG_INTERVAL = 1000;//how often we record a datapoint in milliseconds.

/*
* GLOBAL VARIABLES
*/ 
File csvFile;
unsigned long lastDataLogTime = millis();

void datalogSetup(){
    if(!SPIFFS.begin(true)){
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    csvFile = openCSVFile();

    listLogFiles();
}



void datalogLoop(){
  if (millis()>lastDataLogTime+DATALOG_INTERVAL){
    DataPoint data = createDataPoint();
    saveDataPoint(data, csvFile);
    lastDataLogTime = millis();
  }
}

File& openCSVFile(){
    String filename;
    for(int i = 0; true; i++){
      filename=DATALOG_DIR+"/data_"+String(i)+".csv";
      if (!SPIFFS.exists(filename)) break;
    }
    File csvFile = SPIFFS.open(filename, FILE_WRITE);
    if (EnableDebugLog) Serial.println(String("writing to "+filename));
    csvFile.println(HEADER);
    csvFile.flush();
    return csvFile;
}

void listLogFiles(){
    Serial.printf("Listing directory: %s\r\n", DATALOG_DIR);

    File root = SPIFFS.open(DATALOG_DIR);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

DataPoint createDataPoint(){

}

void saveDataPoint(DataPoint datapoint, File &csvFile){

}