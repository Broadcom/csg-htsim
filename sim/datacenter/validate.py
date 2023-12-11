# Sample Python script to read filenames from an input file and launch a process for each filename to count the number of lines in that file.

import subprocess
import sys
import os

def run_experiments(input_filename):
    # Read the filenames from the input file
    with open(input_filename, 'r') as file:
        inputlines = file.readlines()

        
    # Remove any whitespace or newlines from the filenames
    i = 0
    # Iterate over each filename and launch a process to count its lines
    while i < len(inputlines): 
        filename = str(inputlines[i]).rstrip();
        i  = i+1

        if (filename.startswith("#")):
            continue;
        elif (filename.startswith("!")):
            print ("Found parameters when not processing a file!",filename)
            continue;

        targetTailFCT = 0
        params = []
        targetFCT = {}

        #figure out parameters.
        while i<len(inputlines):
            #print(str(inputlines[i]))
            if (not str(inputlines[i]).startswith("!")):
                #print ("Stopping param parsing")
                break;

            p = str(inputlines[i])
            i = i + 1

            if ("Param" in p):
                params.append(p.split(" ",1)[1])
                #print ("Found param",p.split(" ",1)[1])
            elif ("tailFCT" in p):
                targetTailFCT = int(p.split(" ",1)[1])
                #print ("Found targetTailFCT",targetTailFCT)
            elif ("FCT" in p):
                q = p.split()
                targetFCT[q[1]] = int(q[2])

        if not os.path.isfile(filename) :
            print ("Cannot find experiment file ", filename, "- skipping to next experiment")
            continue

        cmdline = "./htsim_eqds -tm "+filename+" "
        for p in params:
            cmdline = cmdline + p.rstrip() + " "

        if (debug):
            print("Cmdline\n",cmdline,"\nTargetTailFCT",targetTailFCT,"\nTargetFCT",targetFCT)

        #finding out number of connections:
        conncountcmd = "grep Connections " + filename + "| awk '{print $2;}'"

        if (debug):
            print (conncountcmd)
        process = subprocess.Popen(conncountcmd,shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    
        # Get the output and errors from the process
        output, errors = process.communicate()

        connection_count = 0;

        if process.returncode == 0:
            connection_count = int(output.decode('utf-8'));
            if (debug):
                print("Connections in CM file:", connection_count)
        else:
            print("Error getting connection count for file '{filename}': {errors.decode()}")


        print ("Running",cmdline)

        process = subprocess.Popen(cmdline,shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        
        # Get the output and errors from the process
        output, errors = process.communicate()

        if process.returncode == 0:
            # Extract the line count from the output
            #line_count = output.decode().split()[0]
            #print(f"File '{filename}' has {line_count} lines.")
            lines = output.splitlines()

            fcttail = 0
            actual_connection_count = 0
            for x in lines:
                if "finished" in str(x):
                    a = x.decode('utf-8')
                    if (debug):
                        print (a)

                    items = a.split()

                    actual_connection_count = actual_connection_count + 1

                    flowname = a[1]
                    fct = float(items[8])
                    fcttail = fct

                    if items[1] in targetFCT:
                        if fct <= targetFCT[items[1]]:
                            print ("[PASS] FCT",fct,"us for flow ",items[1], "which is below the target of",targetFCT[items[1]],"us")
                        else:
                            print ("[FAIL] FCT",fct,"us for flow ",items[1], "which is higher than the target of",targetFCT[items[1]],"us")


            if (fcttail > targetTailFCT and targetTailFCT >0):
                print ("[FAIL] Tail FCT",fcttail, "us above the target of",targetTailFCT,"us")
            else:
                print ("[PASS] Tail FCT",fcttail, "us below the target of",targetTailFCT,"us")

            if (actual_connection_count != connection_count):
                print("[FAIL] Total connections in connection matrix was ",connection_count," but only ",actual_connection_count,"finished")
            else:
                print ("[PASS] Connection count",actual_connection_count)


        else:
            # Print any errors that occurred
            print("Error processing file ",filename,errors.decode())



debug = False

# total arguments
n = len(sys.argv)
i = 1;

while (i<n):
    if (sys.argv[i]=="-debug"):
        debug = True;
    else:
        print ("Unknown parameter",sys.argv[i])    
    i = i + 1

run_experiments('validate.txt')
