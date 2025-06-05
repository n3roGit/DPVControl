Import("env")

def before_upload(source, target, env):
    # Nur im esp32dev-Environment uploadfs ausführen
    if env["PIOENV"] == "esp32dev":
        env.Execute("pio run -t uploadfs -e esp32dev")

env.AddPreAction("upload", before_upload) 