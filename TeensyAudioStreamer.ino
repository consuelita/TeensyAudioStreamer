#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SerialFlash.h>

// Audio Library Objects
AudioPlaySdWav        playWav;    // WAV file player
AudioOutputUSB        usb;        // USB audio output
AudioConnection       patchCord1(playWav, 0, usb, 0);   // Left channel
AudioConnection       patchCord2(playWav, 1, usb, 1);   // Right channel

// SD Card
#define SDCARD_CS_PIN 10 // IMPORTANT: Your pin is likely to be different
#define MAX_RETRY_ATTEMPTS 3
#define MAX_FILES 15

// File Management
File root;
char currentFile[64] = "";
int fileIndex = 0;
int totalFiles = 0;
char fileList[MAX_FILES][64];  // Char arrays to save memory

// Timing
unsigned long lastStatusPrint = 0;
const unsigned long STATUS_INTERVAL = 5000; // Print the status every 5 seconds

// Error handling
int consecutiveFailures = 0;
const int MAX_CONSECUTIVE_FAILURES = 5;

void setup() 
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("Teensy USB Audio WAV Player");
  Serial.println("===========================");

  // Initialize the audio system - reduced memory to prevent corruption
  AudioMemory(25); 

  // Initialize the SD card with retry mechanism
  Serial.print("Initializing the SD card...");
  
  bool sdInitialized = false;
  for (int attempt = 1; attempt <= MAX_RETRY_ATTEMPTS; attempt++) {
    Serial.print(" (attempt ");
    Serial.print(attempt);
    Serial.print(")");
    
    if (SD.begin(SDCARD_CS_PIN)) 
    {
      sdInitialized = true;
      Serial.println(" OK!");
      break;
    }
    
    if (attempt < MAX_RETRY_ATTEMPTS) 
    {
      Serial.print(" failed, retrying...");
      delay(1000);
    }
  }

  if (!sdInitialized)
  {
    Serial.println(" FAILED!");
    Serial.println("SD card initialization failed after multiple attempts.");
    Serial.println("Please check:");
    Serial.println("- SD card is properly inserted");
    Serial.println("- SD card is formatted (FAT32 recommended)");
    Serial.println("- SD card connections are secure");
    Serial.println();
    Serial.println("System will continue checking every 10 seconds...");
    
    // Instead of infinite loop, periodically retry
    while (!sdInitialized) 
    {
      delay(10000); // Wait 10 seconds
      Serial.print("Retrying SD card initialization...");
      if (SD.begin(SDCARD_CS_PIN)) {
        sdInitialized = true;
        Serial.println(" SUCCESS!");
        break;
      } else {
        Serial.println(" still failed");
      }
    }
  }

  // Scan for WAV files
  scanForWavFiles();

  if (totalFiles == 0) 
  {
    Serial.println("No WAV files were found on this SD card.");
    Serial.println("Please add some .WAV files to the root directory of the SD card.");
    Serial.println("System will continue checking every 30 seconds...");
    
    // Instead of infinite loop, periodically rescan
    while (totalFiles == 0) 
    {
      delay(30000); // Wait 30 seconds
      Serial.println("Rescanning for WAV files...");
      scanForWavFiles();
    }
  }

  Serial.println("Setup complete - starting playback");

  // Start playing the first file
  playNextFile();
}

void loop() 
{
  // Check if the current file is finished playing
  if(!playWav.isPlaying() && totalFiles > 0) 
  {
    delay(500);
    playNextFile();
  }

  // Print the status periodically
  if (millis() - lastStatusPrint > STATUS_INTERVAL) 
  {
    printStatus();
    lastStatusPrint = millis();
  }

  // Check for serial commands
  if (Serial.available()) 
  {
    handleSerialCommand();
  }

  delay(50);
}

void scanForWavFiles() {
  Serial.println(F("Scanning for WAV files..."));
  
  // Reset file count
  totalFiles = 0;
  
  // Try to open root directory
  root = SD.open("/");
  if (!root) 
  {
    Serial.println(F("Failed to open root directory"));
    return;
  }

  while (true) {
    File entry = root.openNextFile();

    if (!entry) 
    { 
      break; // No more files
    }

    // Use fixed buffer instead of String
    char filename[64];
    strncpy(filename, entry.name(), sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';
    
    // Convert to lowercase for comparison
    for (int i = 0; filename[i]; i++) 
    {
      filename[i] = tolower(filename[i]);
    }

    // Check if it's a WAV file and not a directory
    if (!entry.isDirectory() && strstr(filename, ".wav") != NULL) 
    {
      if (totalFiles < MAX_FILES) 
      {
        // Store original filename (not lowercase)
        strncpy(fileList[totalFiles], entry.name(), sizeof(fileList[totalFiles]) - 1);
        fileList[totalFiles][sizeof(fileList[totalFiles]) - 1] = '\0';
        
        Serial.print(F("Found: "));
        Serial.println(fileList[totalFiles]);
        totalFiles++;
      } 
      else 
      {
        Serial.print(F("Warning: Maximum file limit ("));
        Serial.print(MAX_FILES);
        Serial.println(F(") reached. Some files ignored."));
        entry.close();
        break;
      }
    }
    entry.close();
  }
  root.close();

  Serial.print(F("Total WAV files found: "));
  Serial.println(totalFiles);
}

void playNextFile() 
{
  if (totalFiles == 0) 
  {
    Serial.println(F("No files available to play"));
    return;
  }

  // Bounds checking
  if (fileIndex >= totalFiles) 
  {
    fileIndex = 0;
  }

  // Stop current playback
  playWav.stop();
  delay(100);

  // Get the next file - copy to current file buffer
  strncpy(currentFile, fileList[fileIndex], sizeof(currentFile) - 1);
  currentFile[sizeof(currentFile) - 1] = '\0';
  
  Serial.print(F("Playing: "));
  Serial.println(currentFile);

  // Create full path for the file
  char fullPath[80];
  snprintf(fullPath, sizeof(fullPath), "/%s", currentFile);

  // Start playing the file
  if (playWav.play(fullPath)) 
  {
    Serial.println(F("Playback started successfully."));
    consecutiveFailures = 0; // Reset failure counter on success
    fileIndex = (fileIndex + 1) % totalFiles; // Move to next file
  } 
  else 
  {
    Serial.print(F("Error starting playback for: "));
    Serial.println(currentFile);
    consecutiveFailures++;
    
    // If we've had too many consecutive failures, something is seriously wrong
    if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) 
    {
      Serial.println(F("Too many consecutive playback failures!"));
      Serial.println(F("Possible issues:"));
      Serial.println(F("- Corrupted WAV files"));
      Serial.println(F("- Insufficient memory"));
      Serial.println(F("- SD card problems"));
      Serial.println(F("Rescanning for files..."));
      
      // Rescan in case files were corrupted or changed
      scanForWavFiles();
      consecutiveFailures = 0;
      fileIndex = 0;
      
      if (totalFiles == 0) 
      {
        Serial.println(F("No valid files found after rescan"));
        return;
      }
    } 
    else 
    {
      // Skip to next file
      fileIndex = (fileIndex + 1) % totalFiles;
      delay(500);
      // Don't recursively call - let main loop handle it
    }
  }
}

void printStatus() 
{
  Serial.println(F("--- Status ---"));
  Serial.print(F("Current file: "));
  Serial.println(currentFile);
  Serial.print(F("File "));
  Serial.print((fileIndex == 0) ? totalFiles : fileIndex);
  Serial.print(F(" of "));
  Serial.println(totalFiles);
  Serial.print(F("Playing: "));
  Serial.println(playWav.isPlaying() ? F("YES") : F("NO"));
  Serial.print(F("Processor usage: "));
  Serial.print(AudioProcessorUsage());
  Serial.println(F("%"));
  Serial.print(F("Memory usage: "));
  Serial.print(AudioMemoryUsage());
  Serial.print(F(" of 25 blocks ("));
  Serial.print((AudioMemoryUsage() * 100) / 25);
  Serial.println(F("%)"));
  
  if (consecutiveFailures > 0) 
  {
    Serial.print(F("Consecutive failures: "));
    Serial.println(consecutiveFailures);
  }
  
  Serial.println(F("Commands: 'n' = next, 'r' = restart, 's' = stop, 'p' = play, '?' = help"));
  Serial.println();
}

void handleSerialCommand() 
{
  char cmd = Serial.read();

  switch (cmd) 
  {
    case 'n':
    case 'N':
      Serial.println(F("Command: Next track"));
      playWav.stop();
      delay(100);
      playNextFile();
      break;

    case 'r':
    case 'R':
      Serial.println(F("Command: Restart current track"));
      playWav.stop();
      delay(100);
      // Go back one file since playNextFile() will increment
      fileIndex = (fileIndex - 1 + totalFiles) % totalFiles;
      playNextFile();
      break;

    case 's':
    case 'S':
      Serial.println(F("Command: Stop playback"));
      playWav.stop();
      break;
      
    case 'p':
    case 'P':
      Serial.println(F("Command: Play/Resume"));
      if (!playWav.isPlaying()) {
        playNextFile();
      }
      break;
      
    case '?':
    case 'h':
    case 'H':
      printHelp();
      break;
      
    case 'f':
    case 'F':
      Serial.println(F("Command: Rescan files"));
      scanForWavFiles();
      if (totalFiles > 0) {
        fileIndex = 0;
        playNextFile();
      }
      break;
      
    default:
      // Ignore other characters
      break;
  }
}

void printHelp() 
{
  Serial.println(F("=== Teensy USB Audio WAV Player ==="));
  Serial.println(F("Commands:"));
  Serial.println(F("  n/N - Next track"));
  Serial.println(F("  r/R - Restart current track"));  
  Serial.println(F("  s/S - Stop playback"));
  Serial.println(F("  p/P - Play/Resume"));
  Serial.println(F("  f/F - Rescan for files"));
  Serial.println(F("  ?/h/H - Show this help"));
  Serial.println();
  Serial.println(F("The Teensy will automatically loop through all WAV files"));
  Serial.println(F("Connect via USB to your Android device to test the app"));
  Serial.println(F("Supported formats: WAV files in root directory of SD card"));
  Serial.println();
}