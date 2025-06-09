Import("env")
import sys

def before_upload(source, target, env):
    # Only ask in esp32dev environment
    if env["PIOENV"] == "esp32dev":
        print("\n" + "="*60)
        print("WARNING: Do you want to update web files (uploadfs)?")
        print("This will DELETE all persistent data including:")
        print("  - Session files (dive logs)")
        print("  - DPV settings")
        print("  - Total uptime counter")
        print("  - All runtime-generated files")
        print("="*60)
        
        try:
            # Python 2/3 compatibility
            try:
                response = raw_input("Upload web files? (y/N): ").strip().lower()
            except NameError:
                response = input("Upload web files? (y/N): ").strip().lower()
            
            if response in ['y', 'yes']:
                print("Uploading web files...")
                env.Execute("pio run -t uploadfs -e esp32dev")
                print("Web files uploaded successfully!")
            else:
                print("Skipping web files upload. Persistent data preserved.")
                
        except KeyboardInterrupt:
            print("\nUpload cancelled by user.")
            sys.exit(1)
        except:
            print("Input error. Skipping web files upload.")

env.AddPreAction("upload", before_upload) 