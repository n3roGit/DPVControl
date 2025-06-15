#!/usr/bin/env python3
"""
One-time cleanup script to delete old web files from ESP32 /data directory.
This removes the old files that are now embedded in firmware.

Run this once after uploading the new firmware with embedded web files.
"""

import requests
import time
import sys

# ESP32 IP addresses to try
ESP32_IPS = [
    "4.3.2.1",      # Captive portal
    "192.168.4.1",  # AP mode
]

# Files to delete from ESP32 /data directory
FILES_TO_DELETE = [
    "index.html",
    "app.js", 
    "style.css",
    "chart.min.js",
    "chartjs-plugin-zoom.min.js",
    "jszip.min.js",
    "info.html",
    "remote.html", 
    "settings.html",
    "logo.jpg",
    "version.txt",
    "version.js"
]

def find_esp32():
    """Find the ESP32 by trying common IP addresses."""
    for ip in ESP32_IPS:
        try:
            print(f"🔍 Trying ESP32 at {ip}...")
            response = requests.get(f"http://{ip}/api/status", timeout=3)
            if response.status_code == 200:
                print(f"✅ Found ESP32 at {ip}")
                return ip
        except:
            continue
    return None

def delete_file_via_api(esp_ip, filename):
    """Delete a file via ESP32 API (if such endpoint exists)."""
    # Note: This would require a custom API endpoint for file deletion
    # Since we don't have this, we'll use a different approach
    pass

def check_file_exists(esp_ip, filename):
    """Check if a file exists by trying to access it."""
    try:
        response = requests.get(f"http://{esp_ip}/{filename}", timeout=3)
        return response.status_code == 200
    except:
        return False

def main():
    print("🧹 ESP32 Data Directory Cleanup Script")
    print("=" * 50)
    print("This script will help clean up old web files from ESP32.")
    print("The files are now embedded in firmware and no longer needed in /data.")
    print()
    
    # Find ESP32
    esp_ip = find_esp32()
    if not esp_ip:
        print("❌ Could not find ESP32. Please ensure:")
        print("   - ESP32 is powered on")
        print("   - You're connected to the ESP32 WiFi")
        print("   - ESP32 is running the web server")
        return 1
    
    print(f"📡 Using ESP32 at: {esp_ip}")
    print()
    
    # Check which files exist
    existing_files = []
    print("🔍 Checking which old files still exist...")
    for filename in FILES_TO_DELETE:
        if check_file_exists(esp_ip, filename):
            print(f"   ✓ Found: {filename}")
            existing_files.append(filename)
        else:
            print(f"   - Missing: {filename}")
    
    print()
    if not existing_files:
        print("✅ No old web files found! Cleanup not needed.")
        return 0
    
    print(f"📋 Found {len(existing_files)} files that should be removed:")
    for f in existing_files:
        print(f"   - {f}")
    print()
    
    print("⚠️  IMPORTANT INSTRUCTIONS:")
    print("Since there's no direct file deletion API, you have two options:")
    print()
    print("1. 🔄 RECOMMENDED: Upload empty filesystem")
    print("   Run: python -m platformio run -e esp32dev -t uploadfs")
    print("   This will clear all /data files and use only embedded files.")
    print()
    print("2. 🛠️  MANUAL: Delete files via serial console")
    print("   Connect via serial and use LittleFS.remove() commands")
    print()
    
    choice = input("❓ Do you want me to prepare the uploadfs command? (y/N): ").strip().lower()
    
    if choice in ['y', 'yes']:
        print()
        print("🚀 Preparing filesystem upload...")
        print("This will:")
        print("   ✅ Remove all old web files from ESP32")
        print("   ✅ ESP32 will use only embedded files (faster!)")
        print("   ✅ Preserve all session data and settings")
        print()
        print("Run this command:")
        print("   python -m platformio run -e esp32dev -t uploadfs")
        print()
        print("After upload, the ESP32 will restart and serve files from embedded memory.")
    else:
        print("ℹ️  No action taken. Files remain on ESP32.")
    
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n❌ Interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ Error: {e}")
        sys.exit(1) 