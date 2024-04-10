import os, sys, subprocess

def get_otool_entries(libpath, build_dir):
    proc = subprocess.Popen(["/usr/bin/otool", "-L", libpath], stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    stdout, stderr = proc.communicate(timeout=1)
    lines = [x.strip('\t') for x in stdout.decode('utf-8').split('\n')]
    # get only those entries that have absolute path of our build_directory
    return [x.split(' (compatibility')[0] for x in lines[1:] if os.path.isabs(x) and os.path.commonprefix([x, build_dir]) == build_dir]

def patch_lib(abspath, build_dir):
    processed_set = set()
    ext = os.path.splitext(abspath)[1]
    if ext != ".dylib":
        print(f"error: {abspath} is not a dynamic lib")
        return
    processed_set.add(abspath)
    parse_queue = [os.path.abspath(abspath)]
    while len(parse_queue) > 0:
        item = parse_queue.pop(0)
        if not os.path.isabs(item):
            print(f"warning: {item} is not an absolute path, skipping")
            continue
        print(f"> processing: {item}")
        # on the Internet someone sauid id can be left as is but lets patch it as well.
        subprocess.run(["install_name_tool", "-id", f"@rpath/{os.path.basename(item)}", item])
        for x in get_otool_entries(item, build_dir):
            subprocess.run(["install_name_tool", "-change", x, f"@rpath/{os.path.basename(x)}", item])
            if x not in processed_set:
                parse_queue.append(x)
            processed_set.add(x)
        print(f"< done: {item}")

if __name__ == "__main__":
    print("\n> Patching installed libraries with install_name_tool..")
    if len(sys.argv) != 3:
        print("missing args")
        sys.exit(0)
    patch_lib(sys.argv[1], sys.argv[2])
    
