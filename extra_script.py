Import("env")
def before_upload(source, target, env):
    env.Execute("pio run -t uploadfs")
env.AddPreAction("upload", before_upload) 