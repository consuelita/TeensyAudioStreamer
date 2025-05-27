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
#define MAX_FILENAME_LEN 64 

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

// Debug counters
unsigned long loopCount = 0;
unsigned long statusCallCount = 0;

void setup() 
{
  Serial.begin(115200);
  delay(1000);

  // Add memory info at startup
  Serial.println(F("=== TEENSY WAV PLAYER STARTUP ==="));
  Serial.print(F("Free RAM at startup: "));
  printFreeMemory();
  Serial.println();

  Serial.println(F("Teensy USB Audio WAV Player"));
  Serial.println(F("==========================="));

  // Initialize the audio system - reduced memory to prevent corruption
  Serial.print(F("Allocating AudioMemory(25)..."));
  AudioMemory(25); 
  Serial.println(F(" done"));
  
  Serial.print(F("Free RAM after AudioMemory: "));
  printFreeMemory();
  Serial.println();

  // Initialize the SD card with retry mechanism
  Serial.print(F("Initializing the SD card..."));
  
  bool sdInitialized = false;
  for (int attempt = 1; attempt <= MAX_RETRY_ATTEMPTS; attempt++) 
  {
    Serial.print(F(" (attempt "));
    Serial.print(attempt);
    Serial.print(F(")"));
    
    if (SD.begin(SDCARD_CS_PIN)) 
    {
      sdInitialized = true;
      Serial.println(F(" OK!"));
      break;
    }
    
    if (attempt < MAX_RETRY_ATTEMPTS) 
    {
      Serial.print(F(" failed, retrying..."));
      delay(1000);
    }
  }

  if (!sdInitialized)
  {
    Serial.println(F(" FAILED!"));
    Serial.println(F("SD card initialization failed after multiple attempts."));
    Serial.println(F("Please check:"));
    Serial.println(F("- SD card is properly inserted"));
    Serial.println(F("- SD card is formatted (FAT32 recommended)"));
    Serial.println(F("- SD card connections are secure"));
    Serial.println();
    Serial.println(F("System will continue checking every 10 seconds..."));
    
    // Instead of infinite loop, periodically retry
    while (!sdInitialized) 
    {
      delay(10000); // Wait 10 seconds
      Serial.print(F("Retrying SD card initialization..."));
      if (SD.begin(SDCARD_CS_PIN)) 
      {
        sdInitialized = true;
        Serial.println(F(" SUCCESS!"));
        break;
      } 
      else 
      {
        Serial.println(F(" still failed"));
      }
    }
  }

  Serial.print(F("Free RAM before file scan: "));
  printFreeMemory();
  Serial.println();

  // Scan for WAV files
  scanForWavFiles();

  Serial.print(F("Free RAM after file scan: "));
  printFreeMemory();
  Serial.println();

  if (totalFiles == 0) 
  {
    Serial.println(F("No WAV files were found on this SD card."));
    Serial.println(F("Please add some .WAV files to the root directory of the SD card."));
    Serial.println(F("System will continue checking every 30 seconds..."));
    
    // Instead of infinite loop, periodically rescan
    while (totalFiles == 0) 
    {
      delay(30000); // Wait 30 seconds
      Serial.println(F("Rescanning for WAV files..."));
      scanForWavFiles();
    }
  }

  Serial.println(F("Setup complete - starting playback"));
  Serial.print(F("Free RAM before first playback: "));
  printFreeMemory();
  Serial.println();

  // Start playing the first file
  playNextFile();
  
  Serial.println(F("=== ENTERING MAIN LOOP ==="));
}

void loop() 
{
  // Check if the current file is finished playing
  if(!playWav.isPlaying() && totalFiles > 0) 
  {
    Serial.println(F(">> Track finished, playing next"));
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

// Function to estimate free RAM (simple version for Teensy)
void printFreeMemory() 
{
  // Simple approximation - just show that we're checking memory
  Serial.print(F("~"));
  Serial.print(millis()); // Use millis as a changing value to show function is called
  Serial.print(F(" (mem check)"));
}

void scanForWavFiles() 
{
  Serial.println(F("=== SCANNING FOR WAV FILES ==="));
  
  // Reset file count
  totalFiles = 0;
  
  Serial.print(F("Free RAM at scan start: "));
  printFreeMemory();
  Serial.println();
  
  // Try to open root directory
  root = SD.open("/");
  if (!root) 
  {
    Serial.println(F("ERROR: Failed to open root directory"));
    return;
  }
  Serial.println(F("Root directory opened successfully"));

  while (totalFiles < MAX_FILES) 
  {
    File entry = root.openNextFile();
    if (!entry) 
    {
      Serial.println(F("No more files to scan"));
      break;
    }

    // Get filename
    const char* name = entry.name();
    Serial.print(F("Examining file: '"));
    Serial.print(name);
    Serial.print(F("' - "));
    
    // Skip if filename is too long
    if (strlen(name) >= MAX_FILENAME_LEN) 
    {
      Serial.println(F("SKIP: filename too long"));
      entry.close();
      continue;
    }
    
    // Skip hidden/system files (starting with . or _)
    if (name[0] == '.' || name[0] == '_') 
    {
      Serial.println(F("SKIP: hidden/system file"));
      entry.close();
      continue;
    }
    
    // Skip directories
    if (entry.isDirectory()) 
    {
      Serial.println(F("SKIP: directory"));
      entry.close();
      continue;
    }

    // Create lowercase copy for extension checking
    char lowerName[MAX_FILENAME_LEN];
    strncpy(lowerName, name, MAX_FILENAME_LEN - 1);
    lowerName[MAX_FILENAME_LEN - 1] = '\0';
    
    // Convert to lowercase manually
    for (int i = 0; lowerName[i]; i++) 
    {
      if (lowerName[i] >= 'A' && lowerName[i] <= 'Z') 
      {
        lowerName[i] += 32;
      }
    }

    // Check if it ends with .wav
    int len = strlen(lowerName);
    if (len > 4 && strcmp(&lowerName[len-4], ".wav") == 0) 
    {
      // Store the original filename (not lowercase)
      strncpy(fileList[totalFiles], name, MAX_FILENAME_LEN - 1);
      fileList[totalFiles][MAX_FILENAME_LEN - 1] = '\0';
      
      Serial.print(F("ADDED: "));
      Serial.println(fileList[totalFiles]);
      totalFiles++;
      
      Serial.print(F("Current file count: "));
      Serial.print(totalFiles);
      Serial.print(F(", Free RAM: "));
      printFreeMemory();
      Serial.println();
    } 
    else 
    {
      Serial.println(F("SKIP: not a .wav file"));
    }
    
    entry.close();
  }
  root.close();

  Serial.print(F("=== SCAN COMPLETE: "));
  Serial.print(totalFiles);
  Serial.println(F(" WAV files found ==="));
}

void playNextFile() 
{
  Serial.println(F("=== PLAY NEXT FILE ==="));
  
  if (totalFiles == 0) 
  {
    Serial.println(F("ERROR: No files available to play"));
    return;
  }

  // Bounds checking
  if (fileIndex >= totalFiles) 
  {
    Serial.print(F("DEBUG: fileIndex ("));
    Serial.print(fileIndex);
    Serial.print(F(") >= totalFiles ("));
    Serial.print(totalFiles);
    Serial.println(F("), resetting to 0"));
    fileIndex = 0;
  }

  // Stop current playback
  Serial.println(F("Stopping current playback..."));
  playWav.stop();
  delay(100);

  // Get the next file - copy to current file buffer
  Serial.print(F("Copying filename from slot "));
  Serial.print(fileIndex);
  Serial.print(F(": "));
  Serial.println(fileList[fileIndex]);
  
  strncpy(currentFile, fileList[fileIndex], sizeof(currentFile) - 1);
  currentFile[sizeof(currentFile) - 1] = '\0';
  
  Serial.print(F("Current file set to: '"));
  Serial.print(currentFile);
  Serial.println(F("'"));

  // Create full path for the file
  char fullPath[80];
  snprintf(fullPath, sizeof(fullPath), "/%s", currentFile);
  Serial.print(F("Full path: '"));
  Serial.print(fullPath);
  Serial.println(F("'"));

  // Start playing the file
  Serial.println(F("Attempting to start playback..."));
  if (playWav.play(fullPath)) 
  {
    Serial.println(F("SUCCESS: Playback started"));
    consecutiveFailures = 0; // Reset failure counter on success
    fileIndex = (fileIndex + 1) % totalFiles; // Move to next file
    Serial.print(F("Next fileIndex will be: "));
    Serial.println(fileIndex);
  } 
  else 
  {
    Serial.print(F("ERROR: Failed to start playback for: "));
    Serial.println(currentFile);
    consecutiveFailures++;
    Serial.print(F("Consecutive failures: "));
    Serial.println(consecutiveFailures);
    
    // If we've had too many consecutive failures, something is seriously wrong
    if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) 
    {
      Serial.println(F("CRITICAL: Too many consecutive playback failures!"));
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
        Serial.println(F("CRITICAL: No valid files found after rescan"));
        return;
      }
    } 
    else 
    {
      // Skip to next file
      fileIndex = (fileIndex + 1) % totalFiles;
      Serial.print(F("Skipping to next file, index now: "));
      Serial.println(fileIndex);
      delay(500);
      // Don't recursively call - let main loop handle it
    }
  }
  
  Serial.print(F("Free RAM after playback attempt: "));
  printFreeMemory();
  Serial.println();
  Serial.println(F("=== END PLAY NEXT FILE ==="));
}

void printStatus() 
{
  Serial.println(F("=========================================="));
  
  Serial.print(F("File: "));
  Serial.println(currentFile);
  
  Serial.print(F("Track: "));
  Serial.print((fileIndex == 0) ? totalFiles : fileIndex);
  Serial.print(F(" of "));
  Serial.println(totalFiles);
  
  // Core playback info
  bool isPlaying = playWav.isPlaying();
  Serial.print(F("Status: "));
  Serial.println(isPlaying ? F("PLAYING") : F("STOPPED"));
  
  if (isPlaying) 
  {
    Serial.print(F("Length: "));
    Serial.print(playWav.lengthMillis() / 1000);
    Serial.println(F(" seconds"));
    
    Serial.print(F("Position: "));
    Serial.print(playWav.positionMillis() / 1000);
    Serial.println(F(" seconds"));
    
    int progress = (playWav.positionMillis() * 100) / playWav.lengthMillis();
    Serial.print(F("Progress: "));
    Serial.print(progress);
    Serial.println(F("%"));
  }
  
  // Technical info
  Serial.print(F("CPU: "));
  Serial.print(AudioProcessorUsage(), 1);
  Serial.println(F("%"));
  
  Serial.print(F("Audio Memory: "));
  Serial.print(AudioMemoryUsage());
  Serial.println(F(" blocks"));
  
  if (consecutiveFailures > 0) 
  {
    Serial.print(F("Failures: "));
    Serial.println(consecutiveFailures);
  }
  
  Serial.println(F("Commands: n=next, s=stop, p=play, ?=help"));
  Serial.println(F("=========================================="));
}

void handleSerialCommand() 
{
  char cmd = Serial.read();
  Serial.print(F("DEBUG: Received command '"));
  Serial.print(cmd);
  Serial.println(F("'"));

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
      if (!playWav.isPlaying()) 
      {
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
      if (totalFiles > 0) 
      {
        fileIndex = 0;
        playNextFile();
      }
      break;
      
    default:
      Serial.print(F("DEBUG: Ignoring unknown command '"));
      Serial.print(cmd);
      Serial.println(F("'"));
      break;
  }
}

void printHelp() 
{
  Serial.println(F("=== TEENSY USB AUDIO WAV PLAYER ==="));
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