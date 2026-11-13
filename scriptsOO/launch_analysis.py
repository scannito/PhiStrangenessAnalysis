import os

dir_spec = "test_gammas_"

for volF in range(-100002, -100001):
    os.system("mkdir out")

    for vol in range(30, 31): # 27 28
        os.system("mkdir /data/mciacco/outOO")

        for mult in range(0, 8):
            with open(f"cmds_parallel_{mult}", "w") as f:
                for sample in range(0, 40):
#                    f.write(f". launch_gen_parallel.sh {mult} {vol} {sample}\n") # {volF}\n")
                    f.write(f". launch_gen_parallel.sh {mult} {vol + .8} {sample}\n") # {volF}\n")

        os.system("/bin/bash launch_full_parallel.sh")
        os.system("/bin/bash launchPar.sh")
#        os.system(f"mv out out_{vol}_{volF}")
        #os.system(f"rm /data/mciacco/out_{dir_spec}{vol}_low_mult_gce/out_file* /data/mciacco/out_{dir_spec}{vol}_low_mult_gce/out__file*")
