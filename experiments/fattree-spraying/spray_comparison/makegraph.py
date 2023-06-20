for paths in [1,8,16,32,64,128,256]:
    ofile = open("data/fct_paths_" + str(paths) + ".dat", "w")
    for nodes in [432,1024,2000,3456,8192,16000]:
        fctsum = 0
        fctcount = 0
        for seed in range(13,17):
            try:
                filename = "data/finished_" + str(nodes) + "_" + str(seed) + "_" \
                            + str(paths) + ".txt"
                file = open(filename, "r")
                for line in file:
                    parts = line.split()
                    if len(parts) > 10:
                        fctsum += float(parts[9])
                        fctcount += 1
            except:
                print("couldn't open", filename)
        if fctcount > 0:
            print(nodes, paths, fctsum/fctcount, file=ofile)
    ofile.close()




                        
    
