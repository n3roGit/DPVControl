Import("env")
import sys
import time

def after_upload_prompt(source, target, env):
    # Only ask in esp32dev environment
    if env["PIOENV"] == "esp32dev":
        # Clear screen and add spacing for better visibility
        print("\n" * 2)
        print("🔧 FIRMWARE UPLOAD COMPLETED!")
        print("\n")
        print("="*70)
        print("║" + " " * 68 + "║")
        print("║" + " OPTIONAL: Web Files Upload Decision".center(68) + "║")
        print("║" + " " * 68 + "║")
        print("="*70)
        print("║ Do you want to ALSO update web files (uploadfs)?".ljust(69) + "║")
        print("║".ljust(70) + "║")
        print("║ ⚠️  WARNING: This will DELETE all persistent data:".ljust(69) + "║")
        print("║   - Session files (dive logs)".ljust(69) + "║")
        print("║   - DPV settings".ljust(69) + "║")
        print("║   - Total uptime counter".ljust(69) + "║")
        print("║   - All runtime-generated files".ljust(69) + "║")
        print("║".ljust(70) + "║")
        print("║ 💡 Only choose YES if you changed HTML/CSS/JS files".ljust(69) + "║")
        print("║".ljust(70) + "║")
        print("="*70)
        print()
        
        # Add a small delay to ensure output is visible
        time.sleep(0.5)
        
        try:
            # Force flush output
            sys.stdout.flush()
            
            # Python 2/3 compatibility with more explicit prompting
            try:
                response = raw_input(">>> Update web files? Type 'y' for YES or just press ENTER for NO (y/N): ").strip().lower()
            except NameError:
                response = input(">>> Update web files? Type 'y' for YES or just press ENTER for NO (y/N): ").strip().lower()
            
            print("\n" + "-"*50)
            
            if response in ['y', 'yes']:
                print("✓ YES selected - Uploading web files...")
                print("⚠️  WARNING: Deleting persistent data...")
                time.sleep(1)
                env.Execute("pio run -t uploadfs -e esp32dev")
                print("✓ Web files uploaded successfully!")
                print("🔄 ESP32 will restart automatically")
            else:
                print("✓ NO selected - Web files unchanged")
                print("✅ Persistent data preserved (sessions, settings, uptime)")
                print("🚀 ESP32 is ready with new firmware!")
                
            print("-"*50 + "\n")
                
        except KeyboardInterrupt:
            print("\n❌ Interrupted by user (Ctrl+C)")
            print("✅ Web files unchanged - Persistent data preserved")
            print("🚀 ESP32 is ready with new firmware!")
        except Exception as e:
            print(f"\n❌ Input error: {e}")
            print("✅ Defaulting to NO - Web files unchanged")
            print("✅ Persistent data preserved")
            print("🚀 ESP32 is ready with new firmware!")

# Use PostAction instead of PreAction to show prompt after upload
env.AddPostAction("upload", after_upload_prompt) 