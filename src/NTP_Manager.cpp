#include "NTP_Manager.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <ArduinoJson.h>
#include "RTC_Manager.h"

// These definitions allocate memory for the globals. 
// They are declared as 'extern' in define.h and defined here.
String g_lat;
String g_lon;
String g_timezone;
String g_utcTime;
String g_localTime;
time_t g_epochTime;

// Saves geo-location and timezone offset data to NVS (Non-Volatile Storage).
// This allows the device to quickly set up time synchronization on subsequent boots
// without needing to query the geo-location API again immediately.
// offset: The timezone offset in seconds from UTC.
// The "geo-cache" namespace is used for these preferences.
void saveGeoCache(long offset) {
  if (prefs.begin("geo-cache", false)) {
    prefs.putString("lat", g_lat);
    prefs.putString("lon", g_lon);
    prefs.putString("tz_name", g_timezone);
    prefs.putLong("offset", offset);
    prefs.end();
    Serial.println("Geo-coordinates saved to Preferences.");
  } else {
    Serial.println("Failed to open Preferences for saving.");
  }
}

// Loads geo-location and timezone offset data from NVS.
// If valid cached data is found, it configures the system time with the loaded offset.
// Returns true if cached data was successfully loaded and applied, false otherwise.
bool loadGeoCache() {
  if (prefs.begin("geo-cache", true)) {
    // Using isKey() prevents internal NVS error logging on first boot.
    g_lat = prefs.isKey("lat") ? prefs.getString("lat") : "";
    g_lon = prefs.isKey("lon") ? prefs.getString("lon") : "";
    g_timezone = prefs.isKey("tz_name") ? prefs.getString("tz_name") : "";
    long offset = prefs.isKey("offset") ? prefs.getLong("offset") : 0;
    prefs.end();

    if (g_lat.length() > 0 && g_timezone.length() > 0) {
      // Apply the loaded offset to the system immediately
      // configTime expects the offset in seconds from UTC and a DST offset (0 here).
      // The NTP server "pool.ntp.org" is used for time synchronization.
      char tzbuf[64];
      int hours = offset / 3600;
      int minutes = abs((int)(offset % 3600) / 60);
      snprintf(tzbuf, sizeof(tzbuf), "UTC%+d:%02d", -hours, minutes); // POSIX TZ format for logging.
      
      configTime(offset, 0, "pool.ntp.org");
      Serial.printf("Loaded Geo Cache: %s, %s (Offset: %ld)\n", g_lat.c_str(), g_timezone.c_str(), offset);
      return true;
    }
  }
  Serial.println("No valid Geo Cache found.");
  return false;
}

// Initializes NTP synchronization.
// Attempts to load cached geo-location and timezone data to provide an initial time context.
void ntpInit() {
  Serial.println("NTP init");
  // Attempt to load cached data to set up a timezone as early as possible,
  // even before a fresh API sync.
  if (loadGeoCache()) {
    Serial.println("Initial time sync configured from cache.");
  }
}

// Updates NTP and geo-location data.
// This function is typically called upon a fresh WiFi connection or when a daily update is desired.
// It fetches geo-location and timezone information from an external API and then synchronizes time.
void ntpUpdateOnConnect() {
  Serial.println("NTP/Geo update on connect");
  
  // Fetch Geo + TZ
  HTTPClient http;
  http.begin("http://ip-api.com/json/?fields=status,lat,lon,timezone,offset");
  http.addHeader("User-Agent", "ESP32");
  int code = http.GET();
  
  if (code == HTTP_CODE_OK) {
    JsonDocument doc;
    
    // Parse directly from the response stream to save memory
    WiFiClient* stream = http.getStreamPtr();
    DeserializationError error = deserializeJson(doc, *stream);
    http.end();

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }
    
    // Check if the API returned a success status
    if (doc["status"] != "success") {
      Serial.println("API Status not success");
      return;
    }
    
    // Extract values safely
    // ArduinoJson handles the conversion from numbers to Strings for g_lat/g_lon
    g_lat = doc["lat"].as<String>();
    g_lon = doc["lon"].as<String>();
    g_timezone = doc["timezone"].as<String>();
    long offset = doc["offset"]; // Offset in seconds from UTC.
    
    // Set TZ with offset (seconds). 
    // Note: POSIX TZ strings use negative values for East (e.g., UTC-5:30 for GMT+5:30).
    char tzbuf[64];
    int hours = offset / 3600; // Convert offset to hours.
    int minutes = abs((int)(offset % 3600) / 60);
    // Formatting as UTC-H:M or UTC+H:M
    snprintf(tzbuf, sizeof(tzbuf), "UTC%+d:%02d", -hours, minutes);
    
    Serial.printf("TZ=%s (POSIX: %s) offset=%ld\n", g_timezone.c_str(), tzbuf, offset);
    
    // NTP sync - Apply the offset directly from the API.
    // This configures the system's local time offset, so `localtime_r` will return the correct local time.
    configTime(offset, 0, "pool.ntp.org");
    
    // Save the fetched geo-location and offset for future use (caching).
    saveGeoCache(offset);
    
    // Wait for the NTP client to synchronize time.
    // The loop retries for up to 5 seconds (10 retries * 500ms).
    int retry = 0;
    while (time(nullptr) < 1000000000L && retry < 10) {
      delay(500); // Wait for half a second.
      retry++;
    }
    
    g_epochTime = time(nullptr);
    
    struct tm timeinfo;
    // UTC
    gmtime_r(&g_epochTime, &timeinfo);
    char ubuf[64];
    strftime(ubuf, sizeof(ubuf), "%Y-%m-%d %H:%M:%S UTC", &timeinfo);
    g_utcTime = ubuf;
    
    // Local
    localtime_r(&g_epochTime, &timeinfo);
    char lbuf[64];
    strftime(lbuf, sizeof(lbuf), "%Y-%m-%d %H:%M:%S Local", &timeinfo);
    g_localTime = lbuf;
    
    Serial.printf("Success!\nLAT=%s LON=%s \nUTC=%s \nLocal=%s \nTZ=\"%s\"\n", g_lat.c_str(), g_lon.c_str(), g_utcTime.c_str(), g_localTime.c_str(), g_timezone.c_str());
    
    // Notify RTC Manager that sync is complete
    rtcSyncWithNTP();
  } else {
    Serial.printf("API fail %d\n", code);
    http.end();
  }
}
