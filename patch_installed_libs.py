import os, sys, subprocess, shutil

def get_all_otool_entries(libpath):
    proc = subprocess.Popen(["/usr/bin/otool", "-L", libpath], stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    stdout, stderr = proc.communicate(timeout=5)
    lines = [x.strip('\t') for x in stdout.decode('utf-8').split('\n')]
    return [x.split(' (compatibility')[0] for x in lines[1:]]    

def patch_lib(abspath, build_dir, outdir):
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
        # we should first copy item to a local directory so we have access and can patch it.
        shutil.copy(item, outdir)
        # once we copied it we should be able to patch it..
        copy_fname = os.path.join(outdir, os.path.basename(item))
        print(f"> Copied file to {copy_fname}")
        
        subprocess.run(["install_name_tool", "-id", f"@rpath/{os.path.basename(item)}", copy_fname])

        def prefix_one_of(x):
            for bdir in ["/opt/local/lib/", build_dir]:                
                if os.path.commonprefix([x, bdir]) == bdir:
                    return True
            return False
        
        for x in [x for x in get_all_otool_entries(item) if os.path.abspath(x) and prefix_one_of(x)]:
            print(f"our passanger: {x}")
            subprocess.run(["install_name_tool", "-change", x, f"@rpath/{os.path.basename(x)}", copy_fname])
            if x not in processed_set:
                parse_queue.append(x)
            processed_set.add(x)
    print(f"< done: {item} ({copy_fname})")
                
if __name__ == "__main__":
    print("\n> Patching installed libraries with install_name_tool..")
    if len(sys.argv) != 4:
        print(f"missing args. Usage: {sys.argv[0]} <roo_lib_path> <build_dir> <patched_out_dir>")
        sys.exit(0)
    os.makedirs(sys.argv[3], exist_ok=True)
    patch_lib(sys.argv[1], sys.argv[2], sys.argv[3])

