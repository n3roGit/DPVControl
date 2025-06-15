#!/usr/bin/env python3
"""
Test script to check what the ESP32 is actually serving for app.js
This will help us debug if the file is being truncated during transmission.
"""

import requests
import sys

# ESP32 IP address - adjust as needed
ESP32_IP = "192.168.178.45"  # Update this to your ESP32's IP

def test_app_js():
    try:
        url = f"http://{ESP32_IP}/app.js"
        print(f"Fetching {url}...")
        
        response = requests.get(url, timeout=30)
        
        print(f"Status Code: {response.status_code}")
        print(f"Content-Type: {response.headers.get('Content-Type', 'Unknown')}")
        print(f"Content-Length: {response.headers.get('Content-Length', 'Unknown')}")
        print(f"Cache-Control: {response.headers.get('Cache-Control', 'Unknown')}")
        print(f"ETag: {response.headers.get('ETag', 'Unknown')}")
        
        content = response.text
        content_length = len(content)
        content_bytes = len(response.content)
        
        print(f"Actual content length (chars): {content_length}")
        print(f"Actual content length (bytes): {content_bytes}")
        
        # Check if file ends properly
        last_100_chars = content[-100:] if content else ""
        print(f"\nLast 100 characters of file:")
        print(repr(last_100_chars))
        
        # Check for showTab function
        if "function showTab" in content:
            print("\n✅ showTab function found")
            # Find the line with showTab
            lines = content.split('\n')
            for i, line in enumerate(lines, 1):
                if "function showTab" in line:
                    print(f"   showTab defined at line {i}")
                    break
        else:
            print("\n❌ showTab function NOT found!")
        
        # Check if file is complete by looking for the last function
        if "function toggleTooltipMode" in content:
            print("✅ Last function (toggleTooltipMode) found - file appears complete")
        else:
            print("❌ Last function missing - file is truncated!")
        
        # Check for syntax errors in the last part
        try:
            # Try to find where the file actually ends
            lines = content.split('\n')
            print(f"\nFile has {len(lines)} lines")
            print(f"Last 5 lines:")
            for i, line in enumerate(lines[-5:], len(lines)-4):
                print(f"  {i}: {repr(line)}")
                
        except Exception as e:
            print(f"Error analyzing content: {e}")
            
        return content_length, content_bytes
        
    except requests.exceptions.Timeout:
        print(f"❌ Timeout connecting to {ESP32_IP}")
        return None, None
    except requests.exceptions.ConnectionError:
        print(f"❌ Could not connect to {ESP32_IP}")
        print("   Make sure the ESP32 is connected and the IP is correct")
        return None, None
    except Exception as e:
        print(f"❌ Error: {e}")
        return None, None

if __name__ == "__main__":
    if len(sys.argv) > 1:
        ESP32_IP = sys.argv[1]
    
    print(f"Testing app.js from ESP32 at {ESP32_IP}")
    print("=" * 50)
    
    chars, bytes_len = test_app_js()
    
    if chars is not None:
        expected_size = 68906  # From our embed_webfiles.py output
        print(f"\nComparison:")
        print(f"Expected size: {expected_size} bytes")
        print(f"Actual size:   {bytes_len} bytes")
        
        if bytes_len == expected_size:
            print("✅ Size matches perfectly!")
        else:
            diff = expected_size - bytes_len
            print(f"❌ Size difference: {diff} bytes missing" if diff > 0 else f"❌ Size difference: {-diff} bytes extra") 