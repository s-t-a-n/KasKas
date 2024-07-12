Import("env")

print("Current CLI targets", COMMAND_LINE_TARGETS)
print("Current Build targets", BUILD_TARGETS)

def post_program_action(source, target, env):
    print("Program has been built!")
    program_path = target[0].get_abspath()
    print("Program path", program_path)
    # Use case: sign a firmware, do any manipulations with ELF, etc
    # env.Execute(f"sign --elf {program_path}")

env.AddPostAction("$PROGPATH", post_program_action)

#
# Upload actions
#

def before_upload(source, target, env):
    print("before_upload")
    # do some actions

    # call Node.JS or other script
    env.Execute("node --version")
    
    f = open("/tmp/pio_before.txt", "a")
    f.write("yes!")
    f.close()


def after_upload(source, target, env):
    print("after_upload")
    f = open("/tmp/pio_after.txt", "a")
    f.write("yes!")
    # do some actions

env.AddPreAction("upload", before_upload)
env.AddPostAction("upload", after_upload)
