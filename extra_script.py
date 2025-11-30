Import("env")
import sys
import subprocess
import os

def pre_build_embed_webfiles(source, target, env):
    """Pre-build action to embed web files from upload/ directory."""
    # Only run for esp32dev environment
    if env["PIOENV"] == "esp32dev":
        print("\n🔧 PRE-BUILD: Embedding Web Files")
        print("="*50)
        
        try:
            # Get the project directory
            project_dir = env.get("PROJECT_DIR")
            embed_script = os.path.join(project_dir, "embed_webfiles.py")
            
            # Check if embed script exists
            if not os.path.exists(embed_script):
                print(f"❌ Embed script not found: {embed_script}")
                return
            
            # Set up environment for UTF-8 with fallbacks
            embed_env = os.environ.copy()
            embed_env["PYTHONIOENCODING"] = "utf-8:replace"
            # Try C.UTF-8 first, then fallback to en_US.UTF-8
            embed_env["LC_ALL"] = "C.UTF-8"
            embed_env["LANG"] = "C.UTF-8"
            
            # Run the embed script with explicit UTF-8 encoding and environment
            # Use unbuffered output for better visibility
            embed_env["PYTHONUNBUFFERED"] = "1"
            result = subprocess.run([sys.executable, "-u", embed_script], 
                                  capture_output=True, text=True, cwd=project_dir,
                                  encoding='utf-8', errors='replace', env=embed_env)
            
            # Always print output, even if successful
            if result.stdout:
                print(result.stdout)
            if result.stderr:
                print(result.stderr)
            
            if result.returncode == 0:
                print("✅ Web files embedded successfully!")
            else:
                print(f"❌ Error embedding web files (exit code: {result.returncode})")
                print("⚠️  Build will continue but web files may not be available")
                
        except Exception as e:
            print(f"❌ Exception during web file embedding: {e}")
            print("⚠️  Build will continue but web files may not be available")
        
        print("="*50)

# Run pre-build embedding for esp32dev environment
env.AddPreAction("buildprog", pre_build_embed_webfiles) 