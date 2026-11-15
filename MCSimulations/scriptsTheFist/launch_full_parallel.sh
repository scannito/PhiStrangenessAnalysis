cat cmds_parallel_0 | xargs -P 40 -I CMD bash -c CMD
cat cmds_parallel_1 | xargs -P 40 -I CMD bash -c CMD
cat cmds_parallel_2 | xargs -P 40 -I CMD bash -c CMD
cat cmds_parallel_3 | xargs -P 40 -I CMD bash -c CMD
cat cmds_parallel_4 | xargs -P 40 -I CMD bash -c CMD
cat cmds_parallel_5 | xargs -P 40 -I CMD bash -c CMD
cat cmds_parallel_6 | xargs -P 40 -I CMD bash -c CMD
cat cmds_parallel_7 | xargs -P 40 -I CMD bash -c CMD


#cat cmds_parallel_7 | xargs -P 50 -I CMD bash -c CMD
#cat cmds_parallel_8 | xargs -P 50 -I CMD bash -c CMD # for pp only
